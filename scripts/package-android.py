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
PACKAGE = "org.metravolokos.allmusicplayer.spike002dmedia3"
ANDROID = "{http://schemas.android.com/apk/res/android}"
FORBIDDEN = re.compile(
    r"com[./]google[./](?:android[./]gms|firebase)|"
    r"com\.google\.gms:|play-services|firebase-|"
    r"ffmpeg|avcodec|avformat|avutil|swresample|swscale", re.I)
GOOGLE_FORBIDDEN = re.compile(
    r"com[./]google[./](?:android[./]gms|firebase)|"
    r"com\.google\.gms:|play-services|firebase-", re.I)
MEDIA3_VERSION = "1.11.1"
EXPECTED_ASSETS = {
    "assets/audio/synthetic-440hz.mp3",
    "assets/audio/synthetic-660hz.flac",
    "assets/audio/synthetic-880hz.wav",
}
MAVEN_LICENSE_OVERRIDES = {
    ("com.google.guava", "guava", "33.3.1-android"):
        "Apache License, Version 2.0 (Google Guava v33.3.1 upstream POM: "
        "https://github.com/google/guava/blob/v33.3.1/pom.xml)",
    ("com.google.guava", "failureaccess", "1.0.2"):
        "Apache License, Version 2.0 (Google Guava v33.3.1 source: "
        "https://github.com/google/guava/blob/v33.3.1/futures/failureaccess/src/"
        "com/google/common/util/concurrent/internal/InternalFutureFailureAccess.java)",
    ("com.google.guava", "listenablefuture", "9999.0-empty-to-avoid-conflict-with-guava"):
        "Apache License, Version 2.0 (empty artifact POM inherits Google Guava parent "
        "26.0-android license: https://repo.maven.apache.org/maven2/com/google/guava/"
        "guava-parent/26.0-android/guava-parent-26.0-android.pom)",
}


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
    assert not FORBIDDEN.search(dependencies), "Forbidden runtime dependency"
    assert "FAILED" not in dependencies, "Unresolved runtime dependency"
    assert "debugRuntimeClasspath" in dependencies, "Missing Gradle dependency evidence"
    assert f"androidx.media3:media3-exoplayer:{MEDIA3_VERSION}" in dependencies, \
        "Pinned Media3 ExoPlayer runtime dependency missing"
    assert "media3-decoder-ffmpeg" not in dependencies.lower(), \
        "Media3 FFmpeg extension must not be packaged"
    with zipfile.ZipFile(apk) as archive:
        names = archive.namelist()
        missing_assets = EXPECTED_ASSETS.difference(names)
        assert not missing_assets, f"Synthetic Android test assets missing: {sorted(missing_assets)}"
        native = sorted(n for n in names if n.startswith("lib/") and n.endswith(".so"))
        assert native and {n.split("/")[1] for n in native} == {"arm64-v8a"}
        assert any("liballmusic-player" in n for n in native)
        assert any("libQt6Quick" in n for n in native), "Qt runtime must be bundled"
        # Licensing scope is deliberately narrow: a new Qt module needs review.
        allowed_qt = {"Concurrent", "Core", "Gui", "Network", "OpenGL",
                      "Qml", "QmlMeta", "QmlModels", "QmlWorkerScript", "Quick", "QmlBuiltins"}
        for name in native:
            match = re.search(r"/libQt6(\w+)_arm64-v8a\.so$", name)
            assert not match or match[1] in allowed_qt, f"Review Qt module license: {name}"
            assert not re.search(r"webdav|ffmpeg|avcodec|avformat|taglib|sqlite|multimedia", name, re.I)
        assert not any("libQt6Multimedia" in name for name in native), \
            "Qt Multimedia must not be bundled on Android"
        assert not any("plugins_multimedia" in name.lower() for name in native), \
            "Qt multimedia plugins must not be bundled on Android"
        dex_data = b""
        for name in names:
            assert not FORBIDDEN.search(name), f"Forbidden APK entry: {name}"
            assert not name.lower().endswith((".keystore", ".jks", ".testkey", ".pem"))
            if re.fullmatch(r"classes\d*\.dex", name):
                data = archive.read(name)
                # DefaultRenderersFactory reflectively probes the optional FFmpeg renderer by
                # class-name string. Reject the extension's actual DEX class descriptor below.
                assert not GOOGLE_FORBIDDEN.search(data.decode("latin-1")), \
                    "Forbidden GMS/Firebase runtime reference in DEX"
                dex_data += data
        assert b"androidx/media3/exoplayer" in dex_data, "Media3 ExoPlayer classes missing from APK"
        assert b"Media3PlaybackAdapter" in dex_data, "Qt JNI adapter missing from APK"
        assert b"Landroidx/media3/decoder/ffmpeg/" not in dex_data, \
            "Media3 FFmpeg decoder classes must not be packaged"
    return native


