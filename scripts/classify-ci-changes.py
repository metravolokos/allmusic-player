"""Fail-closed path classifier shared by the Desktop and Android workflows."""
from pathlib import PurePosixPath
import os
import re
import subprocess
import sys


DOC_EXTENSIONS = {".md", ".markdown", ".mdown", ".mkd"}
SOURCE_EXTENSIONS = {".cpp", ".h"}
ANDROID_CONFIG_EXTENSIONS = {".gradle", ".java", ".kt", ".properties", ".xml"}
VALID_SHA = re.compile(r"(?:[0-9a-fA-F]{40}|[0-9a-fA-F]{64})\Z")


def full_check(reason, ci_rules_changed=False):
    return {
        "desktop_linux": True,
        "desktop_windows": True,
        "fixtures": True,
        "android": True,
        "android_fast_checks": False,
        "windows_zip": True,
        "webdav_test_kit": True,
        "desktop_cache_validation": True,
        "android_cache_validation": True,
        "ci_mode": "full-fail-closed",
        "affected_platforms": "Linux Desktop, Windows Desktop, Android",
        "ci_rules_changed": ci_rules_changed,
        "reason": reason,
    }


def platform_summary(selection):
    targets = []
    if selection["desktop_linux"]:
        targets.append("Linux Desktop")
    if selection["desktop_windows"]:
        targets.append("Windows Desktop")
    if selection["android"]:
        targets.append("Android")
    if selection["windows_zip"]:
        targets.append("Windows package audit")
    if selection["webdav_test_kit"]:
        targets.append("WebDAV package audit")
    return ", ".join(targets) if targets else "только лёгкие проверки документов и политики CI"


def classify_paths(paths):
    """Return affected targets. Unknown, empty, or malformed paths run everything."""
    normalized = [str(path).replace("\\", "/") for path in paths]
    if not normalized or any(
            not path or path.startswith("/") or ".." in PurePosixPath(path).parts
            for path in normalized):
        return full_check("не удалось надёжно определить изменённые файлы")
    if all(PurePosixPath(path).suffix.lower() in DOC_EXTENSIONS for path in normalized):
        return {
            "desktop_linux": False,
            "desktop_windows": False,
            "fixtures": False,
            "android": False,
            "android_fast_checks": False,
            "windows_zip": False,
            "webdav_test_kit": False,
            "desktop_cache_validation": False,
            "android_cache_validation": False,
            "reason": "изменены только Markdown-документы",
        }

    result = {
        "desktop_linux": False,
        "desktop_windows": False,
        "fixtures": False,
        "android": False,
        "android_fast_checks": False,
        "windows_zip": False,
        "webdav_test_kit": False,
        "desktop_cache_validation": False,
        "android_cache_validation": False,
        "reason": "изменён код или конфигурация",
    }
    recognized = True
    classifier_change = False
    workflow_change = False

    def desktop():
        result["desktop_linux"] = True
        result["desktop_windows"] = True

    def windows():
        result["desktop_windows"] = True

    for path in normalized:
        suffix = PurePosixPath(path).suffix.lower()
        if suffix in DOC_EXTENSIONS:
            continue
        if path in {"CMakeLists.txt", "CMakePresets.json", "src/app/CMakeLists.txt"}:
            desktop()
            result["android"] = True
            result["windows_zip"] = True
            result["webdav_test_kit"] = True
        elif path.startswith(".github/workflows/"):
            workflow_change = True
        elif path in {"scripts/classify-ci-changes.py", "scripts/check_repository.py",
                      "scripts/ci_result.py"} \
                or path.startswith("tests/ci/"):
            classifier_change = True
        elif path in {"scripts/package-android.py", "scripts/build-android-openssl.py",
                      "scripts/generate_android_media3_fixtures.py"}:
            result["android"] = True
            result["android_fast_checks"] = True
        elif path.startswith("ui/android/") and suffix == ".qml":
            result["android"] = True
            result["android_fast_checks"] = True
        elif path.startswith("android/") and suffix in (ANDROID_CONFIG_EXTENSIONS | {".mp3", ".flac", ".wav"}):
            result["android"] = True
            result["android_fast_checks"] = True
        elif path.startswith("tests/android/") and suffix == ".py":
            result["android"] = True
            result["android_fast_checks"] = True
        elif path.startswith("tests/android_media3/") and suffix in SOURCE_EXTENSIONS:
            # This CTest target is built only in the Desktop CMake path.
            desktop()
        elif path.startswith("src/app/") and suffix in (SOURCE_EXTENSIONS | {".qml"}):
            desktop()
            result["android"] = True
        elif path == "src/cloud/probe-main.cpp":
            # The command-line probe is a Desktop executable, not an Android dependency.
            desktop()
        elif path.startswith("src/cloud/") and suffix in SOURCE_EXTENSIONS:
            # CMake links these common WebDAV targets into Desktop and Android.
            desktop()
            result["android"] = True
        elif path.startswith("src/playback/") and suffix in SOURCE_EXTENSIONS:
            if PurePosixPath(path).name.startswith("MailWebDavRangeDevice"):
                # Android links this bounded Range device; Desktop also uses it.
                desktop()
                result["android"] = True
            else:
                desktop()
        elif path == "scripts/collect-licenses.py":
            result["android"] = True
            windows()
            result["windows_zip"] = True
        elif path in {"scripts/package-windows.ps1", "scripts/collect-ffmpeg.ps1",
                      "scripts/inspect-ffmpeg.py"}:
            windows()
            result["windows_zip"] = True
        elif path == "scripts/package-webdav.ps1":
            windows()
            result["webdav_test_kit"] = True
        elif path == "scripts/generate-audio-fixtures.py":
            desktop()
        elif path.startswith(("ui/desktop/", "tests/playback/", "tests/streaming/",
                              "tests/webdav/")) and suffix in (SOURCE_EXTENSIONS | {".qml"}):
            desktop()
        elif path.startswith("scripts/"):
            recognized = False
        else:
            recognized = False

    if not recognized:
        return full_check("неизвестный тип файла; включены все проверки и аудит упаковки")
    if workflow_change:
        return full_check("изменены правила GitHub Actions; включены все платформы и аудиты",
                          ci_rules_changed=True)
    if classifier_change:
        return full_check("изменён классификатор CI или его тесты; включены все платформы и аудиты",
                          ci_rules_changed=True)
    if result["windows_zip"] or result["webdav_test_kit"]:
        result["desktop_windows"] = True
    result["fixtures"] = result["desktop_linux"] or result["desktop_windows"]
    if result["desktop_linux"] or result["desktop_windows"]:
        result["reason"] = "изменения Desktop-кода или его инструментов"
    elif result["android"]:
        result["reason"] = "изменения Android-кода или его инструментов"
    elif result["desktop_windows"]:
        result["reason"] = "изменения комплекта Windows"
    return result


