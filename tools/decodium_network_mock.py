#!/usr/bin/env python3
"""
Decodium WSJT-X UDP / ADIF TCP mock server.

Typical local test:

  python3 tools/decodium_network_mock.py --udp-ports 2237,2239 --tcp-port 52001 --client-port 2238

Configure Decodium:
  Primary UDP server:   127.0.0.1:2237
  Secondary UDP server: 127.0.0.1:2239
  UDP listen port:      2238
  ADIF TCP:             127.0.0.1:52001

Interactive commands while the mock is running:
  reply CQ TEST1 JM75        Send a WSJT-X Reply packet to Decodium
  halt                       Send HaltTx
  halt auto                  Send HaltTx auto-only
  free TU 73                 Set/send free text
  setup 6 TU 73              Set a TX message slot
  replay                     Ask Decodium to replay decodes
  clear 2                    Clear both decode windows
  quit                       Stop the mock
"""

from __future__ import annotations

import argparse
import datetime as _dt
import select
import socket
import struct
import sys
import threading
import time
from dataclasses import dataclass
from typing import Iterable


MAGIC = 0xADBCCBDA
SCHEMA = 3
HMAC_MARKER = b"WSJTHMAC256:"

MSG_TYPES = {
    0: "Heartbeat",
    1: "Status",
    2: "Decode",
    3: "Clear",
    4: "Reply",
    5: "QSOLogged",
    6: "Close",
    7: "Replay",
    8: "HaltTx",
    9: "FreeText",
    10: "WSPRDecode",
    11: "Location",
    12: "LoggedADIF",
    13: "HighlightCallsign",
    14: "SwitchConfiguration",
    15: "Configure",
    16: "AnnotationInfo",
    17: "SetupTx",
    18: "EnqueueDecode",
}


class DecodeError(Exception):
    pass


class QtReader:
    def __init__(self, data: bytes) -> None:
        marker = data.find(HMAC_MARKER)
        self.data = data[:marker] if marker >= 0 else data
        self.pos = 0

    def remaining(self) -> int:
        return len(self.data) - self.pos

    def _read(self, size: int) -> bytes:
        if self.pos + size > len(self.data):
            raise DecodeError(f"short packet at byte {self.pos}, need {size}, have {self.remaining()}")
        out = self.data[self.pos : self.pos + size]
        self.pos += size
        return out

    def u8(self) -> int:
        return self._read(1)[0]

    def bool(self) -> bool:
        return self.u8() != 0

    def u32(self) -> int:
        return struct.unpack(">I", self._read(4))[0]

    def i32(self) -> int:
        return struct.unpack(">i", self._read(4))[0]

    def u64(self) -> int:
        return struct.unpack(">Q", self._read(8))[0]

    def i64(self) -> int:
        return struct.unpack(">q", self._read(8))[0]

    def double(self) -> float:
        return struct.unpack(">d", self._read(8))[0]

    def ba(self) -> str:
        size = self.u32()
        if size == 0xFFFFFFFF:
            return ""
        return self._read(size).decode("utf-8", errors="replace")

    def qtime(self) -> str:
        ms = self.u32()
        if ms == 0xFFFFFFFF:
            return ""
        sec, milli = divmod(ms, 1000)
        minute, second = divmod(sec, 60)
        hour, minute = divmod(minute, 60)
        return f"{hour:02d}:{minute:02d}:{second:02d}.{milli:03d}"

    def qdatetime(self) -> str:
        julian_day = self.i64()
        ms = self.u32()
        spec = self.u8()
        suffix = {0: "local", 1: "UTC", 2: "offset", 3: "tz"}.get(spec, f"spec={spec}")
        if spec == 2 and self.remaining() >= 4:
            offset = self.i32()
            suffix = f"offset={offset}"
        elif spec == 3:
            # QTimeZone serialization is not needed for Decodium's UTC log
            # packets; leave the remaining fields for best-effort parsing.
            suffix = "timezone"
        try:
            year, month, day = julian_to_gregorian(julian_day)
            sec, milli = divmod(ms, 1000)
            minute, second = divmod(sec, 60)
            hour, minute = divmod(minute, 60)
            return f"{year:04d}-{month:02d}-{day:02d}T{hour:02d}:{minute:02d}:{second:02d}.{milli:03d} {suffix}"
        except Exception:
            return f"jd={julian_day} ms={ms} {suffix}"


