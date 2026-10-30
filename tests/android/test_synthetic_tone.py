"""Check that the Android audio fixture is reproducible and contains no media input."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import wave


ROOT = Path(__file__).resolve().parents[2]
GENERATOR = ROOT / "scripts/generate_android_tone.py"


class SyntheticAndroidTone(unittest.TestCase):
    def test_generated_wav_is_short_mono_pcm(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "tone.wav"
            subprocess.run([sys.executable, str(GENERATOR), str(path)], check=True)
            with wave.open(str(path), "rb") as tone:
                self.assertEqual(tone.getnchannels(), 1)
                self.assertEqual(tone.getsampwidth(), 2)
                self.assertEqual(tone.getframerate(), 48_000)
                self.assertEqual(tone.getnframes(), 288_000)
                self.assertEqual(tone.getnframes() / tone.getframerate(), 6)
                self.assertNotEqual(set(tone.readframes(tone.getnframes())), {0})

    def test_generation_is_deterministic(self):
        with tempfile.TemporaryDirectory() as directory:
            first = Path(directory) / "first.wav"
            second = Path(directory) / "second.wav"
            subprocess.run([sys.executable, str(GENERATOR), str(first)], check=True)
            subprocess.run([sys.executable, str(GENERATOR), str(second)], check=True)
            self.assertEqual(first.read_bytes(), second.read_bytes())


if __name__ == "__main__":
    unittest.main()
