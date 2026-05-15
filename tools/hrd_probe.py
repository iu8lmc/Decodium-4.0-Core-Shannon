#!/usr/bin/env python3
"""
Ham Radio Deluxe TCP protocol probe for Decodium diagnostics.

This tool is intentionally read-only: it only sends HRD "get ..." commands.
It probes IPv4/IPv6 resolution, TCP connect behavior, HRD protocol v5/v4,
first-command sensitivity, command latency, raw reply headers, and close/reset
behavior without involving Decodium, Qt, CAT state, PTT, split, or shutdown.
"""

from __future__ import annotations

import argparse
import datetime as _dt
import json
import locale
import os
import platform
import socket
import struct
import sys
import time
import traceback
import zipfile
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Iterable


MAGIC_1 = 0x1234ABCD
MAGIC_2 = 0xABCD1234
HEADER_SIZE = 16
MAX_REPLY_BYTES = 16 * 1024 * 1024

DEFAULT_COMMANDS = [
    "get context",
    "get id",
    "get version",
    "get radios",
    "get radio",
    "get vfo-count",
    "get frequency",
    "get buttons",
    "get dropdowns",
    "get sliders",
]


@dataclass
class Event:
    kind: str
    message: str
    data: dict[str, Any] = field(default_factory=dict)


class Logger:
    def __init__(self, log_path: Path) -> None:
        self.log_path = log_path
        self.events: list[Event] = []
        self._fh = log_path.open("w", encoding="utf-8", newline="\n")

    def close(self) -> None:
        self._fh.close()

    def log(self, kind: str, message: str, **data: Any) -> None:
        ts = _dt.datetime.now().isoformat(timespec="milliseconds")
        event = Event(kind=kind, message=message, data=data)
        self.events.append(event)
        suffix = ""
        if data:
            suffix = " " + json.dumps(data, ensure_ascii=False, sort_keys=True, default=str)
        line = f"{ts} [{kind}] {message}{suffix}"
        print(line)
        self._fh.write(line + "\n")
        self._fh.flush()

    def section(self, title: str) -> None:
        self._fh.write("\n" + "=" * 78 + "\n")
        self._fh.write(title + "\n")
        self._fh.write("=" * 78 + "\n")
        self._fh.flush()
        print("\n" + title)


def now_ms() -> int:
    return int(time.monotonic() * 1000)


def hex_preview(data: bytes, limit: int = 96) -> str:
    if not data:
        return ""
    text = data[:limit].hex(" ")
    if len(data) > limit:
        text += " ..."
    return text


def text_preview(text: str, limit: int = 240) -> str:
    text = text.replace("\r", "\\r").replace("\n", "\\n").replace("\t", " ")
    while "  " in text:
        text = text.replace("  ", " ")
    if len(text) > limit:
        return text[:limit] + "..."
    return text


def build_v5_packet(command: str) -> bytes:
    payload = command.encode("utf-16le") + b"\x00\x00"
    size = HEADER_SIZE + len(payload)
    return struct.pack("<IIII", size, MAGIC_1, MAGIC_2, 0) + payload


def decode_v5_payload(packet: bytes) -> str:
    payload = packet[HEADER_SIZE:]
    terminator = payload.find(b"\x00\x00")
    if terminator >= 0:
        # Keep UTF-16 alignment.
        terminator -= terminator % 2
        payload = payload[:terminator]
    return payload.decode("utf-16le", errors="replace").strip()


def local_8bit_encoding() -> str:
    return locale.getpreferredencoding(False) or "latin-1"


def build_v4_packet(command: str) -> bytes:
    return (command + "\r").encode(local_8bit_encoding(), errors="replace")


def decode_v4_payload(packet: bytes) -> str:
    return packet.decode(local_8bit_encoding(), errors="replace").strip()


def recv_some(sock: socket.socket, timeout_s: float) -> bytes:
    sock.settimeout(timeout_s)
    return sock.recv(65536)


def recv_until_v4(sock: socket.socket, timeout_s: float, max_bytes: int = MAX_REPLY_BYTES) -> tuple[bytes, int]:
    started = now_ms()
    data = bytearray()
    while b"\r" not in data and b"\n" not in data and len(data) < max_bytes:
        chunk = recv_some(sock, timeout_s)
        if not chunk:
            break
        data.extend(chunk)
    return bytes(data), now_ms() - started


def recv_exactish_v5(sock: socket.socket, timeout_s: float, max_bytes: int = MAX_REPLY_BYTES) -> tuple[bytes, int]:
    started = now_ms()
    data = bytearray()
    while len(data) < HEADER_SIZE:
        chunk = recv_some(sock, timeout_s)
        if not chunk:
            break
        data.extend(chunk)
    if len(data) < HEADER_SIZE:
        return bytes(data), now_ms() - started

    size, magic1, magic2, _checksum = struct.unpack("<IIII", data[:HEADER_SIZE])
    if size < HEADER_SIZE or size > max_bytes:
        return bytes(data), now_ms() - started
    while len(data) < size:
        chunk = recv_some(sock, timeout_s)
        if not chunk:
            break
        data.extend(chunk)
    return bytes(data), now_ms() - started