def julian_to_gregorian(jd: int) -> tuple[int, int, int]:
    l_val = jd + 68569
    n_val = (4 * l_val) // 146097
    l_val = l_val - (146097 * n_val + 3) // 4
    i_val = (4000 * (l_val + 1)) // 1461001
    l_val = l_val - (1461 * i_val) // 4 + 31
    j_val = (80 * l_val) // 2447
    day = l_val - (2447 * j_val) // 80
    l_val = j_val // 11
    month = j_val + 2 - 12 * l_val
    year = 100 * (n_val - 49) + i_val + l_val
    return year, month, day


def pack_ba(text: str | bytes) -> bytes:
    raw = text if isinstance(text, bytes) else text.encode("utf-8")
    return struct.pack(">I", len(raw)) + raw


def pack_bool(value: bool) -> bytes:
    return b"\x01" if value else b"\x00"


def pack_qtime_from_hhmmss(value: str | None = None) -> bytes:
    if value:
        digits = "".join(ch for ch in value if ch.isdigit())
        if len(digits) not in (4, 6):
            raise ValueError("time must be HHMM or HHMMSS")
        hour = int(digits[0:2])
        minute = int(digits[2:4])
        second = int(digits[4:6] or "0")
    else:
        now = _dt.datetime.utcnow().time()
        hour, minute, second = now.hour, now.minute, now.second
    return struct.pack(">I", ((hour * 60 + minute) * 60 + second) * 1000)


def build_message(msg_type: int, target_id: str, payload: bytes = b"", schema: int = SCHEMA) -> bytes:
    return (
        struct.pack(">III", MAGIC, schema, msg_type)
        + pack_ba(target_id)
        + payload
    )


def build_heartbeat(target_id: str, version: str = "DecodiumMock 1.0", revision: str = "mock") -> bytes:
    return build_message(0, target_id, struct.pack(">I", SCHEMA) + pack_ba(version) + pack_ba(revision))


def build_reply(
    target_id: str,
    message: str,
    mode: str,
    snr: int,
    dt: float,
    df: int,
    time_token: str | None,
    low_confidence: bool = False,
    modifiers: int = 0,
) -> bytes:
    payload = (
        pack_qtime_from_hhmmss(time_token)
        + struct.pack(">i", snr)
        + struct.pack(">d", dt)
        + struct.pack(">I", df)
        + pack_ba(mode)
        + pack_ba(message)
        + pack_bool(low_confidence)
        + struct.pack(">B", modifiers & 0xFF)
    )
    return build_message(4, target_id, payload)


def build_halt(target_id: str, auto_only: bool) -> bytes:
    return build_message(8, target_id, pack_bool(auto_only))


def build_free_text(target_id: str, text: str, send: bool = True) -> bytes:
    return build_message(9, target_id, pack_ba(text) + pack_bool(send))


def build_replay(target_id: str) -> bytes:
    return build_message(7, target_id)


def build_clear(target_id: str, window: int) -> bytes:
    return build_message(3, target_id, struct.pack(">B", window & 0xFF))


def build_setup_tx(target_id: str, tx_index: int, message: str, offset: int) -> bytes:
    payload = (
        struct.pack(">i", tx_index)
        + pack_ba(message)
        + pack_bool(False)
        + pack_bool(False)
        + pack_ba("")
        + struct.pack(">I", offset & 0xFFFFFFFF)
    )
    return build_message(17, target_id, payload)


