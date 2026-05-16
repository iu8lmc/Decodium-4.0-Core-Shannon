#!/usr/bin/env python3
"""
Ham Radio Deluxe TCP tap/proxy for Decodium diagnostics.

Run this on the same Windows PC as HRD, then point Decodium to the tap's
listen port instead of HRD's real port. The tap forwards traffic unchanged
and logs both directions, including decoded HRD v5/v4 payloads and raw hex.
"""

from __future__ import annotations

import argparse
import datetime as _dt
import json
import os
import platform
import socket
import struct
import sys
import threading
import time
import traceback
import zipfile
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Iterable


MAGIC_1 = 0x1234ABCD
MAGIC_2 = 0xABCD1234
HEADER_SIZE = 16
MAX_PACKET_BYTES = 16 * 1024 * 1024
RAW_FLUSH_BYTES = 4096


@dataclass
class Event:
    kind: str
    message: str
    data: dict[str, Any] = field(default_factory=dict)


class Logger:
    def __init__(self, log_path: Path) -> None:
        self.log_path = log_path
        self.events: list[Event] = []
        self._lock = threading.Lock()
        self._fh = log_path.open("w", encoding="utf-8", newline="\n")

    def close(self) -> None:
        with self._lock:
            self._fh.close()

    def log(self, kind: str, message: str, **data: Any) -> None:
        ts = _dt.datetime.now().isoformat(timespec="milliseconds")
        event = Event(kind=kind, message=message, data=data)
        suffix = ""
        if data:
            suffix = " " + json.dumps(data, ensure_ascii=False, sort_keys=True, default=str)
        line = f"{ts} [{kind}] {message}{suffix}"
        with self._lock:
            self.events.append(event)
            print(line)
            self._fh.write(line + "\n")
            self._fh.flush()

    def section(self, title: str) -> None:
        text = "\n" + "=" * 78 + "\n" + title + "\n" + "=" * 78
        with self._lock:
            print("\n" + title)
            self._fh.write(text + "\n")
            self._fh.flush()


def hex_preview(data: bytes, limit: int = 128) -> str:
    if not data:
        return ""
    text = data[:limit].hex(" ")
    if len(data) > limit:
        text += " ..."
    return text


def text_preview(text: str, limit: int = 320) -> str:
    text = text.replace("\r", "\\r").replace("\n", "\\n").replace("\t", " ")
    while "  " in text:
        text = text.replace("  ", " ")
    if len(text) > limit:
        text = text[:limit] + "..."
    return text


def decode_v5_payload(packet: bytes) -> str:
    payload = packet[HEADER_SIZE:]
    terminator = -1
    for index in range(0, max(0, len(payload) - 1), 2):
        if payload[index:index + 2] == b"\x00\x00":
            terminator = index
            break
    if terminator >= 0:
        payload = payload[:terminator]
    if len(payload) % 2:
        payload = payload[:-1]
    return payload.decode("utf-16le", errors="replace").strip()


def decode_v4_payload(packet: bytes) -> str:
    return packet.decode("latin-1", errors="replace").strip()


def find_line_end(buffer: bytearray) -> int:
    cr = buffer.find(b"\r")
    lf = buffer.find(b"\n")
    if cr < 0:
        return lf
    if lf < 0:
        return cr
    return min(cr, lf)


class HRDStreamDecoder:
    def __init__(self, log: Logger, connection_id: int, direction: str) -> None:
        self.log = log
        self.connection_id = connection_id
        self.direction = direction
        self.buffer = bytearray()
        self.packet_index = 0

    def feed(self, data: bytes) -> None:
        if not data:
            return
        self.buffer.extend(data)
        self._drain()

    def close(self) -> None:
        if self.buffer:
            self.packet_index += 1
            leftover = bytes(self.buffer)
            self.log.log(
                "RAW",
                "undecoded trailing bytes",
                connection=self.connection_id,
                direction=self.direction,
                packet=self.packet_index,
                bytes=len(leftover),
                hex=hex_preview(leftover),
            )
            self.buffer.clear()

    def _drain(self) -> None:
        while self.buffer:
            if self._try_v5_packet():
                continue
            if self._try_v4_line():
                continue
            if len(self.buffer) > RAW_FLUSH_BYTES:
                self.packet_index += 1
                raw = bytes(self.buffer)
                self.log.log(
                    "RAW",
                    "large undecoded byte run",
                    connection=self.connection_id,
                    direction=self.direction,
                    packet=self.packet_index,
                    bytes=len(raw),
                    hex=hex_preview(raw),
                )
                self.buffer.clear()
            return

    def _try_v5_packet(self) -> bool:
        if len(self.buffer) < HEADER_SIZE:
            return False
        size, magic_1, magic_2, checksum = struct.unpack("<IIII", self.buffer[:HEADER_SIZE])
        if magic_1 != MAGIC_1 or magic_2 != MAGIC_2:
            return False
        if size < HEADER_SIZE or size > MAX_PACKET_BYTES:
            self.packet_index += 1
            self.log.log(
                "WARN",
                "invalid v5 header size",
                connection=self.connection_id,
                direction=self.direction,
                packet=self.packet_index,
                size=size,
                checksum=checksum,
                buffered=len(self.buffer),
                hex=hex_preview(bytes(self.buffer[:HEADER_SIZE])),
            )
            del self.buffer[:HEADER_SIZE]
            return True
        if len(self.buffer) < size:
            return False
        packet = bytes(self.buffer[:size])
        del self.buffer[:size]
        self.packet_index += 1
        text = decode_v5_payload(packet)
        self.log.log(
            "PACKET",
            "hrd v5 packet",
            connection=self.connection_id,
            direction=self.direction,
            packet=self.packet_index,
            bytes=len(packet),
            payloadBytes=max(0, len(packet) - HEADER_SIZE),
            checksum=checksum,
            text=text_preview(text),
            hex=hex_preview(packet),
        )
        return True

    def _try_v4_line(self) -> bool:
        index = find_line_end(self.buffer)
        if index < 0:
            return False
        packet = bytes(self.buffer[:index + 1])
        del self.buffer[:index + 1]
        self.packet_index += 1
        text = decode_v4_payload(packet)
        self.log.log(
            "PACKET",
            "hrd v4 line",
            connection=self.connection_id,
            direction=self.direction,
            packet=self.packet_index,
            bytes=len(packet),
            text=text_preview(text),
            hex=hex_preview(packet),
        )
        return True