def runtime_coordinates(dependencies):
    pattern = re.compile(
        r"([A-Za-z0-9_.-]+):([A-Za-z0-9_.-]+):([^\s()]+)(?:\s+->\s+([^\s()]+))?")
    coordinates = set()
    for line in dependencies.splitlines():
        match = pattern.search(line)
        if match:
            group, artifact, requested, selected = match.groups()
            coordinates.add((group, artifact, selected or requested))
    return sorted(coordinates)


def audit_maven_licenses(dependencies, gradle_home, output):
    """Require recognizable license metadata for every resolved runtime artifact."""
    namespace = {"m": "http://maven.apache.org/POM/4.0.0"}
    root = Path(gradle_home) / "caches/modules-2/files-2.1"
    records = []
    for group, artifact, version in runtime_coordinates(dependencies):
        artifact_root = root / group / artifact / version
        poms = sorted(artifact_root.rglob("*.pom")) if artifact_root.is_dir() else []
        assert poms, f"Maven POM missing from Gradle cache: {group}:{artifact}:{version}"
        pom = ET.parse(poms[0]).getroot()
        licenses = []
        for license_node in pom.findall("m:licenses/m:license", namespace):
            name = (license_node.findtext("m:name", default="", namespaces=namespace)).strip()
            url = (license_node.findtext("m:url", default="", namespaces=namespace)).strip()
            label = name or url
            assert label, f"Unnamed Maven license: {group}:{artifact}:{version}"
            normalized = label.lower()
            assert any(token in normalized for token in ("apache", "mit", "bsd")), \
                f"License needs manual review: {group}:{artifact}:{version}: {label} ({url})"
            licenses.append(f"{label} ({url})" if url else label)
        if not licenses:
            # These exact Guava artifacts omit license metadata; pinned upstream
            # POMs or source declare Apache-2.0. Keep each exception version-specific.
            override = MAVEN_LICENSE_OVERRIDES.get((group, artifact, version))
            assert override, f"Maven POM has no declared license: {group}:{artifact}:{version}"
            licenses.append(override)
        records.append(f"{group}:{artifact}:{version} — {'; '.join(sorted(set(licenses)))}")

    media3_license = next((record for record in records
                           if record.startswith(f"androidx.media3:media3-exoplayer:{MEDIA3_VERSION} —")), None)
    assert media3_license and "apache" in media3_license.lower(), \
        "Media3 ExoPlayer must declare Apache License 2.0"
    report = Path(output) / "licenses/java/DEPENDENCY-LICENSES.txt"
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text("\n".join(records) + "\n", encoding="utf-8")
    return records


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
    media3_license = ROOT / "licenses/Media3-1.11.1-Apache-2.0.txt"
    assert media3_license.is_file(), "Media3 Apache-2.0 license text missing"
    java_license_dir = output / "licenses/java"
    java_license_dir.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(media3_license, java_license_dir / "Media3-1.11.1-Apache-2.0.txt")
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
    for name in ("ANDROID-MEDIA3-SPIKE-TEST.md", "DEPENDENCIES.md"):
        shutil.copyfile(ROOT / "docs" / name, output / name)
    shutil.copytree(evidence, output / "audit", dirs_exist_ok=True)
    dependency_licenses = audit_maven_licenses(
        (evidence / "gradle-runtime.txt").read_text(encoding="utf-8"),
        os.environ["GRADLE_USER_HOME"], output)
    metadata = {"commit": subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
                "build_label": os.environ["BUILD_LABEL"], "abi": "arm64-v8a",
                "run": os.environ["GITHUB_RUN_ID"], "attempt": os.environ["GITHUB_RUN_ATTEMPT"],
                "device_compatibility": "NOT TESTED", "permissions": [],
                "package": PACKAGE, "media3_version": MEDIA3_VERSION,
                "apk_bytes": apk.stat().st_size,
                "java_runtime_licenses": dependency_licenses,
                "signing": "ephemeral debug certificate; uninstall the previous test copy before replacement",
                "native_libraries": native}
    (output / "BUILD-INFO.json").write_text(json.dumps(metadata, indent=2) + "\n")
    sums = []
    for path in sorted(output.rglob("*")):
        if path.is_file() and path.name != "SHA256SUMS.txt":
            sums.append(f"{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.relative_to(output).as_posix()}")
    (output / "SHA256SUMS.txt").write_text("\n".join(sums) + "\n")
    print(f"APK audit passed: {apk.stat().st_size} bytes; signature, alignment, arm64-v8a, "
          "zero permissions, no Qt Multimedia/FFmpeg, Media3 runtime, and Maven license metadata")


if __name__ == "__main__":
    main()

