#!/usr/bin/env python3
"""Build the pinned OpenSSL 3 Android arm64 runtime used by Qt TLS."""
import argparse
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import tarfile
import urllib.request


VERSION = "3.5.9"
SHA256 = "603f5602e2eef00d77fbd429d34dcd5822bb301757a1bc9cdb24c670f1eb859a"
URL = f"https://github.com/openssl/openssl/releases/download/openssl-{VERSION}/openssl-{VERSION}.tar.gz"


def configure_android_arm64_variant(config_text):
    """Set OpenSSL's arm64 shared-library suffix before Configure generates ELF files."""
    stanza = re.compile(r'(?ms)(    "android-arm64"\s*=>\s*\{.*?)(\n    \},)')
    matches = list(stanza.finditer(config_text))
    if len(matches) != 1:
        raise ValueError("Expected exactly one android-arm64 OpenSSL configuration stanza")
    match = matches[0]
    body = match.group(1)
    variants = re.findall(r'^\s*shlib_variant\s*=>\s*[\'\"]([^\'\"]*)[\'\"]\s*,',
                          body, re.MULTILINE)
    if variants == ["_3"]:
        return config_text
    if variants:
        raise ValueError(f"Unexpected android-arm64 shlib_variant: {variants}")
    insertion = re.search(r'^(\s*perlasm_scheme\s*=>[^\n]*)', body, re.MULTILINE)
    if not insertion:
        raise ValueError("android-arm64 OpenSSL stanza has no expected perlasm_scheme anchor")
    body = body[:insertion.end()] + '\n        shlib_variant   => "_3",' + body[insertion.end():]
    return config_text[:match.start(1)] + body + config_text[match.end(1):]


def run(command, *, cwd=None, env=None):
    subprocess.run(list(map(str, command)), cwd=cwd, env=env, check=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--ndk", required=True, type=Path)
    parser.add_argument("--work", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--jobs", type=int, default=2)
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")

    args.work = args.work.resolve()
    args.output = args.output.resolve()
    args.work.mkdir(parents=True, exist_ok=True)
    archive = args.work / f"openssl-{VERSION}.tar.gz"
    if not archive.exists():
        request = urllib.request.Request(URL, headers={"User-Agent": "AllMusic-Android-build"})
        with urllib.request.urlopen(request, timeout=90) as response, archive.open("wb") as target:
            shutil.copyfileobj(response, target)
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    if digest != SHA256:
        raise SystemExit(f"OpenSSL source SHA-256 mismatch: {digest}")

    source = args.work / f"openssl-{VERSION}"
    if source.exists():
        shutil.rmtree(source)
    with tarfile.open(archive, "r:gz") as bundle:
        bundle.extractall(args.work, filter="data")
    if not (source / "Configure").is_file() or not (source / "LICENSE.txt").is_file():
        raise SystemExit("Pinned OpenSSL source archive is incomplete")
    android_config = source / "Configurations/15-android.conf"
    if not android_config.is_file():
        raise SystemExit("Pinned OpenSSL Android configuration is missing")
    try:
        original_config = android_config.read_text(encoding="utf-8")
        patched_config = configure_android_arm64_variant(original_config)
    except (OSError, ValueError) as error:
        raise SystemExit(f"Could not safely configure OpenSSL arm64 suffix: {error}") from error
    android_config.write_text(patched_config, encoding="utf-8")

    ndk = args.ndk.resolve()
    toolchain = ndk / "toolchains/llvm/prebuilt/linux-x86_64/bin"
    if not (toolchain / "aarch64-linux-android28-clang").is_file():
        raise SystemExit(f"Android NDK arm64 API 28 compiler missing: {toolchain}")
    env = os.environ.copy()
    env["ANDROID_NDK_ROOT"] = str(ndk)
    env["PATH"] = str(toolchain) + os.pathsep + env.get("PATH", "")
    env["CFLAGS"] = "-O2 -fPIC"
    env["LDFLAGS"] = "-Wl,-z,max-page-size=16384"
    run(["./Configure", "shared", "android-arm64", "-D__ANDROID_API__=28",
         f"--prefix={args.work / 'install'}", "no-tests"], cwd=source, env=env)
    run(["make", f"-j{args.jobs}", "SHLIB_VERSION_NUMBER=", "build_libs"], cwd=source, env=env)

    args.output.mkdir(parents=True, exist_ok=True)
    crypto = source / "libcrypto_3.so"
    ssl = source / "libssl_3.so"
    if not crypto.is_file() or not ssl.is_file():
        raise SystemExit("OpenSSL shlib_variant=_3 build did not produce suffixed shared libraries")
    crypto_out = args.output / "libcrypto_3.so"
    ssl_out = args.output / "libssl_3.so"
    shutil.copyfile(crypto, crypto_out)
    shutil.copyfile(ssl, ssl_out)
    shutil.copyfile(source / "LICENSE.txt", args.output / "OpenSSL-LICENSE.txt")
    (args.output / "OPENSSL-SOURCE.txt").write_text(
        f"OpenSSL {VERSION}\nSource: {URL}\nSHA-256: {SHA256}\n"
        "License: Apache License 2.0 (LICENSE.txt from the verified source archive)\n"
        "Android arm64 suffix: shlib_variant=_3, set in source configuration before Configure\n"
        "Target: Android arm64-v8a, API 28, shared, NDK 27.2.12479018\n",
        encoding="utf-8")


if __name__ == "__main__":
    main()
