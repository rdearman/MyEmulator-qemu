#!/usr/bin/env python3
"""Small HTTP file drop for moving tar archives to/from a REM guest."""

import argparse
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path
from urllib.parse import unquote, urlsplit
import os
import tempfile


class TransferHandler(BaseHTTPRequestHandler):
    server_version = "REMTransfer/1.0"

    def target(self):
        parts = urlsplit(self.path)
        prefix = "/files/"
        if parts.query or not parts.path.startswith(prefix):
            return None
        name = unquote(parts.path[len(prefix):])
        if not name or name in (".", "..") or "/" in name or "\\" in name:
            return None
        return self.server.root / name

    def do_HEAD(self):
        target = self.target()
        if target is None or not target.is_file():
            self.send_error(404)
            return
        self.send_response(200)
        self.send_header("Content-Length", str(target.stat().st_size))
        self.end_headers()

    def do_GET(self):
        target = self.target()
        if target is None or not target.is_file():
            self.send_error(404)
            return
        size = target.stat().st_size
        self.send_response(200)
        self.send_header("Content-Length", str(size))
        self.send_header("Content-Type", "application/octet-stream")
        self.end_headers()
        with target.open("rb") as source:
            while True:
                block = source.read(1024 * 1024)
                if not block:
                    break
                self.wfile.write(block)

    def do_PUT(self):
        target = self.target()
        if target is None:
            self.send_error(400, "use /files/<single-filename>")
            return
        try:
            remaining = int(self.headers.get("Content-Length", ""))
        except ValueError:
            self.send_error(411, "Content-Length required")
            return
        if remaining < 0:
            self.send_error(400, "invalid Content-Length")
            return

        fd, temporary = tempfile.mkstemp(prefix=".rem-transfer-", dir=self.server.root)
        try:
            with os.fdopen(fd, "wb") as destination:
                while remaining:
                    block = self.rfile.read(min(1024 * 1024, remaining))
                    if not block:
                        raise ConnectionError("incomplete request body")
                    destination.write(block)
                    remaining -= len(block)
                destination.flush()
                os.fsync(destination.fileno())
            os.replace(temporary, target)
        except Exception:
            try:
                os.unlink(temporary)
            except FileNotFoundError:
                pass
            self.send_error(400, "upload did not complete")
            return

        self.send_response(201)
        self.send_header("Content-Length", "0")
        self.end_headers()

    def log_message(self, fmt, *args):
        print("%s - %s" % (self.address_string(), fmt % args), flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bind", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--root", type=Path, required=True,
                        help="directory containing transferable archive files")
    args = parser.parse_args()
    args.root.mkdir(parents=True, exist_ok=True)
    root = args.root.resolve()
    server = HTTPServer((args.bind, args.port), TransferHandler)
    server.root = root
    print(f"REM transfer ready at http://{args.bind}:{args.port}/files/; root={root}",
          flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
