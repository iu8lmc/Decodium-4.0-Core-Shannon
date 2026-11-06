#!/usr/bin/env python3
"""Async local rotator mock for Decodium.

The mock deliberately implements only the command/feedback surface used by
Decodium's RotatorService.  It is useful for exercising the satellite window
without a physical rotator or a real rotator application.

Examples:

    python3 tools/rotator_mock.py --protocol pstrotator
    python3 tools/rotator_mock.py --protocol catrotator
    python3 tools/rotator_mock.py --protocol hamlib

PSTRotator and CatRotator both default to UDP 12000.  PSTRotator feedback is
sent to UDP 12001.  Hamlib rotctld defaults to TCP 4533 and returns feedback
on the same connection.
"""

from __future__ import annotations

import argparse
import asyncio
import re
import signal
from dataclasses import dataclass
from datetime import datetime, timezone
from typing import Optional


TAG_FLOAT = re.compile(
    r"<(?P<tag>[A-Z]+)>\s*(?P<value>[+-]?\d+(?:[.,]\d+)?)\s*</(?P=tag)>",
    re.IGNORECASE,
)


def now_text() -> str:
    return datetime.now(timezone.utc).strftime("%H:%M:%S.%f")[:-3] + "Z"


def normalize_azimuth(value: float) -> float:
    return value % 360.0


def number(value: float) -> str:
    return f"{value:.1f}"


@dataclass
class RotatorState:
    azimuth: float = 0.0
    elevation: float = 0.0
    moving: bool = False

    def set_target(self, azimuth: float, elevation: float) -> None:
        self.azimuth = normalize_azimuth(azimuth)
        self.elevation = max(-10.0, min(180.0, elevation))
        self.moving = True

    def stop(self) -> None:
        self.moving = False

    def park(self) -> None:
        self.azimuth = 0.0
        self.elevation = 0.0
        self.moving = False

    def summary(self) -> str:
        return f"AZ {number(self.azimuth)}° / EL {number(self.elevation)}°"


class RotatorMock:
    def __init__(self, args: argparse.Namespace) -> None:
        self.protocol = args.protocol
        self.host = args.host
        self.port = args.port
        self.feedback_port = args.feedback_port
        self.state = RotatorState(args.azimuth, args.elevation, False)
        self.transport: Optional[asyncio.DatagramTransport] = None

    def log(self, message: str) -> None:
        print(f"[{now_text()}] {message}", flush=True)

    def feedback_text(self) -> bytes:
        return f"AZ: {number(self.state.azimuth)}\r\nEL: {number(self.state.elevation)}\r\n".encode()

    def send_pst_feedback(self, address: tuple[str, int]) -> None:
        if self.transport is None:
            return
        feedback_address = (address[0], self.feedback_port)
        self.transport.sendto(self.feedback_text(), feedback_address)
        self.log(f"[TX feedback] {feedback_address[0]}:{feedback_address[1]} {self.state.summary()}")

    @staticmethod
    def tag_value(payload: str, tag_name: str) -> Optional[float]:
        for match in TAG_FLOAT.finditer(payload):
            if match.group("tag").lower() == tag_name.lower():
                return float(match.group("value").replace(",", "."))
        return None

    def handle_udp(self, payload: bytes, address: tuple[str, int]) -> None:
        text = payload.decode("utf-8", errors="replace").strip()
        self.log(f"[RX UDP {address[0]}:{address[1]}] {text!r}")

        azimuth = self.tag_value(text, "AZIMUTH")
        elevation = self.tag_value(text, "ELEVATION")
        if azimuth is not None or elevation is not None:
            self.state.set_target(
                self.state.azimuth if azimuth is None else azimuth,
                self.state.elevation if elevation is None else elevation,
            )
            self.log(f"[STATE] target accepted: {self.state.summary()}")
            if self.protocol == "pstrotator":
                self.send_pst_feedback(address)
            return

        if re.search(r"<STOP>\s*1\s*</STOP>", text, re.IGNORECASE):
            self.state.stop()
            self.log(f"[STATE] stop accepted: {self.state.summary()}")
            if self.protocol == "pstrotator":
                self.send_pst_feedback(address)
            return

        if re.search(r"<PARK>\s*1\s*</PARK>", text, re.IGNORECASE):
            self.state.park()
            self.log(f"[STATE] park accepted: {self.state.summary()}")
            if self.protocol == "pstrotator":
                self.send_pst_feedback(address)
            return

        if self.protocol == "pstrotator" and re.search(r"\bAZ\?", text, re.IGNORECASE):
            self.send_pst_feedback(address)
            return

        if self.protocol == "pstrotator" and re.search(r"\bEL\?", text, re.IGNORECASE):
            self.send_pst_feedback(address)
            return

        self.log("[INFO] UDP command received but not recognized")

    async def handle_hamlib_client(
        self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter
    ) -> None:
        peer = writer.get_extra_info("peername")
        peer_text = f"{peer[0]}:{peer[1]}" if peer else "unknown"
        self.log(f"[TCP] Hamlib client connected: {peer_text}")
        try:
            while True:
                line = await reader.readline()
                if not line:
                    break
                command = line.decode("ascii", errors="replace").strip()
                if not command:
                    continue
                self.log(f"[RX TCP {peer_text}] {command!r}")
                response = self.handle_hamlib_command(command)
                writer.write(response)
                await writer.drain()
                self.log(f"[TX TCP] {response.decode().strip()!r}")
        except (ConnectionError, asyncio.IncompleteReadError):
            pass
        finally:
            writer.close()
            try:
                await writer.wait_closed()
            except ConnectionError:
                pass
            self.log(f"[TCP] Hamlib client disconnected: {peer_text}")

    def handle_hamlib_command(self, command: str) -> bytes:
        fields = command.split()
        if not fields:
            return b"RPRT 0\n"

        if fields[0].lower() == "p" and len(fields) >= 3:
            try:
                self.state.set_target(float(fields[1]), float(fields[2]))
            except ValueError:
                return b"RPRT -1\n"
            self.log(f"[STATE] target accepted: {self.state.summary()}")
            return b"RPRT 0\n"

        if fields[0].lower() == "p":
            return (
                f"Azimuth: {number(self.state.azimuth)}\n"
                f"Elevation: {number(self.state.elevation)}\n"
                "RPRT 0\n"
            ).encode()

        if fields[0].upper() == "S":
            self.state.stop()
            self.log(f"[STATE] stop accepted: {self.state.summary()}")
            return b"RPRT 0\n"

        if fields[0].upper() == "K":
            self.state.park()
            self.log(f"[STATE] park accepted: {self.state.summary()}")
            return b"RPRT 0\n"

        return b"RPRT 0\n"