class HRDTap:
    def __init__(self, args: argparse.Namespace, log: Logger) -> None:
        self.args = args
        self.log = log
        self.stop_event = threading.Event()
        self.threads: list[threading.Thread] = []
        self.connection_count = 0
        self.server: socket.socket | None = None

    def run(self) -> int:
        self.server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.server.bind((self.args.listen_host, self.args.listen_port))
        self.server.listen(self.args.backlog)
        self.server.settimeout(0.5)
        actual_listen_host, actual_listen_port = self.server.getsockname()[:2]
        self.log.log(
            "INFO",
            "tap listening",
            listenHost=actual_listen_host,
            listenPort=actual_listen_port,
            targetHost=self.args.target_host,
            targetPort=self.args.target_port,
        )
        print()
        print(f"Configure Decodium HRD server as: {actual_listen_host}:{actual_listen_port}")
        print(f"Forwarding to real HRD server:    {self.args.target_host}:{self.args.target_port}")
        print("Press Ctrl+C to stop and write the diagnostic bundle.")

        deadline = time.monotonic() + self.args.duration if self.args.duration > 0 else None
        try:
            while not self.stop_event.is_set():
                if deadline is not None and time.monotonic() >= deadline:
                    self.log.log("INFO", "duration elapsed")
                    break
                try:
                    client, client_addr = self.server.accept()
                except socket.timeout:
                    continue
                except OSError as exc:
                    if self.stop_event.is_set():
                        break
                    self.log.log("ERROR", "accept failed", error=str(exc))
                    continue
                self.connection_count += 1
                connection_id = self.connection_count
                worker = threading.Thread(
                    target=self._handle_connection,
                    args=(connection_id, client, client_addr),
                    daemon=True,
                )
                self.threads.append(worker)
                worker.start()
                if self.args.single:
                    while worker.is_alive() and not self.stop_event.is_set():
                        worker.join(timeout=0.25)
                    break
        finally:
            self.stop()
        return 0

    def stop(self) -> None:
        self.stop_event.set()
        if self.server is not None:
            try:
                self.server.close()
            except OSError:
                pass
        for thread in list(self.threads):
            thread.join(timeout=2.0)

    def _handle_connection(self, connection_id: int, client: socket.socket, client_addr: tuple[str, int]) -> None:
        target: socket.socket | None = None
        try:
            client.settimeout(None)
            self.log.log(
                "INFO",
                "client connected",
                connection=connection_id,
                clientHost=client_addr[0],
                clientPort=client_addr[1],
            )
            target = socket.create_connection(
                (self.args.target_host, self.args.target_port),
                timeout=self.args.connect_timeout,
            )
            target.settimeout(None)
            self.log.log(
                "INFO",
                "target connected",
                connection=connection_id,
                targetHost=self.args.target_host,
                targetPort=self.args.target_port,
                local=target.getsockname(),
            )
            client_to_target = threading.Thread(
                target=self._forward,
                args=(connection_id, client, target, "DECODIUM->HRD"),
                daemon=True,
            )
            target_to_client = threading.Thread(
                target=self._forward,
                args=(connection_id, target, client, "HRD->DECODIUM"),
                daemon=True,
            )
            client_to_target.start()
            target_to_client.start()
            client_to_target.join()
            target_to_client.join()
        except Exception as exc:
            self.log.log(
                "ERROR",
                "connection handler failed",
                connection=connection_id,
                error=str(exc),
                traceback=traceback.format_exc(),
            )
        finally:
            for sock in (client, target):
                if sock is None:
                    continue
                try:
                    sock.close()
                except OSError:
                    pass
            self.log.log("INFO", "connection closed", connection=connection_id)

    def _forward(self, connection_id: int, src: socket.socket, dst: socket.socket, direction: str) -> None:
        decoder = HRDStreamDecoder(self.log, connection_id, direction)
        total = 0
        try:
            while not self.stop_event.is_set():
                data = src.recv(65536)
                if not data:
                    self.log.log("INFO", "stream EOF", connection=connection_id, direction=direction, bytesForwarded=total)
                    break
                total += len(data)
                decoder.feed(data)
                dst.sendall(data)
        except OSError as exc:
            self.log.log("WARN", "stream stopped", connection=connection_id, direction=direction, bytesForwarded=total, error=str(exc))
        finally:
            decoder.close()
            try:
                dst.shutdown(socket.SHUT_WR)
            except OSError:
                pass


