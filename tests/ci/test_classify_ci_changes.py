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


class ChangeSelection(unittest.TestCase):
    def test_markdown_only_runs_no_compilers_or_packagers(self):
        result = CLASSIFIER.classify_paths(["README.md", "docs/CI.md", "AGENTS.md"])
        self.assertEqual(result["desktop_linux"], False)
        self.assertEqual(result["desktop_windows"], False)
        self.assertEqual(result["android"], False)
        self.assertEqual(result["windows_zip"], False)
        self.assertEqual(result["webdav_test_kit"], False)

    def test_desktop_code_keeps_linux_and_windows_tests(self):
        result = CLASSIFIER.classify_paths(["src/playback/LocalPlayback.cpp"])
        self.assertTrue(result["desktop_linux"])
        self.assertTrue(result["desktop_windows"])
        self.assertFalse(result["android"])
        self.assertFalse(result["windows_zip"])

    def test_android_scope_does_not_generate_desktop_audio_fixtures(self):
        result = CLASSIFIER.classify_paths(["ui/android/Main.qml", "android/AndroidManifest.xml"])
        self.assertFalse(result["desktop_linux"])
        self.assertFalse(result["desktop_windows"])
        self.assertTrue(result["android"])

    def test_shared_build_configuration_checks_all_affected_targets(self):
        result = CLASSIFIER.classify_paths(["CMakeLists.txt"])
        self.assertTrue(result["desktop_linux"])
        self.assertTrue(result["desktop_windows"])
        self.assertTrue(result["android"])
        self.assertTrue(result["windows_zip"])
        self.assertTrue(result["webdav_test_kit"])

    def test_shared_application_source_checks_desktop_and_android_without_archives(self):
        result = CLASSIFIER.classify_paths(["src/app/main.cpp"])
        self.assertTrue(result["desktop_linux"])
        self.assertTrue(result["desktop_windows"])
        self.assertTrue(result["android"])
        self.assertFalse(result["windows_zip"])
        self.assertFalse(result["webdav_test_kit"])

    def test_packaging_scripts_run_the_matching_integrity_checks(self):
        zip_result = CLASSIFIER.classify_paths(["scripts/package-windows.ps1"])
        self.assertTrue(zip_result["desktop_windows"])
        self.assertTrue(zip_result["windows_zip"])
        self.assertFalse(zip_result["webdav_test_kit"])
        webdav_result = CLASSIFIER.classify_paths(["scripts/package-webdav.ps1"])
        self.assertTrue(webdav_result["desktop_windows"])
        self.assertTrue(webdav_result["webdav_test_kit"])
        self.assertFalse(webdav_result["windows_zip"])

    def test_classifier_changes_fail_closed(self):
        result = CLASSIFIER.classify_paths(["scripts/classify-ci-changes.py"])
        self.assertTrue(result["desktop_linux"])
        self.assertTrue(result["desktop_windows"])
        self.assertTrue(result["android"])
        self.assertFalse(result["windows_zip"])
        self.assertFalse(result["webdav_test_kit"])
        self.assertTrue(result["desktop_cache_validation"])
        self.assertTrue(result["android_cache_validation"])

    def test_workflow_changes_request_only_the_matching_warm_cache_check(self):
        desktop = CLASSIFIER.classify_paths([".github/workflows/desktop.yml"])
        self.assertTrue(desktop["desktop_cache_validation"])
        self.assertFalse(desktop["android_cache_validation"])
        android = CLASSIFIER.classify_paths([".github/workflows/android.yml"])
        self.assertFalse(android["desktop_cache_validation"])
        self.assertTrue(android["android_cache_validation"])

    def test_manual_artifact_inputs_select_the_pr_branch_build(self):
        environment = {
            "CI_WINDOWS_ZIP": "true",
            "CI_WEBDAV_KIT": "false",
            "CI_FULL_TESTS": "false",
            "CI_ANDROID_APK": "false",
        }
        with patch.dict(os.environ, environment, clear=False):
            desktop = CLASSIFIER.selections_for_event("workflow_dispatch")
        self.assertFalse(desktop["desktop_linux"])
        self.assertTrue(desktop["desktop_windows"])
        self.assertTrue(desktop["windows_zip"])
        self.assertFalse(desktop["webdav_test_kit"])
        self.assertTrue(desktop["fixtures"])
        self.assertFalse(desktop["android"])

    def test_git_diff_error_fails_closed_instead_of_skipping_platforms(self):
        environment = {"CI_BASE_SHA": "base", "CI_HEAD_SHA": "head"}
        with patch.dict(os.environ, environment, clear=False), patch.object(
                CLASSIFIER.subprocess, "run", side_effect=subprocess.CalledProcessError(1, "git diff")):
            result = CLASSIFIER.selections_for_event("pull_request")
        self.assertTrue(result["desktop_linux"])
        self.assertTrue(result["desktop_windows"])
        self.assertTrue(result["android"])
        self.assertTrue(result["windows_zip"])

    def test_unknown_files_and_empty_diff_fail_closed(self):
        for paths in ([], ["new-build-system.toml"]):
            with self.subTest(paths=paths):
                result = CLASSIFIER.classify_paths(paths)
                self.assertTrue(result["desktop_linux"])
                self.assertTrue(result["desktop_windows"])
                self.assertTrue(result["android"])
                self.assertTrue(result["windows_zip"])
                self.assertTrue(result["webdav_test_kit"])

    def test_docs_added_to_code_do_not_hide_the_code_change(self):
        result = CLASSIFIER.classify_paths(["README.md", "src/cloud/WebDavProbe.cpp"])
        self.assertTrue(result["desktop_linux"])
        self.assertTrue(result["desktop_windows"])
        self.assertFalse(result["android"])


if __name__ == "__main__":
    unittest.main()
