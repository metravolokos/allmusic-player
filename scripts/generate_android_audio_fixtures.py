"""Generate short, synthetic MP3 and FLAC fixtures for the Android FFmpeg spike.

The samples are synthetic 440 Hz tones (CC0-1.0). FFmpeg is a build/test tool;
its executable is not included in the Android package.
"""

from pathlib import Path
import argparse
import hashlib
import json
import subprocess


CASES = (
    ("android-spike.mp3", "libmp3lame", "mp3", ["-b:a", "128k"]),
    ("android-spike.flac", "flac", "flac", ["-sample_fmt", "s16"]),
)
DURATION_SECONDS = 6


def run(command):
    return subprocess.check_output(command, text=True)


def generation_command(path: Path, encoder: str, muxer: str, options: list[str]) -> list[str]:
    return [
        "ffmpeg", "-hide_banner", "-loglevel", "error", "-y",
        "-f", "lavfi", "-i",
        f"sine=frequency=440:sample_rate=44100:duration={DURATION_SECONDS}",
        "-af", "volume=0.5,afade=t=in:d=0.05,afade=t=out:st=5.95:d=0.05",
        "-ac", "2", "-ar", "44100", "-map_metadata", "-1",
        "-fflags", "+bitexact", "-flags:a", "+bitexact",
        "-c:a", encoder, *options, "-f", muxer, str(path),
    ]


def probe(path: Path) -> dict:
    return json.loads(run([
        "ffprobe", "-v", "error", "-show_streams", "-show_format", "-of", "json",
        str(path),
    ]))


def build(directory: Path, verify_only: bool = False) -> dict:
    directory.mkdir(parents=True, exist_ok=True)
    manifest = {
        "provenance": "Synthetic 440 Hz tones generated locally; CC0-1.0",
        "duration_seconds": DURATION_SECONDS,
        "ffmpeg": run(["ffmpeg", "-version"]).splitlines()[0],
        "files": [],
    }
    for name, encoder, codec, options in CASES:
        path = directory / name
        command = generation_command(path, encoder, codec, options)
        if not verify_only:
            subprocess.run(command, check=True)
        if not path.is_file():
            raise FileNotFoundError(path)
        metadata = probe(path)
        streams = metadata.get("streams", [])
        if len(streams) != 1:
            raise ValueError(f"Expected exactly one audio stream in {name}")
        stream = streams[0]
        if stream.get("codec_type") != "audio" or stream.get("codec_name") != codec:
            raise ValueError(f"Unexpected codec in {name}: {stream.get('codec_name')}")
        if stream.get("sample_rate") != "44100" or stream.get("channels") != 2:
            raise ValueError(f"Unexpected audio shape in {name}")
        duration = float(metadata["format"]["duration"])
        if not DURATION_SECONDS - 0.25 < duration < DURATION_SECONDS + 0.5:
            raise ValueError(f"Unexpected duration in {name}: {duration}")
        if codec == "mp3" and stream.get("codec_name") != "mp3":
            raise ValueError(f"MP3 verification failed for {name}")
        manifest["files"].append({
            "name": name,
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            "probe": metadata,
        })
    return manifest


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("directory", type=Path)
    parser.add_argument("--verify-only", action="store_true")
    args = parser.parse_args()
    manifest = build(args.directory, args.verify_only)
    manifest_path = args.directory / "manifest.json"
    if args.verify_only:
        previous = json.loads(manifest_path.read_text(encoding="utf-8"))
        expected = [(row["name"], row["sha256"]) for row in manifest["files"]]
        actual = [(row["name"], row["sha256"]) for row in previous.get("files", [])]
        if actual != expected:
            raise SystemExit("Synthetic Android audio fixture manifest does not match its files")
    else:
        manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        (args.directory / "README.txt").write_text(
            "Synthetic 440 Hz stereo tones, CC0-1.0; no personal media.\n",
            encoding="utf-8",
        )
    print("Verified synthetic Android MP3 and FLAC fixtures with ffprobe")


if __name__ == "__main__":
    main()
