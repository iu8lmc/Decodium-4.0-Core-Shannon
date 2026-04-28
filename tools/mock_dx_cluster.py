#!/usr/bin/env python3
"""Mock DX cluster telnet node for Decodium AutoSpot tests.

Run `python3 tools/mock_dx_cluster.py --self-test` to verify the full login,
spot-submit and show/dx confirmation flow without posting to a public cluster.
"""

from __future__ import annotations

import argparse
import socket
import socketserver
import threading
import time
from dataclasses import dataclass, field


IAC = bytes([255])
WILL_ECHO = bytes([255, 251, 1])
WILL_SUPPRESS_GO_AHEAD = bytes([255, 251, 3])


@dataclass
class MockState:
    received: list[str] = field(default_factory=list)
    spots: list[str] = field(default_factory=list)


class MockDxClusterHandler(socketserver.BaseRequestHandler):
    def _send(self, text: str | bytes) -> None:
        if isinstance(text, str):
            text = text.encode("ascii", errors="replace")
        self.request.sendall(text)

    def _read_line(self, timeout: float = 6.0) -> str:
        self.request.settimeout(timeout)
        data = bytearray()
        while True:
            chunk = self.request.recv(1)
            if not chunk:
                break
            if chunk == IAC:
                # Skip a 3-byte Telnet negotiation response from the client.
                self.request.recv(2)
                continue
            data.extend(chunk)
            if chunk == b"\n":
                break
        return bytes(data).decode("ascii", errors="replace").strip()

    def handle(self) -> None:
        state: MockState = self.server.state  # type: ignore[attr-defined]

        self._send(WILL_ECHO + WILL_SUPPRESS_GO_AHEAD)
        self._send("Mock DX Cluster\r\nlogin: ")

        my_call = self._read_line().upper()
        state.received.append(my_call)
        self._send(f"\r\n{my_call} de MOCK-1 > ")

        spot_cmd = self._read_line()
        state.received.append(spot_cmd)
        parts = spot_cmd.split(maxsplit=3)
        if len(parts) >= 3 and parts[0].upper() == "DX":
            freq = parts[1]
            dx_call = parts[2].upper()
            comment = parts[3] if len(parts) > 3 else ""
            line = f"DX de {my_call}:     {freq}  {dx_call:<12} {comment} 1234Z <{my_call}>"
            state.spots.append(line)
            self._send(f"\r\n{line}\r\n{my_call} de MOCK-1 > ")
        else:
            self._send(f"\r\nSorry, bad spot syntax\r\n{my_call} de MOCK-1 > ")

        verify_cmd = self._read_line()
        state.received.append(verify_cmd)
        if verify_cmd.lower().startswith(("show/dx", "sh/dx")):
            self._send("\r\n" + "\r\n".join(state.spots) + f"\r\n{my_call} de MOCK-1 > ")

        bye = self._read_line(timeout=2.0)
        if bye:
            state.received.append(bye)


class MockDxClusterServer(socketserver.ThreadingTCPServer):
    allow_reuse_address = True

    def __init__(self, address: tuple[str, int], state: MockState) -> None:
        super().__init__(address, MockDxClusterHandler)
        self.state = state


def _read_until(sock: socket.socket, needles: tuple[bytes, ...], timeout: float = 6.0) -> bytes:
    sock.settimeout(timeout)
    data = bytearray()
    while True:
        chunk = sock.recv(1024)
        if not chunk:
            return bytes(data)
        i = 0
        while i < len(chunk):
            if chunk[i] == 255 and i + 2 < len(chunk):
                cmd = chunk[i + 1]
                opt = chunk[i + 2]
                if cmd == 251:  # WILL
                    sock.sendall(bytes([255, 253 if opt in (1, 3) else 254, opt]))
                elif cmd == 253:  # DO
                    sock.sendall(bytes([255, 251 if opt in (1, 3) else 252, opt]))
                i += 3
                continue
            data.append(chunk[i])
            i += 1
        if any(needle in data for needle in needles):
            return bytes(data)


def self_test() -> None:
    state = MockState()
    server = MockDxClusterServer(("127.0.0.1", 0), state)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    host, port = server.server_address

    try:
        with socket.create_connection((host, port), timeout=5.0) as sock:
            _read_until(sock, (b"login:",))
            sock.sendall(b"9H1SR\r\n")
            _read_until(sock, (b">",))
            sock.sendall(b"DX 14074.0 EA3EQS FT8 Decodium\r\n")
            _read_until(sock, (b">",))
            sock.sendall(b"show/dx 200 by 9H1SR\r\n")
            verify = _read_until(sock, (b"EA3EQS", b">"))
            sock.sendall(b"bye\r\n")

        assert b"EA3EQS" in verify and b"<9H1SR>" in verify, verify.decode(errors="replace")
        assert any(cmd.startswith("DX 14074.0 EA3EQS") for cmd in state.received), state.received
        print("PASS mock dx cluster: login, spot and show/dx verification completed")
        print("received:", " | ".join(state.received))
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=2.0)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=7373)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()

    if args.self_test:
        self_test()
        return

    state = MockState()
    server = MockDxClusterServer((args.host, args.port), state)
    print(f"Mock DX cluster listening on {args.host}:{args.port}")
    try:
        server.serve_forever(poll_interval=0.2)
    except KeyboardInterrupt:
        pass
    finally:
        server.shutdown()
        server.server_close()
        time.sleep(0.1)


if __name__ == "__main__":
    main()
