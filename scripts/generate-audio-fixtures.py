"""Generate synthetic tones; FFmpeg/ffprobe are build tools, not shipped CLI.

No personal music. Five 8 s local samples and five 32 s streaming samples.
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
STREAM_CASES = [
    ("tone-stream-mp3.mp3", "libmp3lame", "mp3", "mp3", ["-b:a", "128k"]),
    (STREAM_NAME, "flac", "flac", "flac", ["-sample_fmt", "s16"]),
    ("tone-stream-aac-adts.aac", "aac", "aac", "adts", ["-b:a", "128k"]),
    ("tone-stream-aac-m4a.m4a", "aac", "aac", "ipod", ["-b:a", "128k"]),
    ("tone-stream-alac-m4a.m4a", "alac", "alac", "ipod", ["-sample_fmt", "s32p"]),
]


def run(args):
    return subprocess.check_output(args, text=True)


def generation_command(path, encoder, muxer, options, duration):
    fade_start = duration - 0.05
    command = ["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-f", "lavfi",
               "-i", f"sine=frequency=440:sample_rate=44100:duration={duration}",
               "-af", f"volume=0.5,afade=t=in:d=0.05,afade=t=out:st={fade_start}:d=0.05",
               "-ac", "2", "-c:a", encoder, *options, "-map_metadata", "-1"]
    if muxer == "ipod":
        command += ["-movflags", "+faststart"]
    return command + ["-f", muxer, str(path)]


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
        command = generation_command(path, encoder, muxer, options, 8)
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
    streaming_files = []
    for name, encoder, codec, muxer, options in STREAM_CASES:
        stream_path = args.directory / name
        command = generation_command(stream_path, encoder, muxer, options, 32)
        if not args.verify_only:
            subprocess.run(command, check=True)
        probe = json.loads(run(["ffprobe", "-v", "error", "-show_streams", "-show_format",
                               "-of", "json", str(stream_path)]))
        assert len(probe["streams"]) == 1 and probe["streams"][0]["codec_name"] == codec, name
        stream = probe["streams"][0]
        assert stream["sample_rate"] == "44100" and stream["channels"] == 2, name
        assert 31.8 < float(probe["format"]["duration"]) < 32.5, name
        if codec == "aac":
            assert stream["profile"] == "LC", name
        if codec == "alac":
            assert stream["bits_per_raw_sample"] == "24", stream
        streaming_files.append({"name": name,
            "sha256": hashlib.sha256(stream_path.read_bytes()).hexdigest(),
            "generation_command": command, "probe": probe})
    manifest["streaming_files"] = streaming_files
    # Keep the original B2 key available for scripts that identify the FLAC sample.
    manifest["streaming_file"] = next(item for item in streaming_files if item["name"] == STREAM_NAME)
    if not args.verify_only:
        (args.directory / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
        (args.directory / "README.txt").write_text(
            "Synthetic 440 Hz stereo tones, CC0-1.0. No personal music.\n"
            "Five 8 s local samples and five 32 s WebDAV streaming samples.\n"
            "Start at low volume.\n")
    else:
        original = json.loads((args.directory / "manifest.json").read_text())
        assert [(f["name"], f["sha256"]) for f in original["files"]] == [
            (f["name"], f["sha256"]) for f in manifest["files"]], "Fixture hash mismatch"
        expected_streams = [(f["name"], f["sha256"]) for f in manifest["streaming_files"]]
        actual_streams = [(f["name"], f["sha256"]) for f in original.get("streaming_files", [])]
        assert actual_streams == expected_streams, "Streaming fixture hash mismatch"
    print("Verified five local and five streaming formats (ffprobe), not Qt playback")


if __name__ == "__main__":
    main()