def parse_packet(data: bytes) -> dict[str, object]:
    reader = QtReader(data)
    magic = reader.u32()
    if magic != MAGIC:
        raise DecodeError(f"bad magic 0x{magic:08x}")
    schema = reader.u32()
    msg_type = reader.u32()
    client_id = reader.ba()
    parsed: dict[str, object] = {
        "schema": schema,
        "type": msg_type,
        "type_name": MSG_TYPES.get(msg_type, f"Unknown({msg_type})"),
        "id": client_id,
    }

    if msg_type == 0:
        parsed.update(max_schema=reader.u32(), version=reader.ba(), revision=reader.ba())
    elif msg_type == 1:
        parsed.update(
            dial_hz=reader.u64(),
            mode=reader.ba(),
            dx_call=reader.ba(),
            report=reader.ba(),
            tx_mode=reader.ba(),
            tx_enabled=reader.bool(),
            transmitting=reader.bool(),
            decoding=reader.bool(),
            rx_df=reader.u32(),
            tx_df=reader.u32(),
            de_call=reader.ba(),
            de_grid=reader.ba(),
            dx_grid=reader.ba(),
            watchdog=reader.bool(),
            submode=reader.ba(),
            fast=reader.bool(),
            special_op=reader.u8(),
            freq_tolerance=reader.u32(),
            tr_period=reader.u32(),
            config=reader.ba(),
            last_tx_msg=reader.ba(),
        )
        if reader.remaining() >= 4:
            parsed["qso_progress"] = reader.u32()
        if reader.remaining() >= 1:
            parsed["tx_first"] = reader.bool()
        if reader.remaining() >= 1:
            parsed["cq_only"] = reader.bool()
        if reader.remaining() >= 4:
            parsed["gen_msg"] = reader.ba()
    elif msg_type == 2:
        parsed.update(
            new=reader.bool(),
            time=reader.qtime(),
            snr=reader.i32(),
            dt=reader.double(),
            df=reader.u32(),
            mode=reader.ba(),
            message=reader.ba(),
            low_confidence=reader.bool(),
            off_air=reader.bool(),
        )
    elif msg_type == 5:
        parsed.update(
            time_off=reader.qdatetime(),
            dx_call=reader.ba(),
            dx_grid=reader.ba(),
            dial_hz=reader.u64(),
            mode=reader.ba(),
            report_sent=reader.ba(),
            report_received=reader.ba(),
            tx_power=reader.ba(),
            comments=reader.ba(),
            name=reader.ba(),
            time_on=reader.qdatetime(),
            operator_call=reader.ba(),
            my_call=reader.ba(),
            my_grid=reader.ba(),
            exchange_sent=reader.ba(),
            exchange_received=reader.ba(),
            propmode=reader.ba(),
        )
        if reader.remaining() >= 4:
            parsed["satellite"] = reader.ba()
        if reader.remaining() >= 4:
            parsed["satmode"] = reader.ba()
        if reader.remaining() >= 4:
            parsed["freq_rx"] = reader.ba()
    elif msg_type == 12:
        parsed["adif"] = reader.ba()
    elif msg_type == 18:
        parsed.update(
            auto_gen=reader.bool(),
            time=reader.qtime(),
            snr=reader.i32(),
            dt=reader.double(),
            df=reader.u32(),
            mode=reader.ba(),
            message=reader.ba(),
        )

    parsed["remaining"] = reader.remaining()
    return parsed


