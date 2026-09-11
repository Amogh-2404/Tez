#!/usr/bin/env python3
"""Bounded wire-level correctness checks. No load generation or timing scores."""

import argparse
import contextlib
import email.utils
import json
import os
from pathlib import Path
import re
import select
import signal
import socket
import subprocess
import tempfile
import time
import unittest


class Wire:
    def __init__(self, sock):
        self.sock = sock
        self.pending = bytearray()

    def response(self, head=False):
        while b"\r\n\r\n" not in self.pending:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise AssertionError("Connection ended before response headers")
            self.pending.extend(chunk)
            if len(self.pending) > 65536:
                raise AssertionError("Unbounded response headers")
        header, remaining = self.pending.split(b"\r\n\r\n", 1)
        self.pending = bytearray(remaining)
        lines = header.split(b"\r\n")
        version, code, reason = lines[0].decode("ascii").split(" ", 2)
        headers = {}
        for line in lines[1:]:
            key, value = line.decode("latin1").split(":", 1)
            key = key.lower()
            if key in headers:
                raise AssertionError("Duplicate response header: " + key)
            headers[key] = value.strip()
        status = int(code)
        bodyless = head or status in (204, 304) or 100 <= status < 200
        if "transfer-encoding" in headers:
            raise AssertionError("Expected Content-Length response framing")
        if not bodyless and "content-length" not in headers:
            raise AssertionError("Missing response framing")
        size = 0 if bodyless else int(headers["content-length"])
        while len(self.pending) < size:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise AssertionError("Truncated response body")
            self.pending.extend(chunk)
        body = bytes(self.pending[:size])
        del self.pending[:size]
        return version, status, headers, body

    def closed(self):
        if self.pending:
            raise AssertionError("Unexpected bytes after final response")
        try:
            data = self.sock.recv(1)
        except ConnectionResetError:
            return
        if data:
            raise AssertionError("Unexpected response bytes")


class Server:
    def __init__(self, binary, *options):
        self.temp = tempfile.TemporaryDirectory(prefix="tez-http-test-")
        root = Path(self.temp.name)
        (root / "static").mkdir()
        (root / "static" / "sample.txt").write_bytes(b"static contents\n")
        (root / "secret.txt").write_bytes(b"outside root")
        (root / "static" / "link.txt").symlink_to(root / "secret.txt")
        config = {}
        for path, status, body in [
            ("/text", "200 OK", "sample body\n"),
            ("/empty", "204 No Content", ""),
            ("/reset", "205 Reset Content", ""),
            ("/unchanged", "304 Not Modified", ""),
        ]:
            config[path] = {"status": status, "content_type": "text/plain", "body": body}
        (root / "routes.json").write_text(json.dumps(config), encoding="utf-8")
        self.stderr = tempfile.TemporaryFile(mode="w+b")
        self.process = None
        try:
            self.process = subprocess.Popen(
                [binary, "--address", "127.0.0.1", "--port", "0", "--threads", "2",
                 "--config", str(root / "routes.json"), "--static-dir", str(root / "static"),
                 "--timeout", "1", "--body-limit", "1024", *options],
                cwd=root, stdout=subprocess.PIPE, stderr=self.stderr,
            )
            deadline = time.monotonic() + 10
            startup = bytearray()
            while time.monotonic() < deadline:
                if self.process.poll() is not None:
                    raise AssertionError("Server exited at startup: " + self.errors())
                ready, _, _ = select.select([self.process.stdout], [], [], 0.1)
                if not ready:
                    continue
                startup.extend(os.read(self.process.stdout.fileno(), 4096))
                match = re.search(rb"listening on 127\.0\.0\.1:(\d+)", startup)
                if match:
                    self.port = int(match.group(1))
                    return
            raise AssertionError("Server did not become ready: " + self.errors())
        except BaseException:
            self.close(validate=False)
            raise

    def errors(self):
        self.stderr.seek(0)
        return self.stderr.read().decode("utf-8", errors="replace")

    def connect(self):
        sock = socket.create_connection(("127.0.0.1", self.port), timeout=3)
        sock.settimeout(4)
        return sock

    def close(self, validate=True):
        failure = None
        try:
            if self.process is not None:
                if self.process.poll() is None:
                    self.process.terminate()
                    try:
                        self.process.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        self.process.kill()
                        self.process.wait(timeout=5)
                        failure = "Server required forced termination during cleanup"
                diagnostics = self.errors()
                if self.process.returncode != 0:
                    failure = f"Server exited with status {self.process.returncode}: {diagnostics}"
                if re.search(r"AddressSanitizer|UndefinedBehaviorSanitizer|LeakSanitizer|runtime error:", diagnostics):
                    failure = "Sanitizer diagnostics from server: " + diagnostics
        finally:
            if self.process is not None and self.process.stdout:
                self.process.stdout.close()
            self.stderr.close()
            self.temp.cleanup()
        if validate and failure:
            raise AssertionError(failure)

    def __enter__(self):
        return self

    def __exit__(self, exc_type, *_):
        # Preserve the original test failure while still reaping the child.
        self.close(validate=exc_type is None)


class HttpIntegrationTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = Server(BINARY)

    @classmethod
    def tearDownClass(cls):
        cls.server.close()

    def request(self, wire, head=False):
        with self.server.connect() as sock:
            sock.sendall(wire)
            reader = Wire(sock)
            response = reader.response(head=head)
            reader.closed()
            return response

    def test_body_and_pipeline_sent_together(self):
        with self.server.connect() as sock:
            sock.sendall(b"POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nhello"
                         b"GET /health HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n")
            reader = Wire(sock)
            first = reader.response()
            self.assertEqual(first[1], 200)
            self.assertEqual(json.loads(first[3])["received_body"], "hello")
            self.assertEqual(reader.response()[1], 200)
            reader.closed()

    def test_fragmented_request(self):
        with self.server.connect() as sock:
            for part in [b"POST /echo HTTP/1.1\r", b"\nHost: localhost\r\nContent-", b"Length: 3\r\n",
                         b"Connection: close\r\n\r", b"\na", b"b", b"c"]:
                sock.sendall(part)
            reader = Wire(sock)
            self.assertEqual(json.loads(reader.response()[3])["received_body"], "abc")
            reader.closed()

    def test_head_preserves_representation_length_and_pipeline(self):
        with self.server.connect() as sock:
            sock.sendall(b"HEAD /text HTTP/1.1\r\nHost: localhost\r\n\r\n"
                         b"GET /text HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n")
            reader = Wire(sock)
            head = reader.response(head=True)
            get = reader.response()
            self.assertEqual(head[3], b"")
            self.assertEqual(head[2]["content-length"], str(len(get[3])))
            self.assertEqual(get[3], b"sample body\n")
            reader.closed()

    def test_head_errors_also_omit_body(self):
        for path, status in [(b"/missing", 404), (b"/echo", 405)]:
            with self.subTest(path=path):
                response = self.request(b"HEAD " + path + b" HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n", head=True)
                self.assertEqual(response[1], status)
                self.assertEqual(response[3], b"")

    def test_chunked_request_and_following_request(self):
        with self.server.connect() as sock:
            sock.sendall(b"POST /echo HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n"
                         b"2;name=value\r\nab\r\n3\r\ncde\r\n0\r\n\r\n"
                         b"GET /health HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n")
            reader = Wire(sock)
            self.assertEqual(json.loads(reader.response()[3])["received_body"], "abcde")
            self.assertEqual(reader.response()[1], 200)
            reader.closed()

    def test_rejects_malformed_headers_and_ambiguous_framing(self):
        malformed = [
            b"GET / HTTP/1.1\r\n\r\n",
            b"GET / HTTP/1.1\r\nHost: a\r\nHost: b\r\n\r\n",
            b"GET / HTTP/1.1\r\nHost : localhost\r\n\r\n",
            b"GET / HTTP/1.1\r\nHost: a b\r\n\r\n",
            b"GET /bad%xy HTTP/1.1\r\nHost: localhost\r\n\r\n",
            b"GET / HTTP/1.1 extra\r\nHost: localhost\r\n\r\n",
            b"POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 1x\r\n\r\nx",
            b"POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\nContent-Length: 0\r\n\r\n",
            b"POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n",
            b"POST /echo HTTP/1.0\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n",
        ]
        for wire in malformed:
            with self.subTest(wire=wire):
                response = self.request(wire)
                self.assertEqual(response[1], 400)
                self.assertEqual(response[2]["connection"], "close")

    def test_uri_punctuation_validation_preserves_ipv6_authorities(self):
        for ch in b'<>"{}[]^`|':
            for prefix in [b"/health?x=", b"/health", b"http://[::1]?x="]:
                target = prefix + bytes([ch])
                with self.subTest(target=target):
                    response = self.request(b"GET " + target + b" HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n")
                    self.assertEqual(response[1], 400)
        for target in [b"http://[::1]/health?x=%5B1%5D", b"/health?q=!$&'()*+,;=:@/?x=%22"]:
            with self.subTest(target=target):
                response = self.request(b"GET " + target + b" HTTP/1.1\r\nHost: [::1]\r\nConnection: close\r\n\r\n")
                self.assertEqual(response[1], 200)

    def test_unsupported_version_uses_supported_response_version(self):
        for version in [b"HTTP/1.2", b"HTTP/2.0"]:
            with self.subTest(version=version):
                response = self.request(b"GET /health " + version + b"\r\nHost: localhost\r\n\r\n")
                self.assertEqual(response[0], "HTTP/1.1")
                self.assertEqual(response[1], 505)

    def test_forbidden_trailers_cannot_change_request_semantics(self):
        for field in [b"Host: another", b"Content-Length: 1", b"Transfer-Encoding: chunked",
                      b"Connection: close", b"Authorization: secret", b"Content-Type: text/plain"]:
            with self.subTest(field=field):
                response = self.request(b"POST /echo HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n"
                                        b"1\r\na\r\n0\r\n" + field + b"\r\n\r\n")
                self.assertEqual(response[1], 400)

    def test_header_limit(self):
        response = self.request(b"GET / HTTP/1.1\r\nHost: localhost\r\nX-Large: " + b"a" * 9000 + b"\r\n\r\n")
        self.assertEqual(response[1], 431)

    def test_content_length_limit_is_checked_before_body(self):
        response = self.request(b"POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 1025\r\nExpect: 100-continue\r\n\r\n")
        self.assertEqual(response[1], 413)

    def test_chunked_body_limit(self):
        response = self.request(b"POST /echo HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n401\r\n" + b"a" * 1025 + b"\r\n0\r\n\r\n")
        self.assertEqual(response[1], 413)

    def test_expect_continue_then_body(self):
        with self.server.connect() as sock:
            sock.sendall(b"POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\nExpect: 100-continue\r\nConnection: close\r\n\r\n")
            reader = Wire(sock)
            self.assertEqual(reader.response()[1], 100)
            sock.sendall(b"hello")
            self.assertEqual(json.loads(reader.response()[3])["received_body"], "hello")
            reader.closed()

    def test_unsupported_expectation(self):
        response = self.request(b"POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\nExpect: custom\r\n\r\n")
        self.assertEqual(response[1], 417)

    def test_unsupported_transfer_coding(self):
        response = self.request(b"POST /echo HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: gzip, chunked\r\n\r\n0\r\n\r\n")
        self.assertEqual(response[1], 501)

    def test_http10_and_connection_tokens(self):
        response = self.request(b"GET /health HTTP/1.0\r\n\r\n")
        self.assertEqual(response[0], "HTTP/1.0")
        self.assertEqual(response[1], 200)
        response = self.request(b"GET /health HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive, ClOsE\r\n\r\n")
        self.assertEqual(response[2]["connection"], "close")
        with self.server.connect() as sock:
            sock.sendall(b"GET /health HTTP/1.0\r\nConnection: keep-alive\r\n\r\n"
                         b"GET /health HTTP/1.0\r\nConnection: close\r\n\r\n")
            reader = Wire(sock)
            self.assertEqual(reader.response()[2]["connection"], "keep-alive")
            self.assertEqual(reader.response()[1], 200)
            reader.closed()

    def test_incomplete_body_is_not_dispatched(self):
        with self.server.connect() as sock:
            sock.sendall(b"POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nab")
            sock.shutdown(socket.SHUT_WR)
            reader = Wire(sock)
            self.assertEqual(reader.response()[1], 400)
            reader.closed()

    def test_idle_header_and_body_deadlines_close_connections(self):
        for partial in [b"", b"GET /health HTTP/1.1\r\nHost:",
                        b"POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\na"]:
            with self.subTest(partial=partial), self.server.connect() as sock:
                if partial:
                    sock.sendall(partial)
                Wire(sock).closed()

    def test_absolute_form_query_and_security_headers(self):
        response = self.request(b"GET http://localhost/health?x=1 HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n")
        self.assertEqual(response[1], 200)
        self.assertEqual(response[2]["x-content-type-options"], "nosniff")
        self.assertIsNotNone(email.utils.parsedate_to_datetime(response[2]["date"]))

    def test_bodyless_statuses_do_not_desynchronize_pipeline(self):
        with self.server.connect() as sock:
            sock.sendall(b"GET /empty HTTP/1.1\r\nHost: localhost\r\n\r\n"
                         b"GET /reset HTTP/1.1\r\nHost: localhost\r\n\r\n"
                         b"GET /unchanged HTTP/1.1\r\nHost: localhost\r\n\r\n"
                         b"GET /text HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n")
            reader = Wire(sock)
            empty = reader.response()
            self.assertEqual(empty[1], 204)
            self.assertNotIn("content-length", empty[2])
            reset = reader.response()
            self.assertEqual(reset[1], 205)
            self.assertEqual(reset[2]["content-length"], "0")
            unchanged = reader.response()
            self.assertEqual(unchanged[1], 304)
            self.assertNotIn("content-length", unchanged[2])
            self.assertEqual(reader.response()[3], b"sample body\n")
            reader.closed()

    def test_static_get_head_methods_and_containment(self):
        get = self.request(b"GET /static/sample.txt?v=1 HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n")
        self.assertEqual(get[3], b"static contents\n")
        head = self.request(b"HEAD /static/sample.txt HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n", head=True)
        self.assertEqual(head[2]["content-length"], str(len(get[3])))
        invalid_method = self.request(b"POST /static/sample.txt HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n")
        self.assertEqual(invalid_method[1], 405)
        self.assertEqual(invalid_method[2]["allow"], "GET, HEAD")
        for path in [b"/static/../secret.txt", b"/static/%2e%2e/secret.txt", b"/static/link.txt",
                     b"/static/%2fetc/passwd", b"/static/sample.txt%00"]:
            with self.subTest(path=path):
                response = self.request(b"GET " + path + b" HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n")
                self.assertIn(response[1], (400, 403, 404))
                self.assertNotIn(b"outside root", response[3])

    def test_keep_alive_request_cap_announces_final_close(self):
        # This fixed boundary check is protocol coverage, not a throughput test.
        with self.server.connect() as sock:
            sock.sendall(b"GET /health HTTP/1.1\r\nHost: localhost\r\n\r\n" * 1000)
            reader = Wire(sock)
            for index in range(1000):
                response = reader.response()
                self.assertEqual(response[1], 200)
                if index == 999:
                    self.assertEqual(response[2]["connection"], "close")
            reader.closed()

    def test_connection_capacity_is_bounded(self):
        with Server(BINARY, "--max-connections", "1", "--timeout", "10") as server:
            with server.connect() as first:
                first.sendall(b"GET /health HTTP/1.1\r\nHost: localhost\r\n\r\n")
                reader = Wire(first)
                self.assertEqual(reader.response()[1], 200)
                with server.connect() as excess:
                    Wire(excess).closed()
                first.sendall(b"GET /health HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n")
                self.assertEqual(reader.response()[1], 200)
                reader.closed()

    def test_signal_shutdown_does_not_wait_for_slow_clients(self):
        with Server(BINARY, "--timeout", "30") as server:
            with contextlib.ExitStack() as stack:
                clients = [stack.enter_context(server.connect()) for _ in range(3)]
                clients[1].sendall(b"GET /health HTTP/1.1\r\nHost:")
                clients[2].sendall(b"POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\na")
                server.process.send_signal(signal.SIGTERM)
                self.assertEqual(server.process.wait(timeout=3), 0)

    def test_cli_rejects_invalid_options_and_reports_help(self):
        for args in [["--threads", "0"], ["--port", "65536"], ["--timeout", "1x"],
                     ["--body-limit", "0"], ["--unknown"], ["--port"],
                     ["--config", "/nonexistent/tez/config.json"]]:
            with self.subTest(args=args):
                result = subprocess.run([BINARY, *args], capture_output=True, timeout=5)
                self.assertNotEqual(result.returncode, 0)
        self.assertEqual(subprocess.run([BINARY, "--help"], capture_output=True, timeout=5).returncode, 0)
        self.assertEqual(subprocess.run([BINARY, "--version"], capture_output=True, timeout=5).returncode, 0)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--server", required=True)
    args, remaining = parser.parse_known_args()
    BINARY = str(Path(args.server).resolve(strict=True))
    unittest.main(argv=[__file__, *remaining], verbosity=2)
