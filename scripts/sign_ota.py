#!/usr/bin/env python3
"""Sign a Flow.io OTA artifact with an external ECDSA P-256 private key."""

from __future__ import annotations

import argparse
import base64
import subprocess
import tempfile
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("artifact", type=Path)
    parser.add_argument("--private-key", required=True, type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    output = args.output or args.artifact.with_suffix(args.artifact.suffix + ".sig")
    if not args.artifact.is_file() or not args.private_key.is_file():
        parser.error("artifact and private key must exist")

    with tempfile.TemporaryDirectory() as temp_dir:
        der_path = Path(temp_dir) / "signature.der"
        subprocess.run(
            ["openssl", "dgst", "-sha256", "-sign", str(args.private_key),
             "-out", str(der_path), str(args.artifact)],
            check=True,
        )
        output.write_text(base64.b64encode(der_path.read_bytes()).decode("ascii") + "\n",
                          encoding="ascii")
    print(f"signature written: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
