"""Helpers for enforcing required GitHub Actions merge checks."""


PR_FULL_MODES = frozenset({"ready-full", "full-fail-closed"})


def required_pr_gate_error(event_name, ci_mode):
    """Return an error unless a pull request has selected a full CI mode.

    Pushes to protected main, releases, and manual runs retain their own
    lightweight or selected behavior. Only pull request required contexts are
    gated on Ready/full classification.
    """
    if event_name != "pull_request":
        return None
    if ci_mode in PR_FULL_MODES:
        return None
    reported_mode = ci_mode or "missing"
    return (
        f"Draft/non-full PR mode {reported_mode!r} cannot satisfy required "
        "merge checks; convert the PR to Ready and pass full CI first."
    )