def valid_sha(value, name):
    if not VALID_SHA.fullmatch(value or ""):
        raise ValueError(f"{name} не является полным Git SHA")
    return value


def git_paths(event, draft=False):
    if event == "pull_request":
        head = valid_sha(os.environ.get("CI_HEAD_SHA", ""), "HEAD PR")
        if draft:
            parents = subprocess.run(
                ["git", "rev-list", "--parents", "-n", "1", head],
                check=True, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            ).stdout.strip().split()
            if not parents or parents[0].lower() != head.lower() or len(parents) < 2:
                raise ValueError("последний Draft commit не имеет проверяемого родителя")
            revision = f"{parents[1]}..{head}"
        else:
            base = valid_sha(os.environ.get("CI_BASE_SHA", ""), "base PR")
            revision = f"{base}...{head}"
    elif event == "push":
        ref = os.environ.get("CI_REF", "")
        before = valid_sha(os.environ.get("CI_BEFORE", ""), "before push")
        after = valid_sha(os.environ.get("GITHUB_SHA", ""), "after push")
        if ref != "refs/heads/main" or set(before) == {"0"}:
            raise ValueError("push не содержит допустимую пару SHA защищённой ветки main")
        revision = f"{before}..{after}"
    else:
        raise ValueError(f"Событие {event!r} не использует сравнение Git")
    completed = subprocess.run(
        ["git", "diff", "--name-only", "-z", revision, "--"],
        check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    )
    return [name.decode("utf-8", errors="strict") for name in completed.stdout.split(b"\0") if name]


def truthy(value):
    return str(value or "").strip().lower() in {"1", "true", "yes", "on"}


def lightweight_selection(reason, mode="main-lightweight"):
    return {
        "desktop_linux": False,
        "desktop_windows": False,
        "fixtures": False,
        "android": False,
        "android_fast_checks": False,
        "windows_zip": False,
        "webdav_test_kit": False,
        "desktop_cache_validation": False,
        "android_cache_validation": False,
        "ci_mode": mode,
        "affected_platforms": "только лёгкие проверки документов и политики CI",
        "reason": reason,
    }


