"""Loopback-only regression tests; no Minecraft account or public server required."""
import concurrent.futures
import contextlib
import json
import os
from pathlib import Path
import queue
import signal
import socket
import socketserver
import struct
import subprocess
import sys
import threading
import time
import unittest

BINARY = str(Path(sys.argv.pop(1)).resolve())


def varint(value):
    result = bytearray()
    while True:
        byte = value & 127
        value >>= 7
        result.append(byte | (128 if value else 0))
        if not value:
            return bytes(result)


def receive(sock, size):
    result = bytearray()
    while len(result) < size:
        chunk = sock.recv(size - len(result))
        if not chunk:
            raise EOFError("Peer closed before all bytes arrived")
        result.extend(chunk)
    return bytes(result)


def read_varint(sock):
    result = 0
    for shift in range(0, 35, 7):
        byte = receive(sock, 1)[0]
        result |= (byte & 127) << shift
        if not byte & 128:
            return result
    raise ValueError("Overlong VarInt")


def frame(payload):
    return varint(len(payload)) + payload


def packet(sock):
    length = read_varint(sock)
    if length > 131072:
        raise ValueError("Unexpected response size")
    return receive(sock, length)


def handshake(host="localhost", state=2):
    host = host.encode()
    return frame(b"\x00" + varint(47) + varint(len(host)) + host + struct.pack(">H", 25565) + varint(state))


class Echo(socketserver.BaseRequestHandler):
    def handle(self):
        try:
            while True:
                data = self.request.recv(16384)
                if not data:
                    return
                self.request.sendall(data)
        except (ConnectionError, OSError):
            pass