def parse_radio_context(radios_reply: str, current_radio: str) -> int | None:
    current_norm = " ".join(current_radio.strip().casefold().split())
    first_id: int | None = None
    for raw in radios_reply.split(","):
        item = raw.strip()
        if not item or ":" not in item:
            continue
        left, right = item.split(":", 1)
        try:
            radio_id = int(left.strip())
        except ValueError:
            continue
        if first_id is None:
            first_id = radio_id
        right_norm = " ".join(right.strip().casefold().split())
        if current_norm and right_norm == current_norm:
            return radio_id
    return first_id


def resolve_addresses(host: str, port: int, family_mode: str) -> list[tuple[Any, ...]]:
    family = socket.AF_UNSPEC
    if family_mode == "ipv4":
        family = socket.AF_INET
    elif family_mode == "ipv6":
        family = socket.AF_INET6

    infos = socket.getaddrinfo(host, port, family, socket.SOCK_STREAM)
    seen: set[tuple[int, str, int]] = set()
    unique: list[tuple[Any, ...]] = []
    for info in infos:
        af, socktype, proto, canonname, sockaddr = info
        key = (af, sockaddr[0], sockaddr[1])
        if key in seen:
            continue
        seen.add(key)
        unique.append((af, socktype, proto, canonname, sockaddr))
    return unique


def address_label(info: tuple[Any, ...]) -> str:
    af, _socktype, _proto, _canonname, sockaddr = info
    family = "IPv4" if af == socket.AF_INET else "IPv6" if af == socket.AF_INET6 else str(af)
    return f"{family} {sockaddr[0]}:{sockaddr[1]}"


def open_socket(info: tuple[Any, ...], connect_timeout_s: float) -> socket.socket:
    af, socktype, proto, _canonname, sockaddr = info
    sock = socket.socket(af, socktype, proto)
    sock.settimeout(connect_timeout_s)
    sock.connect(sockaddr)
    return sock


def close_socket(sock: socket.socket, mode: str, log: Logger) -> None:
    if mode == "reset":
        try:
            # SO_LINGER with zero timeout asks the OS for an abortive close/RST.
            linger = struct.pack("hh", 1, 0)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, linger)
        except OSError as exc:
            log.log("WARN", "failed to set abortive SO_LINGER", error=str(exc))
    try:
        sock.close()
    except OSError as exc:
        log.log("WARN", "socket close failed", close_mode=mode, error=str(exc))


def send_command(
    sock: socket.socket,
    protocol: str,
    command: str,
    read_timeout_s: float,
    log: Logger,
    session: str,
) -> tuple[bool, str]:
    wire = build_v5_packet(command) if protocol == "v5" else build_v4_packet(command)
    started = now_ms()
    log.log(
        "SEND",
        "command",
        session=session,
        protocol=protocol,
        command=command,
        bytes=len(wire),
        hex=hex_preview(wire),
    )
    try:
        sock.sendall(wire)
        if protocol == "v5":
            reply, wait_ms = recv_exactish_v5(sock, read_timeout_s)
        else:
            reply, wait_ms = recv_until_v4(sock, read_timeout_s)
    except socket.timeout:
        elapsed = now_ms() - started
        log.log("TIMEOUT", "command timed out", session=session, protocol=protocol, command=command, elapsedMs=elapsed)
        return False, ""
    except OSError as exc:
        elapsed = now_ms() - started
        log.log("ERROR", "command socket error", session=session, protocol=protocol, command=command, elapsedMs=elapsed, error=str(exc))
        return False, ""

    elapsed = now_ms() - started
    if not reply:
        log.log("ERROR", "empty reply", session=session, protocol=protocol, command=command, elapsedMs=elapsed)
        return False, ""

    try:
        if protocol == "v5":
            if len(reply) < HEADER_SIZE:
                log.log("ERROR", "short v5 header", session=session, command=command, bytes=len(reply), hex=hex_preview(reply), elapsedMs=elapsed)
                return False, ""
            size, magic1, magic2, checksum = struct.unpack("<IIII", reply[:HEADER_SIZE])
            if magic1 != MAGIC_1 or magic2 != MAGIC_2:
                log.log(
                    "ERROR",
                    "invalid v5 header",
                    session=session,
                    command=command,
                    size=size,
                    magic1=f"0x{magic1:08X}",
                    magic2=f"0x{magic2:08X}",
                    checksum=checksum,
                    bytes=len(reply),
                    hex=hex_preview(reply),
                    elapsedMs=elapsed,
                )
                return False, ""
            if len(reply) < size:
                log.log("ERROR", "partial v5 packet", session=session, command=command, expected=size, got=len(reply), hex=hex_preview(reply), elapsedMs=elapsed)
                return False, ""
            text = decode_v5_payload(reply[:size])
            log.log(
                "RECV",
                "reply",
                session=session,
                protocol=protocol,
                command=command,
                elapsedMs=elapsed,
                waitMs=wait_ms,
                size=size,
                bytes=len(reply),
                result=text_preview(text),
                hex=hex_preview(reply),
            )
        else:
            text = decode_v4_payload(reply)
            log.log(
                "RECV",
                "reply",
                session=session,
                protocol=protocol,
                command=command,
                elapsedMs=elapsed,
                waitMs=wait_ms,
                bytes=len(reply),
                result=text_preview(text),
                hex=hex_preview(reply),
            )
        return True, text
    except Exception as exc:
        log.log("ERROR", "reply decode failed", session=session, protocol=protocol, command=command, error=str(exc), hex=hex_preview(reply), elapsedMs=elapsed)
        return False, ""


