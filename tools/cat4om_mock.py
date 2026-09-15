#!/usr/bin/env python3
"""Asynchronous CAT4OM 1.0.0 simulator for Decodium.

The simulator exposes the same two WebSocket endpoints used by CAT4OM:

* Management endpoint: ws://127.0.0.1:5000/
* Group control endpoint: ws://127.0.0.1:5001/

It never opens a serial port and never controls a physical radio.  It models a
single IC-7300-like radio, management discovery, master/slave ownership,
push-based state updates, frequency/mode/VFO/split commands and safe PTT state.

Run it with:

    python3 tools/cat4om_mock.py

Then select Cat4OM in Decodium and use 127.0.0.1:5000 as the Management
endpoint.  Type ``help`` in this terminal for commands that change the fake
radio state while Decodium is connected.

Requires the ``websockets`` Python package:

    python3 -m pip install websockets
"""

from __future__ import annotations

import argparse
import asyncio
import json
import shlex
import signal
import sys
import uuid
from dataclasses import dataclass
from datetime import datetime, timezone
from typing import Any, Dict, Optional, Set

try:
    from websockets.asyncio.server import serve
    from websockets.exceptions import ConnectionClosed
except ModuleNotFoundError:
    print(
        "Missing Python package 'websockets'. Install it with:\n"
        "  python3 -m pip install websockets",
        file=sys.stderr,
    )
    raise SystemExit(2)


PROTOCOL_VERSION = "1.0.0"
WRITE_ACTIONS = {
    "setFrequency",
    "setMode",
    "setPtt",
    "setVfo",
    "setSplit",
}


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace("+00:00", "Z")


def enabled_value(text: str) -> bool:
    normalized = text.strip().lower()
    if normalized in {"1", "on", "true", "yes", "tx"}:
        return True
    if normalized in {"0", "off", "false", "no", "rx"}:
        return False
    raise ValueError("expected on/off")


@dataclass
class ClientSession:
    client_id: str
    app_name: str
    app_version: str


@dataclass
class FakeRadio:
    radio_id: str
    group_id: str
    radio_name: str
    frequency_a: int
    frequency_b: int
    mode_a: str
    mode_b: str
    active_vfo: str = "A"
    tx_vfo: str = "A"
    split: bool = False
    ptt: bool = False

    def state(self) -> Dict[str, Any]:
        tx_power = 10.0 if self.ptt else 0.0
        alc = 18.0 if self.ptt else 0.0
        return {
            "radioId": self.radio_id,
            "groupId": self.group_id,
            "radioName": self.radio_name,
            "handbookId": "icom/ic7300",
            "manufacturer": "Icom",
            "model": "IC-7300",
            "timestamp": utc_now(),
            "connectionStatus": "connected",
            "lastSeen": utc_now(),
            "availableVfos": ["A", "B"],
            "vfos": {
                "A": {
                    "vfo": "A",
                    "frequency": self.frequency_a,
                    "mode": self.mode_a,
                    "filterWidth": 3000,
                },
                "B": {
                    "vfo": "B",
                    "frequency": self.frequency_b,
                    "mode": self.mode_b,
                    "filterWidth": 3000,
                },
            },
            "activeVfo": self.active_vfo,
            "txVfo": self.tx_vfo,
            "rxVfos": [self.active_vfo],
            "split": self.split,
            "rit": {"enabled": False, "offset": 0},
            "xit": {"enabled": False, "offset": 0},
            "ptt": self.ptt,
            "tuner": "bypass",
            "metering": {
                "sMeter": 5,
                "signalDbm": -93.0,
                "swr": 1.15,
                "power": tx_power,
                "alc": alc,
            },
            "supportedModes": [
                "LSB",
                "USB",
                "CW",
                "CW-R",
                "AM",
                "FM",
                "RTTY",
                "RTTY-R",
                "DATA-USB",
                "DATA-LSB",
                "FT8",
                "FT4",
            ],
            "availableCommands": [
                "GetFrequencyA",
                "GetFrequencyB",
                "GetModeA",
                "GetModeB",
                "SetFrequency",
                "SetMode",
                "SetVfo",
                "SetPtt",
                "SetSplit",
            ],
            "commandVfoScopes": {},
            "frequencyResolution": 1,
            "frequencyGroups": ["hf"],
        }

    def frequency(self, vfo: Optional[str] = None) -> int:
        selected = (vfo or self.active_vfo).upper()
        return self.frequency_b if selected == "B" else self.frequency_a

    def mode(self, vfo: Optional[str] = None) -> str:
        selected = (vfo or self.active_vfo).upper()
        return self.mode_b if selected == "B" else self.mode_a

    def set_frequency(self, value: int, vfo: Optional[str]) -> None:
        selected = (vfo or self.active_vfo).upper()
        if selected == "A":
            self.frequency_a = value
        elif selected == "B":
            self.frequency_b = value
        else:
            raise ValueError("unknown VFO")

    def set_mode(self, value: str, vfo: Optional[str] = None) -> None:
        selected = (vfo or self.active_vfo).upper()
        if selected == "A":
            self.mode_a = value.upper()
        elif selected == "B":
            self.mode_b = value.upper()
        else:
            raise ValueError("unknown VFO")