class EchoServer(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


class Proxy:
    def __init__(self, upstream_port, *options, host="127.0.0.1"):
        self.logs = []
        self.lines = queue.Queue()
        self.process = subprocess.Popen(
            [BINARY, host, "--listen-address", "127.0.0.1", "--listen-port", "0",
             "--upstream-port", str(upstream_port), "--threads", "4", *options],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding="utf-8")
        self.reader = threading.Thread(target=self._read, daemon=True)
        self.reader.start()
        try:
            deadline = time.monotonic() + 8
            while time.monotonic() < deadline:
                line = self.lines.get(timeout=max(0.01, deadline - time.monotonic()))
                if line.startswith("Listening on "):
                    self.port = int(line.rsplit(":", 1)[1])
                    return
                if line == "<EOF>":
                    break
            raise RuntimeError("Proxy did not start:\n" + "".join(self.logs))
        except BaseException:
            self.process.kill()
            self.process.wait()
            self.reader.join(timeout=2)
            self.process.stdout.close()
            raise

    def _read(self):
        for line in self.process.stdout:
            self.logs.append(line)
            self.lines.put(line)
        self.lines.put("<EOF>")

    def connect(self):
        result = socket.create_connection(("127.0.0.1", self.port), timeout=3)
        result.settimeout(3)
        return result

    def stop(self, sig=signal.SIGTERM):
        if self.process.poll() is None:
            if os.name == "posix":
                self.process.send_signal(sig)
            else:
                self.process.terminate()
        try:
            self.process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait()
            raise AssertionError("Proxy failed to stop within five seconds")
        finally:
            self.reader.join(timeout=2)
            self.process.stdout.close()
        log = "".join(self.logs)
        if "ERROR: AddressSanitizer" in log or "runtime error:" in log or "WARNING: ThreadSanitizer" in log:
            raise AssertionError(log)
        if os.name == "posix" and self.process.returncode != 0:
            raise AssertionError(f"Proxy exited with {self.process.returncode}:\n{log}")


class Integration(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.echo = EchoServer(("127.0.0.1", 0), Echo)
        cls.echo_thread = threading.Thread(target=cls.echo.serve_forever, daemon=True)
        cls.echo_thread.start()
        cls.upstream_port = cls.echo.server_address[1]

    @classmethod
    def tearDownClass(cls):
        cls.echo.shutdown()
        cls.echo.server_close()
        cls.echo_thread.join(timeout=2)

    @contextlib.contextmanager
    def proxy(self, *options, **kwargs):
        proxy = Proxy(self.upstream_port, *options, **kwargs)
        try:
            yield proxy
        finally:
            proxy.stop()

    def closed(self, sock):
        try:
            self.assertEqual(sock.recv(1), b"")
        except ConnectionResetError:
            pass

    def login(self, sock, expected_host="127.0.0.1", incoming_host="localhost"):
        sock.sendall(handshake(incoming_host))
        expected = b"\x00" + varint(47) + varint(len(expected_host)) + expected_host.encode() + struct.pack(">H", self.upstream_port) + b"\x02"
        self.assertEqual(packet(sock), expected)

    def test_status_and_framed_pong(self):
        with self.proxy("--motd", "Integration test", "--version-name", "test", "--protocol", "123", "--favicon", "none") as p, p.connect() as s:
            s.sendall(handshake(state=1) + frame(b"\x00"))
            response = packet(s)
            self.assertEqual(response[0], 0)
            offset = 1
            while response[offset] & 128:
                offset += 1
            status = json.loads(response[offset + 1:])
            self.assertEqual(status["description"]["text"], "Integration test")
            self.assertEqual(status["version"], {"name": "test", "protocol": 123})
            self.assertNotIn("favicon", status)
            ping = b"\x01" + struct.pack(">q", -123456789)
            s.sendall(frame(ping))
            self.assertEqual(packet(s), ping)
            self.closed(s)

    def test_rewrite_long_incoming_hostname(self):
        with self.proxy() as p, p.connect() as s:
            self.login(s, incoming_host="a" * 130)

    def test_rewrite_long_configured_hostname(self):
        host = "b" * 130
        with self.proxy("--handshake-host", host) as p, p.connect() as s:
            self.login(s, expected_host=host)

    def test_fragmented_handshake_and_coalesced_payload(self):
        with self.proxy() as p, p.connect() as s:
            wire = handshake()
            for byte in wire[:-1]:
                s.sendall(bytes([byte]))
            s.sendall(wire[-1:] + b"already queued after handshake")
            response = packet(s)
            self.assertIn(b"127.0.0.1", response)
            self.assertEqual(receive(s, 30), b"already queued after handshake")

    def test_large_payload(self):
        with self.proxy() as p, p.connect() as s:
            self.login(s)
            payload = bytes(range(256)) * 4096
            # Read concurrently so a bounded relay cannot deadlock behind full TCP buffers.
            with concurrent.futures.ThreadPoolExecutor(max_workers=1) as executor:
                response = executor.submit(receive, s, len(payload))
                s.sendall(payload)
                self.assertEqual(response.result(timeout=10), payload)

    def test_multiple_clients(self):
        with self.proxy() as p:
            def client(index):
                with p.connect() as s:
                    self.login(s)
                    payload = bytes([index]) * 32768
                    s.sendall(payload)
                    self.assertEqual(receive(s, len(payload)), payload)
            with concurrent.futures.ThreadPoolExecutor(max_workers=12) as executor:
                list(executor.map(client, range(24)))

    def test_invalid_packets_do_not_stop_acceptance(self):
        invalid = [varint(4097), b"\x80" * 6, b"\xff\xff\xff\xff\x10", frame(b"\x00\x2f\x7fA"),
                   frame(b"\x00\x2f\x01a\x63\xdd\x03"), frame(b"\x01\x2f\x01a\x63\xdd\x02")]
        with self.proxy() as p:
            for wire in invalid:
                with p.connect() as s:
                    s.sendall(wire)
                    self.closed(s)
            with p.connect() as s:
                self.login(s)

    def test_invalid_status_and_ping(self):
        with self.proxy() as p:
            with p.connect() as s:
                s.sendall(handshake(state=1) + frame(b"\x01"))
                self.closed(s)
            with p.connect() as s:
                s.sendall(handshake(state=1) + frame(b"\x00"))
                packet(s)
                s.sendall(frame(b"\x01"))
                self.closed(s)

    def test_partial_handshake_disconnect(self):
        with self.proxy() as p:
            with p.connect() as s:
                s.sendall(b"\x80")
            with p.connect() as s:
                self.login(s)

    def test_handshake_deadline(self):
        with self.proxy("--handshake-timeout-ms", "150") as p, p.connect() as s:
            s.sendall(b"\x80")
            self.closed(s)
            with p.connect() as healthy:
                self.login(healthy)

    def test_status_ping_deadline(self):
        with self.proxy("--handshake-timeout-ms", "200") as p, p.connect() as s:
            s.sendall(handshake(state=1) + frame(b"\x00"))
            packet(s)
            self.closed(s)

    def test_connection_limit(self):
        with self.proxy("--max-connections", "1") as p, p.connect() as first:
            self.login(first)
            with p.connect() as second:
                self.closed(second)
            first.sendall(b"first still works")
            self.assertEqual(receive(first, 17), b"first still works")

    def test_upstream_refused(self):
        with socket.socket() as reserved:
            reserved.bind(("127.0.0.1", 0))
            port = reserved.getsockname()[1]
            # Bound but not listening: a local refusal without contacting external hosts.
            with self.proxy("--upstream-port", str(port)) as p, p.connect() as s:
                s.sendall(handshake())
                self.closed(s)

    @unittest.skipUnless(sys.platform.startswith("linux"), "Linux listen-backlog timeout fixture")
    def test_connect_deadline(self):
        # Fill a local listener's accept queue so the next TCP handshake stalls.
        with socket.socket() as listener:
            listener.bind(("127.0.0.1", 0))
            listener.listen(0)
            port = listener.getsockname()[1]
            with socket.create_connection(("127.0.0.1", port), timeout=2):
                with self.proxy("--upstream-port", str(port), "--connect-timeout-ms", "150") as p, p.connect() as s:
                    started = time.monotonic()
                    s.sendall(handshake())
                    self.closed(s)
                    self.assertGreaterEqual(time.monotonic() - started, 0.10)

    def test_upstream_disconnect(self):
        with socket.socket() as listener:
            listener.bind(("127.0.0.1", 0))
            listener.listen()
            listener.settimeout(3)
            port = listener.getsockname()[1]
            with self.proxy("--upstream-port", str(port)) as p, p.connect() as s:
                s.sendall(handshake())
                upstream, _ = listener.accept()
                with upstream:
                    upstream.settimeout(3)
                    packet(upstream)
                self.closed(s)

    def test_localhost_resolution(self):
        with self.proxy(host="localhost") as p, p.connect() as s:
            self.login(s, expected_host="localhost")

    @unittest.skipUnless(os.name == "posix", "POSIX signal delivery test")
    def test_sigterm_with_active_and_partial_sessions(self):
        p = Proxy(self.upstream_port)
        try:
            with p.connect() as active, p.connect() as partial:
                self.login(active)
                partial.sendall(b"\x80")
                p.stop()
                self.closed(active)
                self.closed(partial)
        finally:
            if p.process.poll() is None:
                p.stop()

    @unittest.skipUnless(os.name == "posix", "POSIX signal delivery test")
    def test_sigint_shutdown(self):
        p = Proxy(self.upstream_port)
        p.stop(signal.SIGINT)


if __name__ == "__main__":
    unittest.main(verbosity=2)
