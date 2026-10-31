#!/usr/bin/env python3
"""Build the pinned OpenSSL 3 Android arm64 runtime used by Qt TLS."""
import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import urllib.request


VERSION = "3.5.9"
SHA256 = "603f5602e2eef00d77fbd429d34dcd5822bb301757a1bc9cdb24c670f1eb859a"
URL = f"https://github.com/openssl/openssl/releases/download/openssl-{VERSION}/openssl-{VERSION}.tar.gz"


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
    if not source.exists():
        with tarfile.open(archive, "r:gz") as bundle:
            bundle.extractall(args.work, filter="data")
    if not (source / "Configure").is_file() or not (source / "LICENSE.txt").is_file():
        raise SystemExit("Pinned OpenSSL source archive is incomplete")

    ndk = args.ndk.resolve()
    toolchain = ndk / "toolchains/llvm/prebuilt/linux-x86_64/bin"
    if not (toolchain / "aarch64-linux-android28-clang").is_file():
        raise SystemExit(f"Android NDK arm64 API 28 compiler missing: {toolchain}")
    env = os.environ.copy()
    env["ANDROID_NDK_ROOT"] = str(ndk)
    env["PATH"] = str(toolchain) + os.pathsep + env.get("PATH", "")
    env["CFLAGS"] = "-O2 -fPIC -Wl,-z,max-page-size=16384"
    env["LDFLAGS"] = "-Wl,-z,max-page-size=16384"
    run(["./Configure", "shared", "android-arm64", "-D__ANDROID_API__=28",
         f"--prefix={args.work / 'install'}", "no-tests"], cwd=source, env=env)
    run(["make", f"-j{args.jobs}", "SHLIB_VERSION_NUMBER=", "build_libs"], cwd=source, env=env)

    args.output.mkdir(parents=True, exist_ok=True)
    crypto = source / "libcrypto.so"
    ssl = source / "libssl.so"
    if not crypto.is_file() or not ssl.is_file():
        raise SystemExit("OpenSSL build did not produce both shared libraries")
    crypto_out = args.output / "libcrypto_3.so"
    ssl_out = args.output / "libssl_3.so"
    shutil.copyfile(crypto, crypto_out)
    shutil.copyfile(ssl, ssl_out)
    run(["patchelf", "--set-soname", "libcrypto_3.so", crypto_out])
    run(["patchelf", "--set-soname", "libssl_3.so", ssl_out])
    run(["patchelf", "--replace-needed", "libcrypto.so", "libcrypto_3.so", ssl_out])
    shutil.copyfile(source / "LICENSE.txt", args.output / "OpenSSL-LICENSE.txt")
    (args.output / "OPENSSL-SOURCE.txt").write_text(
        f"OpenSSL {VERSION}\nSource: {URL}\nSHA-256: {SHA256}\n"
        "License: Apache License 2.0 (LICENSE.txt from the verified source archive)\n"
        "Target: Android arm64-v8a, API 28, shared, NDK 27.2.12479018\n",
        encoding="utf-8")


if __name__ == "__main__":
    main()