def connect_test(info: tuple[Any, ...], args: argparse.Namespace, log: Logger, close_mode: str) -> bool:
    label = address_label(info)
    started = now_ms()
    try:
        sock = open_socket(info, args.connect_timeout)
    except OSError as exc:
        log.log("ERROR", "tcp connect failed", address=label, elapsedMs=now_ms() - started, error=str(exc))
        return False
    log.log("INFO", "tcp connect ok", address=label, elapsedMs=now_ms() - started, closeMode=close_mode, local=sock.getsockname())
    close_socket(sock, close_mode, log)
    return True


def run_first_command_test(
    info: tuple[Any, ...],
    args: argparse.Namespace,
    log: Logger,
    protocol: str,
    first_command: str,
) -> bool:
    label = address_label(info)
    session = f"{label} {protocol} first={first_command!r}"
    log.section(f"First-command test: {session}")
    started = now_ms()
    try:
        sock = open_socket(info, args.connect_timeout)
    except OSError as exc:
        log.log("ERROR", "tcp connect failed", session=session, elapsedMs=now_ms() - started, error=str(exc))
        return False
    log.log("INFO", "tcp connect ok", session=session, elapsedMs=now_ms() - started, local=sock.getsockname())
    try:
        ok, _reply = send_command(sock, protocol, first_command, args.read_timeout, log, session)
        return ok
    finally:
        close_socket(sock, "graceful", log)


def run_sequence(info: tuple[Any, ...], args: argparse.Namespace, log: Logger, protocol: str) -> bool:
    label = address_label(info)
    session = f"{label} {protocol} sequence"
    log.section(f"Read-only command sequence: {session}")
    started = now_ms()
    try:
        sock = open_socket(info, args.connect_timeout)
    except OSError as exc:
        log.log("ERROR", "tcp connect failed", session=session, elapsedMs=now_ms() - started, error=str(exc))
        return False
    log.log("INFO", "tcp connect ok", session=session, elapsedMs=now_ms() - started, local=sock.getsockname())

    success_count = 0
    radios_reply = ""
    current_radio = ""
    selected_context: int | None = None
    try:
        for command in DEFAULT_COMMANDS:
            ok, reply = send_command(sock, protocol, command, args.read_timeout, log, session)
            if ok:
                success_count += 1
            if command == "get radios" and ok:
                radios_reply = reply
            elif command == "get radio" and ok:
                current_radio = reply

        selected_context = parse_radio_context(radios_reply, current_radio)
        if selected_context:
            log.log("INFO", "selected HRD context candidate", session=session, context=selected_context, currentRadio=text_preview(current_radio), radios=text_preview(radios_reply))
            for command in ["get vfo-count", "get frequency", "get buttons", "get dropdowns", "get sliders"]:
                contextual = f"[{selected_context}] {command}"
                ok, _reply = send_command(sock, protocol, contextual, args.read_timeout, log, session)
                if ok:
                    success_count += 1
        else:
            log.log("WARN", "unable to infer HRD context id from get radios/get radio", session=session, currentRadio=text_preview(current_radio), radios=text_preview(radios_reply))
    finally:
        close_socket(sock, "graceful", log)

    log.log("INFO", "sequence complete", session=session, protocol=protocol, successCount=success_count, context=selected_context)
    return success_count > 0