def compact_summary(parsed: dict[str, object]) -> str:
    msg = f"{parsed['type_name']} id={parsed['id']} schema={parsed['schema']}"
    msg_type = parsed["type"]
    if msg_type == 0:
        return f"{msg} max_schema={parsed.get('max_schema')} version={parsed.get('version')} revision={parsed.get('revision')}"
    if msg_type == 1:
        return (
            f"{msg} de={parsed.get('de_call')} grid={parsed.get('de_grid')} "
            f"mode={parsed.get('mode')} dial={parsed.get('dial_hz')} "
            f"tx_enabled={parsed.get('tx_enabled')} tx={parsed.get('transmitting')} "
            f"rx_df={parsed.get('rx_df')} tx_df={parsed.get('tx_df')} "
            f"last='{parsed.get('last_tx_msg')}'"
        )
    if msg_type in (2, 18):
        return (
            f"{msg} {parsed.get('time')} {parsed.get('snr')} "
            f"{parsed.get('dt')} {parsed.get('df')} {parsed.get('mode')} "
            f"{parsed.get('message')}"
        )
    if msg_type == 5:
        return (
            f"{msg} qso={parsed.get('dx_call')} grid={parsed.get('dx_grid')} "
            f"mode={parsed.get('mode')} sent={parsed.get('report_sent')} "
            f"rcvd={parsed.get('report_received')} my={parsed.get('my_call')}"
        )
    if msg_type == 12:
        adif = str(parsed.get("adif", "")).replace("\n", "\\n")
        if len(adif) > 220:
            adif = adif[:220] + "..."
        return f"{msg} adif='{adif}'"
    return msg


def parse_ports(value: str) -> list[int]:
    ports = []
    for part in value.split(","):
        part = part.strip()
        if not part:
            continue
        port = int(part)
        if not 1 <= port <= 65535:
            raise argparse.ArgumentTypeError(f"invalid port: {port}")
        ports.append(port)
    if not ports:
        raise argparse.ArgumentTypeError("at least one port is required")
    return ports


def make_udp_socket(host: str, port: int, multicast_groups: Iterable[str], multicast_interface: str) -> socket.socket:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
    except (AttributeError, OSError):
        pass
    sock.bind((host, port))
    for group in multicast_groups:
        membership = socket.inet_aton(group) + socket.inet_aton(multicast_interface)
        sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, membership)
    return sock


@dataclass
class Runtime:
    args: argparse.Namespace
    udp_sockets: list[socket.socket]
    stop: threading.Event
    lock: threading.Lock

    def send_to_client(self, packet: bytes) -> None:
        if not self.udp_sockets:
            raise RuntimeError("no UDP socket available")
        with self.lock:
            self.udp_sockets[0].sendto(packet, (self.args.client_host, self.args.client_port))


def udp_loop(runtime: Runtime) -> None:
    while not runtime.stop.is_set():
        readable, _, _ = select.select(runtime.udp_sockets, [], [], 0.5)
        for sock in readable:
            try:
                data, addr = sock.recvfrom(65535)
            except OSError as exc:
                if runtime.stop.is_set():
                    return
                print(f"[udp] receive error: {exc}", flush=True)
                continue
            local_port = sock.getsockname()[1]
            try:
                parsed = parse_packet(data)
                print(f"[udp:{local_port} <- {addr[0]}:{addr[1]}] {compact_summary(parsed)}", flush=True)
                if parsed["type"] == 0 and runtime.args.reply_heartbeat:
                    response = build_heartbeat(str(parsed["id"]))
                    sock.sendto(response, addr)
                    print(f"[udp:{local_port} -> {addr[0]}:{addr[1]}] Heartbeat id={parsed['id']}", flush=True)
            except Exception as exc:
                print(f"[udp:{local_port} <- {addr[0]}:{addr[1]}] decode error: {exc} bytes={len(data)}", flush=True)
                if runtime.args.verbose_hex:
                    print(data.hex(), flush=True)


def tcp_loop(runtime: Runtime) -> None:
    if runtime.args.tcp_port <= 0:
        return
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind((runtime.args.tcp_host, runtime.args.tcp_port))
    server.listen(8)
    server.settimeout(0.5)
    print(f"[tcp] listening on {runtime.args.tcp_host}:{runtime.args.tcp_port}", flush=True)
    try:
        while not runtime.stop.is_set():
            try:
                conn, addr = server.accept()
            except socket.timeout:
                continue
            with conn:
                chunks = []
                conn.settimeout(2.0)
                while True:
                    try:
                        chunk = conn.recv(65535)
                    except socket.timeout:
                        break
                    if not chunk:
                        break
                    chunks.append(chunk)
                payload = b"".join(chunks)
                text = payload.decode("utf-8", errors="replace").replace("\n", "\\n")
                print(f"[tcp <- {addr[0]}:{addr[1]}] bytes={len(payload)} '{text}'", flush=True)
    finally:
        server.close()


