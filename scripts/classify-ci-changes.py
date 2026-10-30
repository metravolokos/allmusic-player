"""Fail-closed path classifier shared by the Desktop and Android workflows."""
from pathlib import PurePosixPath
import os
import subprocess
import sys


DOC_EXTENSIONS = {".md", ".markdown", ".mdown", ".mkd"}


def full_check(reason, include_packages=True):
    return {
        "desktop_linux": True,
        "desktop_windows": True,
        "fixtures": True,
        "android": True,
        "windows_zip": include_packages,
        "webdav_test_kit": include_packages,
        "desktop_cache_validation": True,
        "android_cache_validation": True,
        "reason": reason,
    }


def classify_paths(paths):
    """Return checks to run. Unknown, empty, or malformed path sets run everything."""
    normalized = [str(path).replace("\\", "/").strip("/") for path in paths]
    if not normalized or any(not path for path in normalized):
        return full_check("не удалось надёжно определить изменённые файлы")
    if all(PurePosixPath(path).suffix.lower() in DOC_EXTENSIONS for path in normalized):
        return {
            "desktop_linux": False,
            "desktop_windows": False,
            "fixtures": False,
            "android": False,
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
        "windows_zip": False,
        "webdav_test_kit": False,
        "desktop_cache_validation": False,
        "android_cache_validation": False,
        "reason": "изменён код или конфигурация",
    }
    recognized = True
    classifier_change = False

    def desktop():
        result["desktop_linux"] = True
        result["desktop_windows"] = True

    def windows():
        result["desktop_windows"] = True

    for path in normalized:
        if PurePosixPath(path).suffix.lower() in DOC_EXTENSIONS:
            continue
        if path in {"CMakeLists.txt", "CMakePresets.json", "src/app/CMakeLists.txt"}:
            desktop()
            result["android"] = True
            result["windows_zip"] = True
            result["webdav_test_kit"] = True
        elif path == ".github/workflows/desktop.yml":
            desktop()
            result["desktop_cache_validation"] = True
        elif path == ".github/workflows/android.yml":
            result["android"] = True
            result["android_cache_validation"] = True
        elif path in {"scripts/classify-ci-changes.py"} or path.startswith("tests/ci/"):
            classifier_change = True
        elif path == "scripts/check_repository.py":
            # The documentation job executes this script on every event.
            continue
        elif path == "scripts/package-android.py" or path.startswith((
                "ui/android/", "android/", "tests/android/")):
            result["android"] = True
        elif path.startswith("src/app/"):
            desktop()
            result["android"] = True
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
        elif path in {"scripts/generate-audio-fixtures.py"}:
            desktop()
        elif path.startswith(("src/cloud/", "src/playback/", "ui/desktop/",
                              "tests/playback/", "tests/streaming/", "tests/webdav/")):
            desktop()
        elif path.startswith("scripts/"):
            recognized = False
        else:
            recognized = False

    if not recognized:
        return full_check("неизвестный тип файла; включены все проверки и аудит упаковки")
    if classifier_change:
        result["desktop_linux"] = True
        result["desktop_windows"] = True
        result["android"] = True
        result["fixtures"] = True
        result["desktop_cache_validation"] = True
        result["android_cache_validation"] = True
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


def git_paths(event):
    if event == "pull_request":
        base = os.environ.get("CI_BASE_SHA", "")
        head = os.environ.get("CI_HEAD_SHA", "")
        if not base or not head:
            raise ValueError("PR не содержит SHA базовой и исходной веток")
        revision = f"{base}...{head}"
    elif event == "push":
        before = os.environ.get("CI_BEFORE", "")
        after = os.environ.get("GITHUB_SHA", "")
        if not before or not after or set(before) == {"0"}:
            raise ValueError("push не содержит допустимую базовую версию для сравнения")
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


def selections_for_event(event):
    if event in {"pull_request", "push"}:
        try:
            return classify_paths(git_paths(event))
        except (OSError, subprocess.SubprocessError, UnicodeError, ValueError) as error:
            print(f"Классификация не удалась, включаю все проверки: {error}", file=sys.stderr)
            return full_check("ошибка сравнения Git; безопасно включены все проверки")
    if event == "release" or truthy(os.environ.get("CI_RELEASE")):
        selection = full_check("выпущен релиз; включены полная проверка и упаковка")
        selection["source_sha"] = os.environ.get("GITHUB_SHA", "")
        return selection
    if event == "workflow_dispatch":
        full = truthy(os.environ.get("CI_FULL_TESTS"))
        windows_zip = truthy(os.environ.get("CI_WINDOWS_ZIP"))
        webdav = truthy(os.environ.get("CI_WEBDAV_KIT"))
        android_apk = truthy(os.environ.get("CI_ANDROID_APK"))
        return {
            "desktop_linux": full,
            "desktop_windows": full or windows_zip or webdav,
            "fixtures": full or windows_zip or webdav,
            "android": full or android_apk,
            "windows_zip": windows_zip,
            "webdav_test_kit": webdav,
            "desktop_cache_validation": False,
            "android_cache_validation": False,
            "reason": "ручной запуск; выполнены только выбранные проверки и упаковка",
        }
    return full_check(f"неизвестное событие {event!r}; включены все проверки")


def write_outputs(selection):
    tested = subprocess.run(
        ["git", "rev-parse", "HEAD"], check=True, text=True,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    ).stdout.strip()
    event = os.environ.get("CI_EVENT", "")
    source = os.environ.get("CI_HEAD_SHA", "") if event == "pull_request" else tested
    selection["source_sha"] = source or tested or "unknown"
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
                f"Решение классификатора: {selection['reason']}\n\n"
                f"Исходный commit: `{selection['source_sha']}`\n"
                f"Commit, проверяемый сборкой: `{selection['tested_sha']}`\n"
            )


def main():
    selection = selections_for_event(os.environ.get("CI_EVENT", ""))
    write_outputs(selection)


if __name__ == "__main__":
    main()
