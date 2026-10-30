"""Audit the final APK before making an owner-installable Android test artifact."""
from pathlib import Path
import hashlib
import json
import os
import re
import shutil
import subprocess
import xml.etree.ElementTree as ET
import zipfile

ROOT = Path(__file__).resolve().parents[1]
PACKAGE = "org.metravolokos.allmusicplayer.spike002a"
ANDROID = "{http://schemas.android.com/apk/res/android}"
FORBIDDEN = re.compile(
    r"com[./]google[./](?:android[./]gms|firebase)|"
    r"com\.google\.gms:|play-services|firebase-", re.I)


def check_manifest(xml):
    manifest = ET.fromstring(xml)
    assert manifest.get("package") == PACKAGE, "Unexpected package identity"
    assert not any(n.tag.startswith("uses-permission") or n.tag == "permission"
                   for n in manifest.iter()), "APK must request/declare zero permissions"
    extra = [(n.tag, n.get(ANDROID + "name")) for n in manifest.iter()
             if n.tag in {"service", "receiver", "provider", "activity-alias"}]
    assert not extra, f"Unexpected Android components: {extra}"
    sdk = manifest.find("uses-sdk")
    assert sdk is not None and sdk.get(ANDROID + "minSdkVersion") == "28"
    assert sdk.get(ANDROID + "targetSdkVersion") == "36"
    application = manifest.find("application")
    assert application is not None
    assert application.get(ANDROID + "allowBackup") == "false"
    assert application.get(ANDROID + "usesCleartextTraffic") == "false"
    activities = application.findall("activity")
    assert len(activities) == 1
    assert activities[0].get(ANDROID + "name") == "org.qtproject.qt.android.bindings.QtActivity"
    assert activities[0].get(ANDROID + "exported") == "true"
    actions = {n.get(ANDROID + "name") for n in manifest.iter("action")}
    categories = {n.get(ANDROID + "name") for n in manifest.iter("category")}
    assert actions == {"android.intent.action.MAIN"}
    assert categories == {"android.intent.category.LAUNCHER"}
    assert not FORBIDDEN.search(xml), "GMS/Firebase manifest reference"
    return manifest


def check_runtime(apk, dependencies):
    assert not FORBIDDEN.search(dependencies), "GMS/Firebase runtime dependency"
    assert "FAILED" not in dependencies, "Unresolved runtime dependency"
    assert "debugRuntimeClasspath" in dependencies, "Missing Gradle dependency evidence"
    with zipfile.ZipFile(apk) as archive:
        names = archive.namelist()
        native = sorted(n for n in names if n.startswith("lib/") and n.endswith(".so"))
        assert native and {n.split("/")[1] for n in native} == {"arm64-v8a"}
        assert any("liballmusic-player" in n for n in native)
        assert any("libQt6Quick" in n for n in native), "Qt runtime must be bundled"
        # Licensing scope is deliberately narrow: a new Qt module needs review.
        allowed_qt = {"Concurrent", "Core", "Gui", "Multimedia", "Network", "OpenGL",
                      "Qml", "QmlMeta", "QmlModels", "QmlWorkerScript", "Quick", "QmlBuiltins"}
        for name in native:
            match = re.search(r"/libQt6(\w+)_arm64-v8a\.so$", name)
            assert not match or match[1] in allowed_qt, f"Review Qt module license: {name}"
            assert not re.search(r"webdav|ffmpeg|avcodec|taglib|sqlite", name, re.I)
        assert any("libQt6Multimedia" in name for name in native), "Qt Multimedia must be bundled"
        assert any("libplugins_multimedia_androidmediaplugin" in name for name in native), \
            "Qt Android media output plugin must be bundled"
        for name in names:
            assert not FORBIDDEN.search(name), f"Forbidden APK entry: {name}"
            assert not name.lower().endswith((".keystore", ".jks", ".testkey", ".pem"))
            if re.fullmatch(r"classes\d*\.dex", name):
                data = archive.read(name)
                assert not FORBIDDEN.search(data.decode("latin-1")), "GMS/Firebase DEX reference"
    return native


