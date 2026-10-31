"""Verify the small synthetic assets embedded by the Media3 Android spike."""
from pathlib import Path
import struct
import wave
import unittest


ROOT = Path(__file__).resolve().parents[2]
AUDIO = ROOT / "android/assets/audio"


class SyntheticMedia3Fixtures(unittest.TestCase):
    def test_mp3_is_a_small_mpeg_audio_stream(self):
        data = (AUDIO / "synthetic-440hz.mp3").read_bytes()
        self.assertGreater(len(data), 16_000)
        self.assertLess(len(data), 150_000)
        offset = 0
        if data.startswith(b"ID3"):
            tag_size = sum((data[index] & 0x7F) << (7 * (9 - index))
                           for index in range(6, 10))
            offset = 10 + tag_size
        self.assertEqual(data[offset], 0xFF)
        self.assertEqual(data[offset + 1] & 0xE0, 0xE0)

    def test_flac_has_the_native_stream_marker(self):
        data = (AUDIO / "synthetic-660hz.flac").read_bytes()
        self.assertGreater(len(data), 16_000)
        self.assertLess(len(data), 200_000)
        self.assertEqual(data[:4], b"fLaC")

    def test_wav_regression_sample_is_short_mono_pcm(self):
        with wave.open(str(AUDIO / "synthetic-880hz.wav"), "rb") as sample:
            self.assertEqual(sample.getnchannels(), 1)
            self.assertEqual(sample.getsampwidth(), 2)
            self.assertEqual(sample.getframerate(), 48_000)
            self.assertEqual(sample.getnframes(), 384_000)
            self.assertEqual(sample.getnframes() / sample.getframerate(), 8)
            raw = sample.readframes(sample.getnframes())
        self.assertNotEqual(set(raw), {0})
        samples = struct.unpack("<" + "h" * (len(raw) // 2), raw)
        crossings = sum(1 for before, after in zip(samples, samples[1:])
                        if before <= 0 < after)
        frequency = crossings / 8
        self.assertGreater(frequency, 870)
        self.assertLess(frequency, 890)

    def test_error_case_is_intentionally_missing(self):
        self.assertFalse((AUDIO / "missing-sample.mp3").exists())


if __name__ == "__main__":
    unittest.main()