def handle_command(runtime: Runtime, line: str) -> None:
    command, _, rest = line.strip().partition(" ")
    command = command.lower()
    rest = rest.strip()
    if not command:
        return
    if command in {"quit", "exit"}:
        runtime.stop.set()
        return
    if command == "reply":
        message = rest or runtime.args.reply_message
        packet = build_reply(
            runtime.args.target_id,
            message,
            runtime.args.reply_mode,
            runtime.args.reply_snr,
            runtime.args.reply_dt,
            runtime.args.reply_df,
            runtime.args.reply_time,
        )
        runtime.send_to_client(packet)
        print(f"[udp -> {runtime.args.client_host}:{runtime.args.client_port}] Reply '{message}'", flush=True)
        return
    if command == "halt":
        auto_only = rest.lower() in {"auto", "1", "true", "yes"}
        runtime.send_to_client(build_halt(runtime.args.target_id, auto_only))
        print(f"[udp -> {runtime.args.client_host}:{runtime.args.client_port}] HaltTx auto_only={auto_only}", flush=True)
        return
    if command == "free":
        runtime.send_to_client(build_free_text(runtime.args.target_id, rest, True))
        print(f"[udp -> {runtime.args.client_host}:{runtime.args.client_port}] FreeText '{rest}'", flush=True)
        return
    if command == "setup":
        tx_text = rest or f"6 {runtime.args.reply_message}"
        tx_token, _, message = tx_text.partition(" ")
        try:
            tx_index = int(tx_token)
        except ValueError:
            tx_index = 6
            message = tx_text
        message = message.strip() or runtime.args.reply_message
        runtime.send_to_client(build_setup_tx(runtime.args.target_id, tx_index, message, runtime.args.reply_df))
        print(f"[udp -> {runtime.args.client_host}:{runtime.args.client_port}] SetupTx TX{tx_index} '{message}'", flush=True)
        return
    if command == "replay":
        runtime.send_to_client(build_replay(runtime.args.target_id))
        print(f"[udp -> {runtime.args.client_host}:{runtime.args.client_port}] Replay", flush=True)
        return
    if command == "clear":
        window = int(rest or "2")
        runtime.send_to_client(build_clear(runtime.args.target_id, window))
        print(f"[udp -> {runtime.args.client_host}:{runtime.args.client_port}] Clear window={window}", flush=True)
        return
    if command == "help":
        print("commands: reply <msg>, halt [auto], free <text>, setup <1-6> <msg>, replay, clear [0|1|2], quit", flush=True)
        return
    print(f"unknown command: {command}", flush=True)