def write_json_summary(log: Logger, json_path: Path, args: argparse.Namespace, exit_code: int) -> None:
    payload = {
        "tool": "hrd_tap",
        "timestamp": _dt.datetime.now().isoformat(timespec="seconds"),
        "exitCode": exit_code,
        "config": vars(args),
        "environment": {
            "platform": platform.platform(),
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
        description="Transparent Ham Radio Deluxe TCP tap/proxy for Decodium diagnostics.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument("--listen-host", default="127.0.0.1", help="local address the tap listens on")
    parser.add_argument("--listen-port", type=int, default=7810, help="local port Decodium connects to")
    parser.add_argument("--target-host", default="127.0.0.1", help="real HRD server host/IP")
    parser.add_argument("--target-port", type=int, default=7809, help="real HRD server port")
    parser.add_argument("--connect-timeout", type=float, default=5.0, help="target HRD TCP connect timeout in seconds")
    parser.add_argument("--duration", type=float, default=0.0, help="stop automatically after N seconds; 0 means manual Ctrl+C")
    parser.add_argument("--single", action="store_true", help="stop after the first Decodium connection closes")
    parser.add_argument("--backlog", type=int, default=8, help="TCP listen backlog")
    parser.add_argument("--output-dir", default=".", help="directory for log/json/zip output")
    parser.add_argument("--no-zip", action="store_true", help="do not create a zip bundle")
    parser.add_argument("--no-prompt", action="store_true", help="do not prompt when run interactively without arguments")
    return parser.parse_args(argv)


def maybe_prompt(args: argparse.Namespace, raw_argv: list[str]) -> None:
    if raw_argv or args.no_prompt or not sys.stdin.isatty():
        return
    print("Decodium HRD Tap - transparent TCP proxy")
    print("Run on the PC where HRD is listening. Then set Decodium HRD server to the tap listen address.")
    listen_host = input(f"Tap listen host [{args.listen_host}]: ").strip()
    if listen_host:
        args.listen_host = listen_host
    listen_port = input(f"Tap listen port [{args.listen_port}]: ").strip()
    if listen_port:
        try:
            args.listen_port = int(listen_port)
        except ValueError:
            print(f"Invalid listen port {listen_port!r}; keeping {args.listen_port}")
    target_host = input(f"Real HRD host [{args.target_host}]: ").strip()
    if target_host:
        args.target_host = target_host
    target_port = input(f"Real HRD port [{args.target_port}]: ").strip()
    if target_port:
        try:
            args.target_port = int(target_port)
        except ValueError:
            print(f"Invalid target port {target_port!r}; keeping {args.target_port}")


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    maybe_prompt(args, argv)

    output_dir = Path(args.output_dir).expanduser().resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    stamp = _dt.datetime.now().strftime("%Y%m%d_%H%M%S")
    base = f"hrd_tap_{args.listen_host.replace(':', '_')}_{args.listen_port}_to_{args.target_host.replace(':', '_')}_{args.target_port}_{stamp}"
    log_path = output_dir / f"{base}.log"
    json_path = output_dir / f"{base}.json"
    zip_path = output_dir / f"{base}.zip"

    log = Logger(log_path)
    exit_code = 1
    try:
        log.section("Environment")
        log.log(
            "INFO",
            "tap start",
            listenHost=args.listen_host,
            listenPort=args.listen_port,
            targetHost=args.target_host,
            targetPort=args.target_port,
            connectTimeout=args.connect_timeout,
            duration=args.duration,
            single=args.single,
            platform=platform.platform(),
            python=sys.version,
            executable=sys.executable,
            frozen=bool(getattr(sys, "frozen", False)),
        )
        tap = HRDTap(args, log)
        try:
            exit_code = tap.run()
        except KeyboardInterrupt:
            log.log("INFO", "interrupted by user")
            tap.stop()
            exit_code = 0
    except Exception as exc:
        exit_code = 2
        log.log("ERROR", "unhandled tap exception", error=str(exc), traceback=traceback.format_exc())
    finally:
        log.section("Summary")
        log.log("INFO" if exit_code == 0 else "ERROR", "tap complete", exitCode=exit_code, log=str(log_path), json=str(json_path), zip=None if args.no_zip else str(zip_path))
        write_json_summary(log, json_path, args, exit_code)
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
    return exit_code


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
