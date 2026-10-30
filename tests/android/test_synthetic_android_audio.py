"""The Android spike packages only locally generated CC0 MP3 and FLAC samples."""
from pathlib import Path
import json
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
GENERATOR = ROOT / "scripts/generate_android_audio_fixtures.py"


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


if __name__ == "__main__":
    unittest.main()