def run_self_test() -> None:
    hb = build_heartbeat("WSJTX")
    parsed_hb = parse_packet(hb)
    assert parsed_hb["type"] == 0
    assert parsed_hb["id"] == "WSJTX"

    reply = build_reply("WSJTX", "CQ TEST1 JM75", "FT8", -12, 0.1, 1500, "123000")
    parsed_reply = parse_packet(reply)
    assert parsed_reply["type"] == 4
    assert parsed_reply["id"] == "WSJTX"

    decode = (
        build_message(
            2,
            "WSJTX",
            pack_bool(True)
            + pack_qtime_from_hhmmss("123015")
            + struct.pack(">i", -18)
            + struct.pack(">d", 0.2)
            + struct.pack(">I", 1200)
            + pack_ba("FT8")
            + pack_ba("CQ DX TEST2 JM76")
            + pack_bool(False)
            + pack_bool(False),
        )
    )
    parsed_decode = parse_packet(decode)
    assert parsed_decode["type"] == 2
    assert parsed_decode["message"] == "CQ DX TEST2 JM76"
    print("self-test ok")


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Mock WSJT-X UDP server and ADIF TCP receiver for Decodium.")
    parser.add_argument("--udp-host", default="0.0.0.0", help="UDP bind host, default: 0.0.0.0")
    parser.add_argument("--udp-ports", type=parse_ports, default=[2237, 2239], help="Comma-separated UDP ports, default: 2237,2239")
    parser.add_argument("--client-host", default="127.0.0.1", help="Decodium UDP listen host for injected commands")
    parser.add_argument("--client-port", type=int, default=2238, help="Decodium UDP listen port for injected commands")
    parser.add_argument("--target-id", default="WSJTX", help="Target id for injected WSJT-X control packets")
    parser.add_argument("--tcp-host", default="0.0.0.0", help="TCP bind host, default: 0.0.0.0")
    parser.add_argument("--tcp-port", type=int, default=52001, help="ADIF TCP listen port; use 0 to disable")
    parser.add_argument("--multicast-group", action="append", default=[], help="Join an IPv4 multicast group; can be repeated")
    parser.add_argument("--multicast-interface", default="0.0.0.0", help="IPv4 interface address for multicast join")
    parser.add_argument("--reply-message", default="CQ TEST1 JM75", help="Default interactive reply message")
    parser.add_argument("--reply-mode", default="FT8")
    parser.add_argument("--reply-snr", type=int, default=-12)
    parser.add_argument("--reply-dt", type=float, default=0.1)
    parser.add_argument("--reply-df", type=int, default=1500)
    parser.add_argument("--reply-time", default=None, help="Reply time HHMM or HHMMSS; default: current UTC")
    parser.add_argument("--send-reply", action="store_true", help="Send one Reply packet shortly after startup")
    parser.add_argument("--no-stdin", action="store_true", help="Do not read interactive commands from stdin")
    parser.add_argument("--no-heartbeat-reply", dest="reply_heartbeat", action="store_false", help="Do not answer incoming heartbeats")
    parser.add_argument("--verbose-hex", action="store_true", help="Print hex for packets that cannot be decoded")
    parser.add_argument("--self-test", action="store_true", help="Run parser/encoder self-test and exit")
    parser.set_defaults(reply_heartbeat=True)
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_arg_parser()
    args = parser.parse_args(argv)
    if args.self_test:
        run_self_test()
        return 0

    stop = threading.Event()
    udp_sockets = [
        make_udp_socket(args.udp_host, port, args.multicast_group, args.multicast_interface)
        for port in args.udp_ports
    ]
    runtime = Runtime(args=args, udp_sockets=udp_sockets, stop=stop, lock=threading.Lock())

    for sock in udp_sockets:
        host, port = sock.getsockname()
        print(f"[udp] listening on {host}:{port}", flush=True)
    if args.multicast_group:
        print(f"[udp] joined multicast groups: {', '.join(args.multicast_group)}", flush=True)
    print(f"[udp] injected commands target {args.client_host}:{args.client_port} id={args.target_id}", flush=True)

    threads = [
        threading.Thread(target=udp_loop, args=(runtime,), daemon=True),
        threading.Thread(target=tcp_loop, args=(runtime,), daemon=True),
    ]
    for thread in threads:
        thread.start()

    if args.send_reply:
        time.sleep(0.5)
        handle_command(runtime, "reply")

    try:
        if args.no_stdin:
            while not stop.is_set():
                time.sleep(0.5)
        else:
            print("Type 'help' for commands.", flush=True)
            while not stop.is_set():
                line = sys.stdin.readline()
                if not line:
                    time.sleep(0.2)
                    continue
                handle_command(runtime, line)
    except KeyboardInterrupt:
        stop.set()
    finally:
        stop.set()
        for thread in threads:
            thread.join(timeout=1.5)
        for sock in udp_sockets:
            sock.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
