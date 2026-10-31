"""Offline policy tests; no GitHub runner, Qt or Android SDK is required."""
import importlib.util
import os
from pathlib import Path
import subprocess
import unittest
from unittest.mock import patch


SCRIPT = Path(__file__).resolve().parents[2] / "scripts/classify-ci-changes.py"
SPEC = importlib.util.spec_from_file_location("ci_classifier", SCRIPT)
CLASSIFIER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CLASSIFIER)
BASE_SHA = "a" * 40
HEAD_SHA = "b" * 40
PARENT_SHA = "c" * 40


def completed(stdout=b""):
    return subprocess.CompletedProcess(["git"], 0, stdout=stdout, stderr=b"")


def completed_text(stdout=""):
    return subprocess.CompletedProcess(["git"], 0, stdout=stdout, stderr="")


class ChangeSelection(unittest.TestCase):
    def test_markdown_only_runs_no_compilers_or_packagers(self):
        result = CLASSIFIER.classify_paths(["README.md", "docs/CI.md", "AGENTS.md"])
        for job in ("desktop_linux", "desktop_windows", "android", "windows_zip", "webdav_test_kit"):
            with self.subTest(job=job):
                self.assertFalse(result[job])

    def test_desktop_code_keeps_linux_and_windows_tests(self):
        result = CLASSIFIER.classify_paths(["src/playback/LocalPlayback.cpp"])
        self.assertTrue(result["desktop_linux"])
        self.assertTrue(result["desktop_windows"])
        self.assertFalse(result["android"])
        self.assertFalse(result["windows_zip"])

    def test_android_qml_java_gradle_and_python_tests_select_android(self):
        for path in (
                "ui/android/Main.qml",
                "android/src/org/metravolokos/allmusicplayer/Media3PlaybackAdapter.java",
                "android/src/org/metravolokos/allmusicplayer/Media3PlaybackAdapter.kt",
                "android/build.gradle",
                "tests/android/test_package_audit.py",
                "scripts/package-android.py"):
            with self.subTest(path=path):
                result = CLASSIFIER.classify_paths([path])
                self.assertTrue(result["android"])
                self.assertTrue(result["android_fast_checks"])
                self.assertFalse(result["desktop_linux"])
                self.assertFalse(result["desktop_windows"])

    def test_shared_build_configuration_and_application_source_check_both_platforms(self):
        for path in ("CMakeLists.txt", "src/app/main.cpp"):
            with self.subTest(path=path):
                result = CLASSIFIER.classify_paths([path])
                self.assertTrue(result["desktop_linux"])
                self.assertTrue(result["desktop_windows"])
                self.assertTrue(result["android"])
        cmake = CLASSIFIER.classify_paths(["CMakeLists.txt"])
        self.assertTrue(cmake["windows_zip"])
        self.assertTrue(cmake["webdav_test_kit"])

    def test_cloud_webdav_and_shared_range_device_check_both_platforms(self):
        for path in ("src/cloud/WebDavProbe.cpp", "src/cloud/MailWebDavProbe.h",
                     "src/playback/MailWebDavRangeDevice.cpp",
                     "src/playback/MailWebDavRangeDevice.h"):
            with self.subTest(path=path):
                result = CLASSIFIER.classify_paths([path])
                self.assertTrue(result["desktop_linux"])
                self.assertTrue(result["desktop_windows"])
                self.assertTrue(result["android"])

    def test_desktop_probe_and_desktop_tests_stay_desktop_only(self):
        for path in ("src/cloud/probe-main.cpp",
                     "src/playback/WebDavRangeDevice.cpp",
                     "tests/android_media3/tst_android_media3_state.cpp"):
            with self.subTest(path=path):
                result = CLASSIFIER.classify_paths([path])
                self.assertTrue(result["desktop_linux"])
                self.assertTrue(result["desktop_windows"])
                self.assertFalse(result["android"])

    def test_packaging_scripts_run_the_matching_integrity_checks(self):
        zip_result = CLASSIFIER.classify_paths(["scripts/package-windows.ps1"])
        self.assertTrue(zip_result["desktop_windows"])
        self.assertTrue(zip_result["windows_zip"])
        self.assertFalse(zip_result["webdav_test_kit"])
        webdav_result = CLASSIFIER.classify_paths(["scripts/package-webdav.ps1"])
        self.assertTrue(webdav_result["desktop_windows"])
        self.assertTrue(webdav_result["webdav_test_kit"])
        self.assertFalse(webdav_result["windows_zip"])

    def test_classifier_tests_and_workflow_changes_fail_closed(self):
        for path in ("scripts/classify-ci-changes.py", "scripts/check_repository.py",
                     "tests/ci/test_classify_ci_changes.py",
                     ".github/workflows/desktop.yml", ".github/workflows/android.yml"):
            with self.subTest(path=path):
                result = CLASSIFIER.classify_paths([path])
                self.assertEqual(result["ci_mode"], "full-fail-closed")
                for job in ("desktop_linux", "desktop_windows", "android", "windows_zip",
                            "webdav_test_kit"):
                    self.assertTrue(result[job])

    def test_manual_artifact_inputs_select_the_requested_source_build(self):
        environment = {
            "CI_WINDOWS_ZIP": "true",
            "CI_WEBDAV_KIT": "false",
            "CI_FULL_TESTS": "false",
            "CI_ANDROID_APK": "false",
        }
        with patch.dict(os.environ, environment, clear=False):
            desktop = CLASSIFIER.selections_for_event("workflow_dispatch")
        self.assertEqual(desktop["ci_mode"], "manual-selected")
        self.assertFalse(desktop["desktop_linux"])
        self.assertTrue(desktop["desktop_windows"])
        self.assertTrue(desktop["windows_zip"])
        self.assertFalse(desktop["webdav_test_kit"])
        self.assertTrue(desktop["fixtures"])
        self.assertFalse(desktop["android"])

    def test_draft_uses_only_latest_commit_and_skips_binary_builds(self):
        environment = {
            "CI_DRAFT": "true",
            "CI_BASE_SHA": BASE_SHA,
            "CI_HEAD_SHA": HEAD_SHA,
        }
        with patch.dict(os.environ, environment, clear=False), patch.object(
                CLASSIFIER.subprocess, "run",
                side_effect=[completed_text(f"{HEAD_SHA} {PARENT_SHA}\n"),
                             completed(b"ui/android/Main.qml\0")]) as run:
            result = CLASSIFIER.selections_for_event("pull_request")
        self.assertEqual(result["ci_mode"], "draft-fast")
        self.assertIn("Android", result["affected_platforms"])
        self.assertTrue(result["android_fast_checks"])
        for job in ("desktop_linux", "desktop_windows", "fixtures", "android",
                    "windows_zip", "webdav_test_kit", "desktop_cache_validation",
                    "android_cache_validation"):
            self.assertFalse(result[job], job)
        self.assertEqual(run.call_args_list[0].args[0],
                         ["git", "rev-list", "--parents", "-n", "1", HEAD_SHA])
        self.assertEqual(run.call_args_list[1].args[0][4], f"{PARENT_SHA}..{HEAD_SHA}")

    def test_draft_classifier_change_forces_full_fail_closed(self):
        environment = {
            "CI_DRAFT": "true",
            "CI_BASE_SHA": BASE_SHA,
            "CI_HEAD_SHA": HEAD_SHA,
        }
        with patch.dict(os.environ, environment, clear=False), patch.object(
                CLASSIFIER.subprocess, "run",
                side_effect=[completed_text(f"{HEAD_SHA} {PARENT_SHA}\n"),
                             completed(b"scripts/classify-ci-changes.py\0")]):
            result = CLASSIFIER.selections_for_event("pull_request")
        self.assertEqual(result["ci_mode"], "full-fail-closed")
        self.assertTrue(result["desktop_linux"])
        self.assertTrue(result["desktop_windows"])
        self.assertTrue(result["android"])

    def test_ready_pr_uses_full_cumulative_base_to_head_diff(self):
        environment = {
            "CI_DRAFT": "false",
            "CI_BASE_SHA": BASE_SHA,
            "CI_HEAD_SHA": HEAD_SHA,
        }
        with patch.dict(os.environ, environment, clear=False), patch.object(
                CLASSIFIER.subprocess, "run", return_value=completed(
                    b"src/cloud/MailWebDavProbe.cpp\0")) as run:
            result = CLASSIFIER.selections_for_event("pull_request")
        self.assertEqual(result["ci_mode"], "ready-full")
        self.assertIn(f"{BASE_SHA}...{HEAD_SHA}", run.call_args.args[0])
        self.assertTrue(result["desktop_linux"])
        self.assertTrue(result["desktop_windows"])
        self.assertTrue(result["android"])

    def test_ready_android_only_pr_skips_desktop(self):
        environment = {
            "CI_DRAFT": "false",
            "CI_BASE_SHA": BASE_SHA,
            "CI_HEAD_SHA": HEAD_SHA,
        }
        with patch.dict(os.environ, environment, clear=False), patch.object(
                CLASSIFIER.subprocess, "run", return_value=completed(
                    b"ui/android/Main.qml\0")):
            result = CLASSIFIER.selections_for_event("pull_request")
        self.assertEqual(result["ci_mode"], "ready-full")
        self.assertTrue(result["android"])
        self.assertFalse(result["desktop_linux"])
        self.assertFalse(result["desktop_windows"])

    def test_ready_docs_only_pr_skips_compilers(self):
        environment = {
            "CI_DRAFT": "false",
            "CI_BASE_SHA": BASE_SHA,
            "CI_HEAD_SHA": HEAD_SHA,
        }
        with patch.dict(os.environ, environment, clear=False), patch.object(
                CLASSIFIER.subprocess, "run", return_value=completed(b"docs/CI.md\0")):
            result = CLASSIFIER.selections_for_event("pull_request")
        self.assertEqual(result["ci_mode"], "ready-full")
        self.assertIn("лёгкие проверки", result["affected_platforms"])
        self.assertFalse(result["desktop_linux"])
        self.assertFalse(result["desktop_windows"])
        self.assertFalse(result["android"])

    def test_invalid_pr_sha_fails_closed(self):
        environment = {"CI_DRAFT": "false", "CI_BASE_SHA": "base", "CI_HEAD_SHA": HEAD_SHA}
        with patch.dict(os.environ, environment, clear=False):
            result = CLASSIFIER.selections_for_event("pull_request")
        self.assertEqual(result["ci_mode"], "full-fail-closed")
        self.assertTrue(result["desktop_linux"])
        self.assertTrue(result["desktop_windows"])
        self.assertTrue(result["android"])

    def test_git_diff_error_fails_closed_instead_of_skipping_platforms(self):
        environment = {"CI_DRAFT": "false", "CI_BASE_SHA": BASE_SHA, "CI_HEAD_SHA": HEAD_SHA}
        with patch.dict(os.environ, environment, clear=False), patch.object(
                CLASSIFIER.subprocess, "run",
                side_effect=subprocess.CalledProcessError(1, "git diff")):
            result = CLASSIFIER.selections_for_event("pull_request")
        self.assertTrue(result["desktop_linux"])
        self.assertTrue(result["desktop_windows"])
        self.assertTrue(result["android"])
        self.assertTrue(result["windows_zip"])

    def test_protected_main_push_skips_repeat_builds_for_known_changes(self):
        environment = {
            "CI_REF": "refs/heads/main",
            "CI_BEFORE": PARENT_SHA,
            "GITHUB_SHA": HEAD_SHA,
        }
        with patch.dict(os.environ, environment, clear=False), patch.object(
                CLASSIFIER.subprocess, "run", return_value=completed(b"src/playback/LocalPlayback.cpp\0")):
            result = CLASSIFIER.selections_for_event("push")
        self.assertEqual(result["ci_mode"], "main-lightweight")
        for job in ("desktop_linux", "desktop_windows", "fixtures", "android",
                    "windows_zip", "webdav_test_kit", "desktop_cache_validation",
                    "android_cache_validation"):
            self.assertFalse(result[job], job)

    def test_main_push_unknown_file_fails_closed(self):
        environment = {
            "CI_REF": "refs/heads/main",
            "CI_BEFORE": PARENT_SHA,
            "GITHUB_SHA": HEAD_SHA,
        }
        with patch.dict(os.environ, environment, clear=False), patch.object(
                CLASSIFIER.subprocess, "run", return_value=completed(b"new-build-system.toml\0")):
            result = CLASSIFIER.selections_for_event("push")
        self.assertTrue(result["desktop_linux"])
        self.assertTrue(result["desktop_windows"])
        self.assertTrue(result["android"])
        self.assertTrue(result["windows_zip"])
        self.assertTrue(result["webdav_test_kit"])

    def test_protected_main_push_keeps_lightweight_mode_after_ci_policy_change(self):
        environment = {
            "CI_REF": "refs/heads/main",
            "CI_BEFORE": PARENT_SHA,
            "GITHUB_SHA": HEAD_SHA,
        }
        with patch.dict(os.environ, environment, clear=False), patch.object(
                CLASSIFIER.subprocess, "run",
                return_value=completed(b"scripts/classify-ci-changes.py\0")):
            result = CLASSIFIER.selections_for_event("push")
        self.assertEqual(result["ci_mode"], "main-lightweight")
        for job in ("desktop_linux", "desktop_windows", "android", "windows_zip", "webdav_test_kit"):
            self.assertFalse(result[job], job)

    def test_main_push_without_valid_before_sha_fails_closed(self):
        environment = {
            "CI_REF": "refs/heads/main",
            "CI_BEFORE": "0" * 40,
            "GITHUB_SHA": HEAD_SHA,
        }
        with patch.dict(os.environ, environment, clear=False):
            result = CLASSIFIER.selections_for_event("push")
        self.assertTrue(result["desktop_linux"])
        self.assertTrue(result["desktop_windows"])
        self.assertTrue(result["android"])

    def test_push_outside_protected_main_fails_closed(self):
        environment = {
            "CI_REF": "refs/heads/feature",
            "CI_BEFORE": PARENT_SHA,
            "GITHUB_SHA": HEAD_SHA,
        }
        with patch.dict(os.environ, environment, clear=False):
            result = CLASSIFIER.selections_for_event("push")
        self.assertTrue(result["desktop_linux"])
        self.assertTrue(result["desktop_windows"])
        self.assertTrue(result["android"])

    def test_release_keeps_full_platform_checks_and_package_audits(self):
        result = CLASSIFIER.selections_for_event("release")
        self.assertEqual(result["ci_mode"], "release-full")
        self.assertTrue(result["desktop_linux"])
        self.assertTrue(result["desktop_windows"])
        self.assertTrue(result["android"])
        self.assertTrue(result["windows_zip"])
        self.assertTrue(result["webdav_test_kit"])

    def test_unknown_empty_and_malformed_paths_fail_closed(self):
        for paths in ([], ["new-build-system.toml"], ["../outside.py"], [""]):
            with self.subTest(paths=paths):
                result = CLASSIFIER.classify_paths(paths)
                self.assertEqual(result["ci_mode"], "full-fail-closed")
                self.assertTrue(result["desktop_linux"])
                self.assertTrue(result["desktop_windows"])
                self.assertTrue(result["android"])
                self.assertTrue(result["windows_zip"])
                self.assertTrue(result["webdav_test_kit"])

    def test_docs_added_to_code_do_not_hide_the_code_change(self):
        result = CLASSIFIER.classify_paths(["README.md", "src/cloud/WebDavProbe.cpp"])
        self.assertTrue(result["desktop_linux"])
        self.assertTrue(result["desktop_windows"])
        self.assertTrue(result["android"])


if __name__ == "__main__":
    unittest.main()