class Cat4OmMock:
    def __init__(self, args: argparse.Namespace) -> None:
        self.args = args
        self.group_running = True
        self.radio = FakeRadio(
            radio_id=args.radio_id,
            group_id=args.group_id,
            radio_name=args.radio_name,
            frequency_a=args.frequency,
            frequency_b=args.frequency_b,
            mode_a=args.mode.upper(),
            mode_b=args.mode_b.upper(),
        )
        self.management_clients: Dict[Any, ClientSession] = {}
        self.management_subscribers: Set[Any] = set()
        self.control_clients: Dict[Any, ClientSession] = {}
        self.master_id: Optional[str] = None
        self.stop_event = asyncio.Event()
        self.tx_watchdog_task: Optional[asyncio.Task[Any]] = None
        self.keepalive_task: Optional[asyncio.Task[Any]] = None

    def log(self, message: str) -> None:
        time_text = datetime.now(timezone.utc).strftime("%H:%M:%S.%f")[:-3] + "Z"
        print(f"[{time_text}] {message}", flush=True)

    def endpoint_host(self) -> str:
        return "127.0.0.1" if self.args.host in {"0.0.0.0", "::"} else self.args.host

    async def send(self, websocket: Any, message: Dict[str, Any], label: str = "") -> None:
        try:
            payload = json.dumps(message, separators=(",", ":"))
            await websocket.send(payload)
            if self.args.verbose_json:
                self.log(f"[TX {label}] {payload}")
        except ConnectionClosed:
            return

    async def response(
        self,
        websocket: Any,
        request: Dict[str, Any],
        result: Any = None,
    ) -> None:
        message: Dict[str, Any] = {
            "type": "response",
            "id": request.get("id"),
            "success": True,
            "result": result,
        }
        if request.get("radioId"):
            message["radioId"] = request["radioId"]
        await self.send(websocket, message, "response")

    async def error_response(
        self,
        websocket: Any,
        request: Dict[str, Any],
        code: str,
        message: str,
    ) -> None:
        await self.send(
            websocket,
            {
                "type": "response",
                "id": request.get("id"),
                "success": False,
                "error": {"code": code, "message": message},
            },
            "error",
        )

    async def receive_hello(self, websocket: Any, endpoint: str) -> Optional[ClientSession]:
        try:
            raw = await asyncio.wait_for(websocket.recv(), timeout=10.0)
            message = json.loads(raw)
        except (asyncio.TimeoutError, json.JSONDecodeError, ConnectionClosed):
            await websocket.close(code=1002, reason="CAT4OM hello required")
            return None

        if message.get("type") != "hello" or message.get("protocolVersion") != PROTOCOL_VERSION:
            await self.send(
                websocket,
                {
                    "type": "error",
                    "error": {
                        "code": "PROTOCOL_MISMATCH",
                        "message": f"CAT4OM protocol {PROTOCOL_VERSION} is required",
                    },
                },
                endpoint,
            )
            await websocket.close(code=1002, reason="Protocol mismatch")
            return None

        session = ClientSession(
            client_id=str(uuid.uuid4()),
            app_name=str(message.get("appName") or "unknown"),
            app_version=str(message.get("appVersion") or "unknown"),
        )
        self.log(
            f"[{endpoint}] hello from {session.app_name} {session.app_version} "
            f"({session.client_id})"
        )
        return session

    def service_status(self) -> Dict[str, Any]:
        return {
            "serviceVersion": "0.1.0.0-simulator",
            "timestamp": utc_now(),
            "groups": [
                {
                    "id": self.args.group_id,
                    "name": self.args.group_name,
                    "isRunning": self.group_running,
                    "controlPort": self.args.control_port,
                    "radios": [
                        {
                            "radioId": self.radio.radio_id,
                            "radioName": self.radio.radio_name,
                            "connectionStatus": "connected" if self.group_running else "stopped",
                        }
                    ],
                }
            ],
        }

    async def broadcast_service_status(self) -> None:
        message = {"type": "serviceStatus", "status": self.service_status()}
        for websocket in list(self.management_subscribers):
            await self.send(websocket, message, "management push")

    async def management_handler(self, websocket: Any) -> None:
        session = await self.receive_hello(websocket, "management")
        if session is None:
            return
        self.management_clients[websocket] = session
        await self.send(
            websocket,
            {
                "type": "managementWelcome",
                "endpoint": "management",
                "protocolVersion": PROTOCOL_VERSION,
                "clientId": session.client_id,
                "serviceVersion": "0.1.0.0-simulator",
                "authRequired": False,
            },
            "management welcome",
        )
        try:
            async for raw in websocket:
                await self.handle_management_message(websocket, session, raw)
        except ConnectionClosed:
            pass
        finally:
            self.management_clients.pop(websocket, None)
            self.management_subscribers.discard(websocket)
            self.log(f"[management] client disconnected ({session.client_id})")

    async def handle_management_message(
        self, websocket: Any, session: ClientSession, raw: str
    ) -> None:
        try:
            request = json.loads(raw)
        except json.JSONDecodeError:
            return
        if self.args.verbose_json:
            self.log(f"[RX management] {raw}")
        if request.get("type") != "request":
            return
        if request.get("clientId") != session.client_id:
            await self.error_response(websocket, request, "CLIENT_MISMATCH", "Invalid clientId")
            return

        action = request.get("action")
        params = request.get("params") or {}
        self.log(f"[management] action={action}")
        if action == "getServiceStatus":
            await self.response(websocket, request, self.service_status())
        elif action == "subscribe":
            self.management_subscribers.add(websocket)
            await self.response(
                websocket,
                request,
                {"subscribed": True, "status": self.service_status()},
            )
        elif action == "unsubscribe":
            self.management_subscribers.discard(websocket)
            await self.response(websocket, request, {"subscribed": False})
        elif action == "getGroupStatus":
            await self.response(websocket, request, self.service_status()["groups"][0])
        elif action in {"startGroup", "stopGroup"}:
            requested_group = params.get("groupId")
            if requested_group and requested_group != self.args.group_id:
                await self.error_response(websocket, request, "GROUP_NOT_FOUND", "Unknown group")
                return
            self.group_running = action == "startGroup"
            await self.response(
                websocket,
                request,
                {"groupId": self.args.group_id, "isRunning": self.group_running},
            )
            await self.broadcast_service_status()
        else:
            await self.error_response(
                websocket,
                request,
                "UNKNOWN_ACTION",
                f"Unsupported management action '{action}'",
            )

    async def control_handler(self, websocket: Any) -> None:
        session = await self.receive_hello(websocket, "control")
        if session is None:
            return
        if not self.group_running:
            await self.send(
                websocket,
                {
                    "type": "error",
                    "error": {"code": "GROUP_STOPPED", "message": "Radio group is stopped"},
                },
                "control",
            )
            await websocket.close(code=1013, reason="Group stopped")
            return

        self.control_clients[websocket] = session
        if self.master_id is None:
            self.master_id = session.client_id
        role = "master" if self.master_id == session.client_id else "slave"
        await self.send(
            websocket,
            {
                "type": "welcome",
                "endpoint": "control",
                "protocolVersion": PROTOCOL_VERSION,
                "groupId": self.args.group_id,
                "clientId": session.client_id,
                "role": role,
                "sessionToken": None,
                "radios": [self.radio.state()],
                "authRequired": False,
            },
            "control welcome",
        )
        self.log(f"[control] {session.app_name} connected as {role}")
        try:
            async for raw in websocket:
                await self.handle_control_message(websocket, session, raw)
        except ConnectionClosed:
            pass
        finally:
            was_master = session.client_id == self.master_id
            self.control_clients.pop(websocket, None)
            if was_master:
                if self.radio.ptt:
                    self.radio.ptt = False
                    self.cancel_tx_watchdog()
                    self.log("[safety] master disconnected: fake radio forced to RX")
                next_session = next(iter(self.control_clients.values()), None)
                self.master_id = next_session.client_id if next_session else None
                await self.broadcast_ownership()
                await self.broadcast_state()
            self.log(f"[control] client disconnected ({session.client_id})")

    async def handle_control_message(
        self, websocket: Any, session: ClientSession, raw: str
    ) -> None:
        try:
            request = json.loads(raw)
        except json.JSONDecodeError:
            return
        if self.args.verbose_json:
            self.log(f"[RX control] {raw}")
        if request.get("type") != "request":
            return
        if request.get("clientId") != session.client_id:
            await self.error_response(websocket, request, "CLIENT_MISMATCH", "Invalid clientId")
            return

        action = request.get("action")
        params = request.get("params") or {}
        radio_id = request.get("radioId")
        self.log(f"[control] action={action} radio={radio_id or '-'} client={session.client_id}")

        if action == "getOwnership":
            if self.radio.ptt:
                await self.error_response(
                    websocket,
                    request,
                    "TX_IN_PROGRESS",
                    "Ownership is frozen while the fake radio is transmitting",
                )
                return
            self.master_id = session.client_id
            await self.response(websocket, request, {"role": "master"})
            await self.broadcast_ownership()
            await self.broadcast_state()
            return

        if action == "getAllState":
            await self.response(websocket, request, {"radios": [self.radio.state()]})
            return
        if action == "getState":
            if radio_id != self.radio.radio_id:
                await self.error_response(websocket, request, "RADIO_NOT_FOUND", "Unknown radio")
            else:
                await self.response(websocket, request, self.radio.state())
            return

        if action in WRITE_ACTIONS and session.client_id != self.master_id:
            await self.error_response(
                websocket,
                request,
                "NOT_MASTER",
                "Only master client can send commands",
            )
            return
        if action in WRITE_ACTIONS and radio_id != self.radio.radio_id:
            await self.error_response(websocket, request, "RADIO_NOT_FOUND", "Unknown radio")
            return
        if self.radio.ptt and not (action == "setPtt" and params.get("enabled") is False):
            await self.error_response(
                websocket,
                request,
                "TX_IN_PROGRESS",
                "The fake radio is transmitting; only PTT-off is accepted",
            )
            return

        try:
            if action == "setFrequency":
                frequency = int(params["frequency"])
                if not 100_000 <= frequency <= 60_000_000:
                    raise ValueError("frequency must be between 100 kHz and 60 MHz")
                self.radio.set_frequency(frequency, params.get("vfo"))
            elif action == "setMode":
                mode = str(params["mode"]).strip().upper()
                if not mode:
                    raise ValueError("mode is empty")
                self.radio.set_mode(mode)
            elif action == "setPtt":
                enabled = params.get("enabled")
                if not isinstance(enabled, bool):
                    self.radio.ptt = False
                    self.cancel_tx_watchdog()
                    await self.response(websocket, request, {"status": "FAILSAFE_UNKEY"})
                    await self.broadcast_state()
                    return
                self.radio.ptt = enabled
                if enabled:
                    self.start_tx_watchdog()
                else:
                    self.cancel_tx_watchdog()
            elif action == "setVfo":
                vfo = str(params["vfo"]).upper()
                if vfo not in {"A", "B"}:
                    raise ValueError("VFO must be A or B")
                self.radio.active_vfo = vfo
            elif action == "setSplit":
                enabled = params.get("enabled")
                if not isinstance(enabled, bool):
                    raise ValueError("enabled must be boolean")
                self.radio.split = enabled
                tx_vfo = params.get("txVfo")
                if tx_vfo is not None:
                    tx_vfo = str(tx_vfo).upper()
                    if tx_vfo not in {"A", "B"}:
                        raise ValueError("TX VFO must be A or B")
                    self.radio.tx_vfo = tx_vfo
            else:
                await self.error_response(
                    websocket,
                    request,
                    "UNKNOWN_ACTION",
                    f"Unsupported control action '{action}'",
                )
                return
        except (KeyError, TypeError, ValueError) as error:
            await self.error_response(websocket, request, "INVALID_PARAM", str(error))
            return

        await self.response(websocket, request, None)
        await self.broadcast_state()
        self.log(f"[radio] {self.state_summary()}")

    def state_summary(self) -> str:
        return (
            f"VFO {self.radio.active_vfo} "
            f"{self.radio.frequency()} Hz {self.radio.mode()} | "
            f"split={'on' if self.radio.split else 'off'} "
            f"PTT={'TX' if self.radio.ptt else 'RX'}"
        )

    async def broadcast_state(self) -> None:
        message = {
            "type": "stateUpdate",
            "timestamp": utc_now(),
            "masterId": self.master_id,
            "radios": [self.radio.state()],
        }
        for websocket in list(self.control_clients):
            await self.send(websocket, message, "state push")

    async def broadcast_ownership(self) -> None:
        message = {
            "type": "event",
            "event": "ownershipChanged",
            "timestamp": utc_now(),
            "radioId": None,
            "details": {"masterId": self.master_id},
        }
        for websocket in list(self.control_clients):
            await self.send(websocket, message, "ownership push")
        self.log(f"[ownership] master={self.master_id or 'none'}")

    def start_tx_watchdog(self) -> None:
        self.cancel_tx_watchdog()
        self.tx_watchdog_task = asyncio.create_task(
            self.tx_watchdog(), name="cat4om-mock-tx-watchdog"
        )

    def cancel_tx_watchdog(self) -> None:
        if self.tx_watchdog_task is not None:
            self.tx_watchdog_task.cancel()
            self.tx_watchdog_task = None

    async def tx_watchdog(self) -> None:
        try:
            await asyncio.sleep(self.args.tx_timeout)
            if self.radio.ptt:
                self.radio.ptt = False
                self.log(f"[safety] {self.args.tx_timeout}s TX watchdog: forced fake radio to RX")
                await self.broadcast_state()
        except asyncio.CancelledError:
            return
        finally:
            self.tx_watchdog_task = None

    async def keepalive_loop(self) -> None:
        try:
            while not self.stop_event.is_set():
                await asyncio.sleep(self.args.keepalive)
                if self.control_clients:
                    await self.broadcast_state()
        except asyncio.CancelledError:
            return

    async def console_loop(self) -> None:
        if not sys.stdin.isatty() or self.args.no_console:
            return
        loop = asyncio.get_running_loop()
        lines: asyncio.Queue[str] = asyncio.Queue()

        def stdin_ready() -> None:
            line = sys.stdin.readline()
            lines.put_nowait(line)

        try:
            loop.add_reader(sys.stdin.fileno(), stdin_ready)
        except (AttributeError, NotImplementedError, OSError):
            self.log("[console] asynchronous terminal input is unavailable; console disabled")
            return
        self.print_console_help()
        print("cat4om> ", end="", flush=True)
        try:
            while not self.stop_event.is_set():
                line = await lines.get()
                if not line:
                    self.stop_event.set()
                    return
                try:
                    words = shlex.split(line)
                except ValueError as error:
                    self.log(f"[console] {error}")
                    print("cat4om> ", end="", flush=True)
                    continue
                if not words:
                    print("cat4om> ", end="", flush=True)
                    continue
                command = words[0].lower()
                try:
                    changed = self.apply_console_command(command, words[1:])
                except (IndexError, ValueError) as error:
                    self.log(f"[console] {error}")
                    print("cat4om> ", end="", flush=True)
                    continue
                if command in {"quit", "exit"}:
                    self.stop_event.set()
                    return
                if command == "help":
                    self.print_console_help()
                elif command == "status":
                    self.log(f"[radio] {self.state_summary()}")
                elif changed:
                    await self.broadcast_state()
                    self.log(f"[radio] {self.state_summary()}")
                print("cat4om> ", end="", flush=True)
        finally:
            loop.remove_reader(sys.stdin.fileno())

    def apply_console_command(self, command: str, args: list[str]) -> bool:
        if command == "freq":
            frequency = int(args[0])
            if not 100_000 <= frequency <= 60_000_000:
                raise ValueError("frequency must be between 100 kHz and 60 MHz")
            vfo = args[1].upper() if len(args) > 1 else self.radio.active_vfo
            self.radio.set_frequency(frequency, vfo)
            return True
        if command == "mode":
            self.radio.set_mode(args[0])
            return True
        if command == "vfo":
            vfo = args[0].upper()
            if vfo not in {"A", "B"}:
                raise ValueError("VFO must be A or B")
            self.radio.active_vfo = vfo
            return True
        if command == "split":
            self.radio.split = enabled_value(args[0])
            return True
        if command == "ptt":
            self.radio.ptt = enabled_value(args[0])
            if self.radio.ptt:
                self.start_tx_watchdog()
            else:
                self.cancel_tx_watchdog()
            return True
        if command in {"help", "status", "quit", "exit"}:
            return False
        raise ValueError(f"unknown command '{command}'; type help")

    @staticmethod
    def print_console_help() -> None:
        print(
            "\nInteractive fake-radio commands:\n"
            "  status                 show current state\n"
            "  freq 7074000 [A|B]     change frequency and push it to Decodium\n"
            "  mode DATA-USB          change mode on the active VFO\n"
            "  vfo A|B                select active VFO\n"
            "  split on|off           toggle split\n"
            "  ptt on|off             simulate TX/RX state only (no hardware)\n"
            "  help                    show this help\n"
            "  quit                    stop the simulator\n",
            flush=True,
        )

    async def run(self) -> None:
        display_host = self.endpoint_host()
        try:
            async with serve(
                self.management_handler,
                self.args.host,
                self.args.management_port,
                max_size=1_048_576,
                ping_interval=20,
                ping_timeout=20,
            ), serve(
                self.control_handler,
                self.args.host,
                self.args.control_port,
                max_size=1_048_576,
                ping_interval=20,
                ping_timeout=20,
            ):
                self.log(
                    f"CAT4OM management listening on ws://{display_host}:"
                    f"{self.args.management_port}/"
                )
                self.log(
                    f"CAT4OM control listening on ws://{display_host}:"
                    f"{self.args.control_port}/"
                )
                self.log(
                    f"Fake radio {self.radio.radio_name} id={self.radio.radio_id}; "
                    f"{self.state_summary()}"
                )
                print(
                    "\nDecodium settings:\n"
                    "  Backend: Cat4OM\n"
                    f"  Management endpoint: {display_host}:{self.args.management_port}\n"
                    "  Request control automatically: enabled\n",
                    flush=True,
                )
                self.keepalive_task = asyncio.create_task(
                    self.keepalive_loop(), name="cat4om-mock-keepalive"
                )
                console_task = asyncio.create_task(
                    self.console_loop(), name="cat4om-mock-console"
                )
                await self.stop_event.wait()
                console_task.cancel()
                self.keepalive_task.cancel()
                self.cancel_tx_watchdog()
                await asyncio.gather(console_task, self.keepalive_task, return_exceptions=True)
        except OSError as error:
            raise RuntimeError(
                f"Cannot bind CAT4OM simulator ports {self.args.management_port}/"
                f"{self.args.control_port}: {error}"
            ) from error


