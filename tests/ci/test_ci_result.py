"""Tests for the merge gate used by the required workflow result jobs."""
import unittest
from pathlib import Path

from scripts.ci_result import required_pr_gate_error


class RequiredPrGate(unittest.TestCase):
    def test_draft_fast_mode_cannot_satisfy_required_checks(self):
        error = required_pr_gate_error("pull_request", "draft-fast")
        self.assertIsNotNone(error)
        self.assertIn("cannot satisfy required", error)

    def test_ready_full_pr_mode_satisfies_mode_gate(self):
        self.assertIsNone(required_pr_gate_error("pull_request", "ready-full"))

    def test_forced_full_pr_mode_satisfies_mode_gate(self):
        self.assertIsNone(required_pr_gate_error("pull_request", "full-fail-closed"))

    def test_unknown_or_missing_pr_mode_fails_closed(self):
        for mode in ("main-lightweight", "", None):
            with self.subTest(mode=mode):
                self.assertIsNotNone(required_pr_gate_error("pull_request", mode))

    def test_non_pull_request_modes_are_not_blocked(self):
        for event, mode in (
                ("push", "main-lightweight"),
                ("release", "release-full"),
                ("workflow_dispatch", "manual-selected")):
            with self.subTest(event=event):
                self.assertIsNone(required_pr_gate_error(event, mode))

    def test_both_required_workflow_jobs_use_the_gate_helper(self):
        repository_root = Path(__file__).resolve().parents[2]
        for workflow in ("desktop.yml", "android.yml"):
            with self.subTest(workflow=workflow):
                content = (repository_root / ".github/workflows" / workflow).read_text(
                    encoding="utf-8")
                self.assertIn("EVENT_NAME: ${{ github.event_name }}", content)
                result_job = content.split("  ci-result:", 1)[1]
                self.assertIn("actions/checkout@11bd71901bbe5b1630ceea73d27597364c9af683", result_job)
                self.assertIn("persist-credentials: false", result_job)
                self.assertIn(
                    'required_pr_gate_error(os.environ["EVENT_NAME"], os.environ["CI_MODE"])',
                    content,
                )


if __name__ == "__main__":
    unittest.main()