class UdpProtocol(asyncio.DatagramProtocol):
    def __init__(self, mock: RotatorMock) -> None:
        self.mock = mock

    def connection_made(self, transport: asyncio.BaseTransport) -> None:
        self.mock.transport = transport  # type: ignore[assignment]
        self.mock.log(
            f"Listening for {self.mock.protocol} UDP commands on "
            f"{self.mock.host}:{self.mock.port}"
        )
        if self.mock.protocol == "pstrotator":
            self.mock.log(f"PSTRotator feedback will be sent to UDP {self.mock.feedback_port}")

    def datagram_received(self, data: bytes, addr: tuple[str, int]) -> None:
        self.mock.handle_udp(data, addr)

    def error_received(self, exc: Exception) -> None:
        self.mock.log(f"[UDP error] {exc}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--protocol",
        choices=("pstrotator", "catrotator", "hamlib"),
        required=True,
        help="protocol to emulate",
    )
    parser.add_argument("--host", default="127.0.0.1", help="local bind address")
    parser.add_argument("--port", type=int, help="command port; defaults per protocol")
    parser.add_argument(
        "--feedback-port",
        type=int,
        help="PSTRotator feedback port; defaults to command port + 1",
    )
    parser.add_argument("--azimuth", type=float, default=0.0, help="initial azimuth")
    parser.add_argument("--elevation", type=float, default=0.0, help="initial elevation")
    args = parser.parse_args()

    if args.port is None:
        args.port = 4533 if args.protocol == "hamlib" else 12000
    if not 1 <= args.port <= 65535:
        parser.error("--port must be between 1 and 65535")
    if args.feedback_port is None:
        args.feedback_port = args.port + 1
    if not 1 <= args.feedback_port <= 65535:
        parser.error("--feedback-port must be between 1 and 65535")
    return args


async def run(args: argparse.Namespace) -> None:
    mock = RotatorMock(args)
    loop = asyncio.get_running_loop()
    stop_event = asyncio.Event()

    for signum in (signal.SIGINT, signal.SIGTERM):
        try:
            loop.add_signal_handler(signum, stop_event.set)
        except (NotImplementedError, RuntimeError):
            pass

    if args.protocol == "hamlib":
        server = await asyncio.start_server(
            mock.handle_hamlib_client, args.host, args.port
        )
        mock.log(f"Listening for Hamlib rotctld TCP on {args.host}:{args.port}")
        async with server:
            await stop_event.wait()
    else:
        transport, _ = await loop.create_datagram_endpoint(
            lambda: UdpProtocol(mock),
            local_addr=(args.host, args.port),
        )
        try:
            await stop_event.wait()
        finally:
            transport.close()

    mock.log("Mock stopped")


def main() -> None:
    args = parse_args()
    try:
        asyncio.run(run(args))
    except KeyboardInterrupt:
        pass
    except OSError as error:
        raise SystemExit(f"Cannot start mock on {args.host}:{args.port}: {error}")


if __name__ == "__main__":
    main()
