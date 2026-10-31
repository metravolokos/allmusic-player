"""Generate the short, synthetic Android playback fixtures with FFmpeg."""
from pathlib import Path
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "android/assets/audio"


def generate(name, frequency, codec, *options):
    OUTPUT.mkdir(parents=True, exist_ok=True)
    destination = OUTPUT / name
    subprocess.run([
        "ffmpeg", "-nostdin", "-hide_banner", "-loglevel", "error",
        "-f", "lavfi", "-i", f"sine=frequency={frequency}:sample_rate=48000:duration=8",
        "-map_metadata", "-1", "-ac", "1", "-c:a", codec,
        *options, "-y", str(destination),
    ], check=True)
    print(f"Generated synthetic fixture: {destination}")


generate("synthetic-440hz.mp3", 440, "libmp3lame", "-b:a", "96k", "-write_id3v2", "0", "-write_xing", "0")
generate("synthetic-660hz.flac", 660, "flac", "-compression_level", "5")
generate("synthetic-880hz.wav", 880, "pcm_s16le")