def selections_for_event(event):
    if event == "pull_request":
        draft = truthy(os.environ.get("CI_DRAFT"))
        try:
            paths = git_paths(event, draft=draft)
            classified = classify_paths(paths)
            # Draft changes to CI rules still run the fast checks. The required
            # result jobs reject draft-fast below, so this cannot satisfy the
            # merge gate; the cumulative Ready run remains full fail-closed.
            if classified.get("ci_mode") == "full-fail-closed" and not (
                    draft and classified.get("ci_rules_changed")):
                return classified
            targets = platform_summary(classified)
            if draft:
                selection = lightweight_selection(
                    "Draft fast mode: проверены последний commit и его родители; "
                    "полные платформенные проверки остаются обязательными при переходе в Ready",
                    mode="draft-fast",
                )
                selection["android_fast_checks"] = True
                selection["affected_platforms"] = targets
                return selection
            classified["ci_mode"] = "ready-full"
            classified["affected_platforms"] = targets
            return classified
        except (OSError, subprocess.SubprocessError, UnicodeError, ValueError) as error:
            print(f"Классификация не удалась, включаю все проверки: {error}", file=sys.stderr)
            return full_check("ошибка сравнения Git/SHA; безопасно включены все проверки")
    if event == "push":
        try:
            paths = git_paths(event)
            classified = classify_paths(paths)
            if classified.get("ci_rules_changed"):
                selection = lightweight_selection(
                    "push в защищённый main после обязательной PR-проверки; "
                    "изменения CI-правил уже прошли полный PR CI; выполнены лёгкие проверки"
                )
                selection["affected_platforms"] = platform_summary(classified)
                return selection
            if classified.get("ci_mode") == "full-fail-closed":
                return classified
            affected = platform_summary(classified)
            selection = lightweight_selection(
                "push в защищённый main после обязательной PR-проверки; "
                "выполнены лёгкие проверки документов и политики CI"
            )
            selection["affected_platforms"] = affected
            return selection
        except (OSError, subprocess.SubprocessError, UnicodeError, ValueError) as error:
            print(f"Классификация не удалась, включаю все проверки: {error}", file=sys.stderr)
            return full_check("ошибка проверки push в main; безопасно включены все проверки")
    if event == "release" or truthy(os.environ.get("CI_RELEASE")):
        selection = full_check("выпущен релиз; включены полная проверка и упаковка")
        selection["ci_mode"] = "release-full"
        selection["source_sha"] = os.environ.get("GITHUB_SHA", "")
        return selection
    if event == "workflow_dispatch":
        full = truthy(os.environ.get("CI_FULL_TESTS"))
        windows_zip = truthy(os.environ.get("CI_WINDOWS_ZIP"))
        webdav = truthy(os.environ.get("CI_WEBDAV_KIT"))
        android_apk = truthy(os.environ.get("CI_ANDROID_APK"))
        selection = {
            "desktop_linux": full,
            "desktop_windows": full or windows_zip or webdav,
            "fixtures": full or windows_zip or webdav,
            "android": full or android_apk,
            "android_fast_checks": False,
            "windows_zip": windows_zip,
            "webdav_test_kit": webdav,
            "desktop_cache_validation": False,
            "android_cache_validation": False,
            "ci_mode": "manual-selected",
            "reason": "ручной запуск; выполнены только выбранные проверки и упаковка",
        }
        selection["affected_platforms"] = platform_summary(selection)
        return selection
    return full_check(f"неизвестное событие {event!r}; включены все проверки")


def write_outputs(selection):
    tested = subprocess.run(
        ["git", "rev-parse", "HEAD"], check=True, text=True,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    ).stdout.strip()
    event = os.environ.get("CI_EVENT", "")
    source = os.environ.get("CI_HEAD_SHA", "") if event == "pull_request" else tested
    selection["source_sha"] = source or selection.get("source_sha") or tested or "unknown"
    selection["tested_sha"] = tested or "unknown"
    destination = os.environ.get("GITHUB_OUTPUT")
    if not destination:
        raise RuntimeError("GITHUB_OUTPUT отсутствует")
    with open(destination, "a", encoding="utf-8") as output:
        for key, value in selection.items():
            if key == "reason":
                value = str(value).replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A")
            elif isinstance(value, bool):
                value = str(value).lower()
            output.write(f"{key}={value}\n")
    summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary:
        with open(summary, "a", encoding="utf-8") as report:
            report.write(
                f"Режим CI: `{selection['ci_mode']}`\n\n"
                f"Затронутые платформы: {selection['affected_platforms']}\n\n"
                f"Причина классификации: {selection['reason']}\n\n"
                f"Исходный commit: `{selection['source_sha']}`\n\n"
                f"Commit, проверяемый сборкой: `{selection['tested_sha']}`\n"
            )


def main():
    selection = selections_for_event(os.environ.get("CI_EVENT", ""))
    write_outputs(selection)


if __name__ == "__main__":
    main()