def capture(command, path):
    result = subprocess.run(list(map(str, command)), check=False,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    path.write_text(result.stdout, encoding="utf-8")
    if result.returncode:
        print(result.stdout)
        result.check_returncode()
    return result.stdout


def main():
    evidence = ROOT / "build/android-evidence"
    evidence.mkdir(parents=True, exist_ok=True)
    candidates = list((ROOT / "build/android-debug/src/app/android-build/build/outputs/apk/debug").glob("*.apk"))
    assert len(candidates) == 1, f"Expected exactly one debug APK: {candidates}"
    apk = candidates[0]
    with zipfile.ZipFile(apk) as archive:
        (evidence / "apk-entries.txt").write_text("\n".join(archive.namelist()) + "\n")
    sdk = Path(os.environ["ANDROID_HOME"])
    analyzers = sorted((sdk / "cmdline-tools").glob("*/bin/apkanalyzer"))
    assert analyzers, "Android SDK apkanalyzer missing"
    analyzer = Path(shutil.which("apkanalyzer") or analyzers[-1])
    tools = sdk / "build-tools/36.0.0"
    manifest = capture([analyzer, "manifest", "print", apk], evidence / "AndroidManifest.xml")
    print("Final APK manifest:\n" + manifest, flush=True)
    print("APK native entries:\n" + "\n".join(
        line for line in (evidence / "apk-entries.txt").read_text().splitlines()
        if line.startswith("lib/")), flush=True)
    parsed = check_manifest(manifest)
    assert os.environ["BUILD_LABEL"] in parsed.get(ANDROID + "versionName", "")
    assert parsed.get(ANDROID + "versionCode") == os.environ["GITHUB_RUN_NUMBER"]
    capture([tools / "apksigner", "verify", "--verbose", "--print-certs", apk],
            evidence / "signature.txt")
    capture([tools / "zipalign", "-c", "-P", "16", "-v", "4", apk], evidence / "alignment.txt")
    # Also verify 16 KiB ELF LOAD alignment of each bundled native library.
    native = check_runtime(apk, (evidence / "gradle-runtime.txt").read_text())
    readelf = sdk / "ndk/27.2.12479018/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-readelf"
    temporary = ROOT / "build/android-elf-audit"
    temporary.mkdir(parents=True, exist_ok=True)
    elf_evidence = []
    with zipfile.ZipFile(apk) as archive:
        for name in native:
            library = temporary / Path(name).name
            library.write_bytes(archive.read(name))
            headers = subprocess.check_output([str(readelf), "-lW", str(library)], text=True)
            loads = [line.split() for line in headers.splitlines() if line.strip().startswith("LOAD ")]
            assert loads and all(int(line[-1], 16) >= 16384 for line in loads), name
            elf_evidence.append(f"{name}: LOAD alignment >= 16384")
    (evidence / "native-libraries.txt").write_text("\n".join(elf_evidence) + "\n")
    output = ROOT / "dist/android"
    output.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(apk, output / (os.environ["ARTIFACT_NAME"] + ".apk"))
    subprocess.run(["python", str(ROOT / "scripts/collect-licenses.py"),
                    str(ROOT / "build/license-sources"), str(output / "licenses")], check=True)
    for name in ("NOTICE", "NOTICE.toolchain"):
        notice = sdk / "ndk/27.2.12479018" / name
        if notice.is_file():
            shutil.copyfile(notice, output / "licenses" / ("Android-NDK-" + name))
    assert list((output / "licenses").glob("Android-NDK-*")), "NDK runtime notices missing"
    # Preserve copyright/license notices embedded in the resolved Java runtime artifacts.
    cache = Path(os.environ["GRADLE_USER_HOME"]) / "caches/modules-2/files-2.1"
    for path in sorted(cache.rglob("*")):
        if path.suffix not in {".aar", ".jar"} or not zipfile.is_zipfile(path):
            continue
        if not any(part.startswith(("androidx.", "org.jetbrains", "com.google.guava", "org.jspecify")) for part in path.parts):
            continue
        with zipfile.ZipFile(path) as archive:
            for name in archive.namelist():
                if not re.search(r"license|notice|copyright", name, re.I) or name.endswith("/"):
                    continue
                target = output / "licenses/java" / path.parent.parent.name / path.name / name
                assert target.resolve().is_relative_to((output / "licenses").resolve())
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(archive.read(name))
    for name in ("ANDROID-TEST.md", "DEPENDENCIES.md"):
        shutil.copyfile(ROOT / "docs" / name, output / name)
    shutil.copytree(evidence, output / "audit", dirs_exist_ok=True)
    metadata = {"commit": subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
                "build_label": os.environ["BUILD_LABEL"], "abi": "arm64-v8a",
                "run": os.environ["GITHUB_RUN_ID"], "attempt": os.environ["GITHUB_RUN_ATTEMPT"],
                "device_compatibility": "NOT TESTED", "permissions": [],
                "signing": "ephemeral debug certificate; uninstall the previous test copy before replacement",
                "native_libraries": native}
    (output / "BUILD-INFO.json").write_text(json.dumps(metadata, indent=2) + "\n")
    sums = []
    for path in sorted(output.rglob("*")):
        if path.is_file() and path.name != "SHA256SUMS.txt":
            sums.append(f"{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.relative_to(output).as_posix()}")
    (output / "SHA256SUMS.txt").write_text("\n".join(sums) + "\n")
    print("APK audit passed: signature, alignment, ABI, zero permissions and no extra components, no GMS/Firebase references")


if __name__ == "__main__":
    main()

