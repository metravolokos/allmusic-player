"""Generate only synthetic tones; FFmpeg/ffprobe are build tools, not shipped CLI.

No personal music. Five 8 s playback samples plus a 32 s FLAC stream/seek fixture.
Generated signals and manifest may be used under CC0-1.0.
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
STREAM_NAME = "tone-stream-flac.flac"


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
    stream_path = args.directory / STREAM_NAME
    stream_command = ["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-f", "lavfi",
                      "-i", "sine=frequency=440:sample_rate=44100:duration=32",
                      "-af", "volume=0.5,afade=t=in:d=0.05,afade=t=out:st=31.95:d=0.05",
                      "-ac", "2", "-c:a", "flac", "-sample_fmt", "s16", "-map_metadata", "-1",
                      "-f", "flac", str(stream_path)]
    if not args.verify_only:
        subprocess.run(stream_command, check=True)
    stream_probe = json.loads(run(["ffprobe", "-v", "error", "-show_streams", "-show_format",
                                   "-of", "json", str(stream_path)]))
    assert len(stream_probe["streams"]) == 1 and stream_probe["streams"][0]["codec_name"] == "flac"
    assert 31.8 < float(stream_probe["format"]["duration"]) < 32.5
    manifest["streaming_file"] = {"name": STREAM_NAME,
        "sha256": hashlib.sha256(stream_path.read_bytes()).hexdigest(),
        "generation_command": stream_command, "probe": stream_probe}
    if not args.verify_only:
        (args.directory / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
        (args.directory / "README.txt").write_text(
            "Synthetic 440 Hz stereo tones, CC0-1.0. No personal music.\n"
            "Five 8 s codec/container playback samples plus 32 s FLAC for 001-B2 Range/seek.\n"
            "Start at low volume.\n")
    else:
        original = json.loads((args.directory / "manifest.json").read_text())
        assert [(f["name"], f["sha256"]) for f in original["files"]] == [
            (f["name"], f["sha256"]) for f in manifest["files"]], "Fixture hash mismatch"
        assert original["streaming_file"]["name"] == STREAM_NAME
        assert original["streaming_file"]["sha256"] == manifest["streaming_file"]["sha256"], "Streaming fixture hash mismatch"
    print("Verified five playback formats and 32 s streaming fixture (ffprobe), not Qt playback")


if __name__ == "__main__":
    main()
