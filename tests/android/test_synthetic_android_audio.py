"""The Android spike packages synthetic audio and keeps the format selector wired."""
from pathlib import Path
import json
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
GENERATOR = ROOT / "scripts/generate_android_audio_fixtures.py"
ANDROID_QML = ROOT / "ui/android/Main.qml"


class SyntheticAndroidAudio(unittest.TestCase):
    def test_mp3_and_flac_fixtures_are_generated_and_verified(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            subprocess.run([sys.executable, str(GENERATOR), str(output)], check=True)
            first_bytes = {name: (output / name).read_bytes()
                           for name in ("android-spike.mp3", "android-spike.flac")}
            manifest = json.loads((output / "manifest.json").read_text(encoding="utf-8"))
            self.assertIn("CC0-1.0", manifest["provenance"])
            self.assertEqual({row["name"] for row in manifest["files"]},
                             {"android-spike.mp3", "android-spike.flac"})
            self.assertEqual({row["probe"]["streams"][0]["codec_name"]
                              for row in manifest["files"]}, {"mp3", "flac"})
            subprocess.run([sys.executable, str(GENERATOR), str(output), "--verify-only"],
                           check=True)

            subprocess.run([sys.executable, str(GENERATOR), str(output)], check=True)
            self.assertEqual(first_bytes,
                             {name: (output / name).read_bytes() for name in first_bytes})

    def test_qml_format_selector_declares_and_uses_repeater_index(self):
        qml = ANDROID_QML.read_text(encoding="utf-8")
        self.assertIn("required property int index", qml)
        self.assertIn("audioTest.selectTrack(index)", qml)
        self.assertIn("audioTest.selectedTrackIndex === index", qml)
        self.assertIn('"Выбранный формат: " + audioTest.trackText', qml)

    def test_diagnostic_audio_paths_are_separate_and_visible(self):
        qml = ANDROID_QML.read_text(encoding="utf-8")
        self.assertIn("audioTest.playerAudioStatusText", qml)
        self.assertIn("audioTest.pcmStatusText", qml)
        self.assertIn("audioTest.syntheticOutputStatusText", qml)
        self.assertIn("audioTest.startSyntheticOutputTest()", qml)


if __name__ == "__main__":
    unittest.main()
