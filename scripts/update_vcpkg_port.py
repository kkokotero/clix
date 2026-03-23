#!/usr/bin/env python3

"""Update the local vcpkg port metadata for a CLIX release."""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import re
import sys
import urllib.request


ROOT = pathlib.Path(__file__).resolve().parent.parent
PORT_MANIFEST = ROOT / "ports" / "clix" / "vcpkg.json"
PORTFILE = ROOT / "ports" / "clix" / "portfile.cmake"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", required=True, help="Port version to publish.")
    parser.add_argument(
        "--ref",
        help="Git ref used by the port. Defaults to v<version>.",
    )
    parser.add_argument(
        "--repo",
        default="kkokotero/clix",
        help="GitHub repository in owner/name form.",
    )
    return parser.parse_args()


def fetch_sha512(url: str) -> str:
    with urllib.request.urlopen(url) as response:
        payload = response.read()
    return hashlib.sha512(payload).hexdigest()


def update_manifest(version: str) -> None:
    manifest = json.loads(PORT_MANIFEST.read_text(encoding="utf-8"))
    manifest["version-semver"] = version
    PORT_MANIFEST.write_text(
        json.dumps(manifest, indent=2) + "\n",
        encoding="utf-8",
    )


def update_portfile(repo: str, ref: str, sha512: str) -> None:
    content = PORTFILE.read_text(encoding="utf-8")
    content = re.sub(
        r"(\n\s*REPO\s+)([^\n]+)",
        lambda match: f"{match.group(1)}{repo}",
        content,
        count=1,
    )
    content = re.sub(
        r"(\n\s*REF\s+)([^\n]+)",
        lambda match: f"{match.group(1)}{ref}",
        content,
        count=1,
    )
    content = re.sub(
        r"(\n\s*SHA512\s+)([^\n]+)",
        lambda match: f"{match.group(1)}{sha512}",
        content,
        count=1,
    )
    PORTFILE.write_text(content, encoding="utf-8")


def main() -> int:
    args = parse_args()
    ref = args.ref or f"v{args.version}"
    archive_url = f"https://github.com/{args.repo}/archive/{ref}.tar.gz"

    try:
        sha512 = fetch_sha512(archive_url)
    except Exception as exc:  # pragma: no cover - release automation path
        print(f"failed to download {archive_url}: {exc}", file=sys.stderr)
        return 1

    update_manifest(args.version)
    update_portfile(args.repo, ref, sha512)

    print(f"updated vcpkg port to version {args.version}")
    print(f"ref: {ref}")
    print(f"sha512: {sha512}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