def write_json_summary(log: Logger, json_path: Path, args: argparse.Namespace, success: bool) -> None:
    payload = {
        "success": success,
        "createdAt": _dt.datetime.now().isoformat(timespec="seconds"),
        "args": vars(args),
        "platform": {
            "system": platform.system(),
            "release": platform.release(),
            "version": platform.version(),
            "machine": platform.machine(),
            "python": sys.version,
            "executable": sys.executable,
            "frozen": bool(getattr(sys, "frozen", False)),
        },
        "events": [
            {"kind": ev.kind, "message": ev.message, "data": ev.data}
            for ev in log.events
        ],
    }
    json_path.write_text(json.dumps(payload, ensure_ascii=False, indent=2, default=str), encoding="utf-8")


def make_zip(zip_path: Path, files: Iterable[Path]) -> None:
    with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        for path in files:
            zf.write(path, path.name)


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Read-only Ham Radio Deluxe TCP probe for Decodium diagnostics.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument("--host", default="127.0.0.1", help="HRD host/IP address")
    parser.add_argument("--port", type=int, default=7809, help="HRD TCP port")
    parser.add_argument("--family", choices=["all", "ipv4", "ipv6"], default="all", help="address family to test")
    parser.add_argument("--protocol", choices=["all", "v5", "v4"], default="all", help="HRD protocol to test")
    parser.add_argument("--connect-timeout", type=float, default=5.0, help="TCP connect timeout in seconds")
    parser.add_argument("--read-timeout", type=float, default=2.0, help="per-read command timeout in seconds")
    parser.add_argument("--output-dir", default=".", help="directory for log/json/zip output")
    parser.add_argument("--no-zip", action="store_true", help="do not create a zip bundle")
    parser.add_argument("--no-prompt", action="store_true", help="do not prompt when run interactively without arguments")
    return parser.parse_args(argv)


def maybe_prompt(args: argparse.Namespace, raw_argv: list[str]) -> None:
    if raw_argv or args.no_prompt or not sys.stdin.isatty():
        return
    print("Decodium HRD Probe - read-only diagnostics")
    print("This will only send HRD get-commands. It will not change radio/PTT/split.")
    host = input(f"HRD host/IP [{args.host}]: ").strip()
    if host:
        args.host = host
    port = input(f"HRD port [{args.port}]: ").strip()
    if port:
        try:
            args.port = int(port)
        except ValueError:
            print(f"Invalid port {port!r}; keeping {args.port}")


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    maybe_prompt(args, argv)

    output_dir = Path(args.output_dir).expanduser().resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    stamp = _dt.datetime.now().strftime("%Y%m%d_%H%M%S")
    base = f"hrd_probe_{args.host.replace(':', '_')}_{args.port}_{stamp}"
    log_path = output_dir / f"{base}.log"
    json_path = output_dir / f"{base}.json"
    zip_path = output_dir / f"{base}.zip"

    log = Logger(log_path)
    success = False
    try:
        log.section("Environment")
        log.log(
            "INFO",
            "probe start",
            host=args.host,
            port=args.port,
            family=args.family,
            protocol=args.protocol,
            connectTimeout=args.connect_timeout,
            readTimeout=args.read_timeout,
            platform=platform.platform(),
            python=sys.version,
            executable=sys.executable,
            frozen=bool(getattr(sys, "frozen", False)),
        )

        try:
            addresses = resolve_addresses(args.host, args.port, args.family)
        except OSError as exc:
            log.log("ERROR", "address resolution failed", host=args.host, port=args.port, error=str(exc))
            addresses = []

        log.section("Resolved addresses")
        for idx, info in enumerate(addresses, start=1):
            log.log("INFO", "resolved address", index=idx, address=address_label(info), raw=str(info[-1]))
        if not addresses:
            log.log("ERROR", "no addresses to test")

        protocols = ["v5", "v4"] if args.protocol == "all" else [args.protocol]
        for info in addresses:
            log.section(f"TCP connect tests: {address_label(info)}")
            success = connect_test(info, args, log, "graceful") or success
            success = connect_test(info, args, log, "reset") or success
            for protocol in protocols:
                for first in ["get context", "get id"]:
                    success = run_first_command_test(info, args, log, protocol, first) or success
                success = run_sequence(info, args, log, protocol) or success

        log.section("Summary")
        log.log("INFO" if success else "ERROR", "probe complete", success=success, log=str(log_path), json=str(json_path), zip=None if args.no_zip else str(zip_path))
    except Exception as exc:
        success = False
        log.log("ERROR", "unhandled probe exception", error=str(exc), traceback=traceback.format_exc())
    finally:
        write_json_summary(log, json_path, args, success)
        log.close()

    if not args.no_zip:
        make_zip(zip_path, [log_path, json_path])
        print(f"\nCreated diagnostic bundle: {zip_path}")
    print(f"Created log: {log_path}")
    print(f"Created JSON: {json_path}")

    if getattr(sys, "frozen", False) and os.name == "nt" and sys.stdin.isatty():
        try:
            input("\nPress Enter to close...")
        except EOFError:
            pass
    return 0 if success else 2


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
