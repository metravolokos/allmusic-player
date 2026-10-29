"""Generate only synthetic tones; FFmpeg/ffprobe are build tools, not shipped CLI.

No personal music. Signal: 440 Hz, 8 s, 44100 Hz stereo, low amplitude.
Generated signal and manifest may be used under CC0-1.0.
"""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess

CASES = [
    ("tone-mp3.mp3", "libmp3lame", "mp3", "mp3", ["-b:a", "128k"]),
    ("tone-flac.flac", "flac", "flac", "flac", ["-sample_fmt", "s16"]),
    ("tone-aac-adts.aac", "aac", "aac", "adts", ["-b:a", "128k"]),
    ("tone-aac-m4a.m4a", "aac", "aac", "ipod", ["-b:a", "128k"]),
    ("tone-alac-m4a.m4a", "alac", "alac", "ipod", ["-sample_fmt", "s32p"]),
]


def run(args):
    return subprocess.check_output(args, text=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("directory", type=Path)
    parser.add_argument("--verify-only", action="store_true")
    args = parser.parse_args()
    args.directory.mkdir(parents=True, exist_ok=True)
    manifest = {"provenance": "Synthetic 440 Hz tone generated locally; CC0-1.0",
                "ffmpeg": run(["ffmpeg", "-version"]).splitlines()[0], "files": []}
    for name, encoder, codec, muxer, options in CASES:
        path = args.directory / name
        command = ["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-f", "lavfi",
                   "-i", "sine=frequency=440:sample_rate=44100:duration=8",
                   "-af", "volume=0.5,afade=t=in:d=0.05,afade=t=out:st=7.95:d=0.05",
                   "-ac", "2", "-c:a", encoder, *options, "-map_metadata", "-1", "-f", muxer, str(path)]
        if not args.verify_only:
            subprocess.run(command, check=True)
        probe = json.loads(run(["ffprobe", "-v", "error", "-show_streams", "-show_format", "-of", "json", str(path)]))
        streams = probe["streams"]
        assert len(streams) == 1 and streams[0]["codec_type"] == "audio", name
        stream = streams[0]
        assert stream["codec_name"] == codec, (name, stream)
        expected_container = "mov" if muxer == "ipod" else ("aac" if muxer == "adts" else muxer)
        assert expected_container in probe["format"]["format_name"].split(","), name
        assert stream["sample_rate"] == "44100" and stream["channels"] == 2, name
        assert 7.8 < float(probe["format"]["duration"]) < 8.5, name
        if codec == "aac":
            assert stream["profile"] == "LC", name
        if codec == "alac":
            assert stream["bits_per_raw_sample"] == "24", stream
        manifest["files"].append({"name": name, "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                                  "generation_command": command, "probe": probe})
    if not args.verify_only:
        (args.directory / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
        (args.directory / "README.txt").write_text(
            "001-B1: synthetic 440 Hz / 8 s stereo tones, CC0-1.0. No personal music.\n"
            "MP3, FLAC 16-bit, AAC-LC ADTS + M4A, ALAC 24-bit M4A. Start at low volume.\n")
    else:
        original = json.loads((args.directory / "manifest.json").read_text())
        assert [(f["name"], f["sha256"]) for f in original["files"]] == [
            (f["name"], f["sha256"]) for f in manifest["files"]], "Fixture hash mismatch"
    print("Verified five real codec/container combinations (ffprobe), not Qt playback")


if __name__ == "__main__":
    main()
