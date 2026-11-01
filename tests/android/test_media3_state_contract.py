"""Source-level JNI contract checks; these do not execute Media3 or Android."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]


class Media3StateContractTest(unittest.TestCase):
    def test_existing_jni_state_numbers_are_unchanged_and_buffering_is_appended(self):
        java = (ROOT / "android/src/org/metravolokos/allmusicplayer/Media3PlaybackAdapter.java")
        cpp = (ROOT / "src/app/AndroidMedia3State.h").read_text(encoding="utf-8")
        java = java.read_text(encoding="utf-8")

        for name, value in (("PREPARING", 0), ("READY", 1), ("PLAYING", 2),
                            ("PAUSED", 3), ("STOPPED", 4), ("FINISHED", 5),
                            ("ERROR", 6), ("BUFFERING", 7)):
            with self.subTest(state=name):
                self.assertRegex(java, rf"public static final int {name} = {value};")
                self.assertRegex(cpp, rf"{name.title()}\s*=\s*{value}")

    def test_buffering_distinguishes_first_prepare_from_rebuffering(self):
        java = (ROOT / "android/src/org/metravolokos/allmusicplayer/Media3PlaybackAdapter.java")
        java = java.read_text(encoding="utf-8")
        buffering = java.split("if (playbackState == Player.STATE_BUFFERING)", 1)[1]

        self.assertIn("hasReachedReady ? BUFFERING : PREPARING", buffering)
        self.assertIn("!player.getPlayWhenReady() ? PAUSED", buffering)
        self.assertIn("hasReachedReady = false", java)

    def test_stop_and_pause_guard_late_playback_callbacks(self):
        java = (ROOT / "android/src/org/metravolokos/allmusicplayer/Media3PlaybackAdapter.java")
        java = java.read_text(encoding="utf-8")
        controller = (ROOT / "src/app/AndroidMedia3Controller.cpp")
        controller = controller.read_text(encoding="utf-8")

        self.assertIn("if (state == ERROR || stoppedByUser)", java)
        self.assertIn("if (stoppedByUser)\n                            return;", java)
        self.assertIn("current.pause();\n            current.stop();", java)
        self.assertIn("acceptsRemoteUpdate(m_state, remoteState)", controller)

    def test_buffering_text_is_shown_by_the_android_player_panel(self):
        controller = (ROOT / "src/app/AndroidMedia3Controller.cpp")
        controller = controller.read_text(encoding="utf-8")
        qml = (ROOT / "ui/android/Main.qml").read_text(encoding="utf-8")

        self.assertIn('Value::Buffering: return tr("Буферизация Media3")', controller)
        self.assertIn("media3.stateText", qml)
        self.assertIn(r'"\nПлеер: " + media3.activeTrackName + " · " + media3.stateText', qml)
        self.assertIn('"Избранное: " + root.selectedCloudLabel', qml)


if __name__ == "__main__":
    unittest.main()
