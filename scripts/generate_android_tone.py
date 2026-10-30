#!/usr/bin/env python3
"""Generate the short synthetic WAV used by the Android audio test screen."""

from pathlib import Path
import math
import struct
import sys
import wave


SAMPLE_RATE = 48_000
DURATION_SECONDS = 6
FREQUENCY_HZ = 440
AMPLITUDE = 0.25
FADE_SECONDS = 0.01


def generate(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    frame_count = SAMPLE_RATE * DURATION_SECONDS
    fade_frames = int(SAMPLE_RATE * FADE_SECONDS)
    frames = bytearray()

    for index in range(frame_count):
        envelope = min(1.0, index / fade_frames, (frame_count - index - 1) / fade_frames)
        sample = round(32767 * AMPLITUDE * envelope
                       * math.sin(2 * math.pi * FREQUENCY_HZ * index / SAMPLE_RATE))
        frames.extend(struct.pack("<h", sample))

    with wave.open(str(path), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(SAMPLE_RATE)
        output.writeframes(frames)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: generate_android_tone.py OUTPUT.wav")
    generate(Path(sys.argv[1]))