def port_number(value: str) -> int:
    number = int(value)
    if not 1 <= number <= 65535:
        raise argparse.ArgumentTypeError("port must be between 1 and 65535")
    return number


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="127.0.0.1", help="local bind address")
    parser.add_argument("--management-port", type=port_number, default=5000)
    parser.add_argument("--control-port", type=port_number, default=5001)
    parser.add_argument("--group-id", default="station")
    parser.add_argument("--group-name", default="Decodium Test Station")
    parser.add_argument("--radio-id", default="ic7300")
    parser.add_argument("--radio-name", default="Icom IC-7300 (simulated)")
    parser.add_argument("--frequency", type=int, default=14_074_000)
    parser.add_argument("--frequency-b", type=int, default=7_074_000)
    parser.add_argument("--mode", default="DATA-USB")
    parser.add_argument("--mode-b", default="DATA-USB")
    parser.add_argument(
        "--keepalive",
        type=float,
        default=10.0,
        help="state push interval in seconds",
    )
    parser.add_argument(
        "--tx-timeout",
        type=float,
        default=120.0,
        help="fake PTT safety timeout in seconds (maximum 180)",
    )
    parser.add_argument("--no-console", action="store_true", help="disable terminal commands")
    parser.add_argument("--verbose-json", action="store_true", help="log complete JSON traffic")
    args = parser.parse_args()
    if args.keepalive <= 0:
        parser.error("--keepalive must be greater than zero")
    if not 1 <= args.tx_timeout <= 180:
        parser.error("--tx-timeout must be between 1 and 180 seconds")
    if args.management_port == args.control_port:
        parser.error("management and control ports must be different")
    return args


async def async_main() -> int:
    args = parse_args()
    mock = Cat4OmMock(args)
    loop = asyncio.get_running_loop()
    for signum in (signal.SIGINT, signal.SIGTERM):
        try:
            loop.add_signal_handler(signum, mock.stop_event.set)
        except (NotImplementedError, RuntimeError):
            pass
    try:
        await mock.run()
    except RuntimeError as error:
        print(f"cat4om_mock: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(asyncio.run(async_main()))
