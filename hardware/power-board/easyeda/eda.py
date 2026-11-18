#!/usr/bin/env python3
"""Run JS in the EasyEDA Pro client via the local run-api-gateway bridge."""
import json
import sys
import urllib.request

PORT = 49620


def run(code: str, timeout: int = 600):
    req = urllib.request.Request(
        f"http://127.0.0.1:{PORT}/execute",
        data=json.dumps({"code": code}).encode(),
        headers={"Content-Type": "application/json"},
    )
    raw = urllib.request.urlopen(req, timeout=timeout).read().decode()
    return json.loads(raw)


if __name__ == "__main__":
    src = sys.stdin.read() if len(sys.argv) < 2 else open(sys.argv[1]).read()
    out = run(src)
    print(json.dumps(out, indent=2, ensure_ascii=False))
