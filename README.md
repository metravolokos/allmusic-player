# AllMusic Player

**[Русская версия](README.ru.md)**

**A personal, highly specialized music player project for Windows, Linux, and Android.**

AllMusic Player is designed around the owner's individual music library and specific workflows. The source repository is public for transparency and development convenience, **not** because the project is intended to become a commercial product or a general-purpose player for a broad audience.

> **Status: early development / technical prototypes.** This is not a finished application, public release, or supported product.

## Project purpose

The owner determines the requirements, architecture priorities, and acceptance criteria according to personal needs. Features requested by third parties, support for arbitrary installations, and broader usability are not primary development objectives. No public support, compatibility guarantee, or delivery schedule is offered.

## Technology and intended scope

- **Technology:** Qt 6, C++17, QML, CMake; shared application core with separate Desktop and Android interfaces.
- **Platforms:** Windows, Linux, Android.
- **Intended source:** read-only WebDAV access to the owner's cloud-hosted music folder.
- **Audio formats studied:** MP3, FLAC, AAC-LC (ADTS/M4A), and ALAC (M4A).
- **Android Media3 spike for Issue #26:** local MP3/FLAC; WAV is regression-only. ALAC is outside this spike's requirements and acceptance.
- **Future features:** local library, favorites, playlists, offline cache, and resilient streaming.

These describe project goals, **not a list of production-ready features**.

## What has actually been verified?

As of **31 October 2026**:

- The owner completed manual listening tests on Windows 11 for **5/5 synthetic streaming audio formats** using a **local loopback mock WebDAV server**, including Stop/restart and handling a server that does not support HTTP Range.
- Automated tests and prototype builds have passed on Linux and Windows; Android APK builds and packaging checks have passed. The basic Android application screen has been launched manually.
- The owner confirmed sign-in, a `/AllMusic` listing, and a bounded HTTP 206 Range response from the real Cloud Mail account on Windows 11. The narrow Cloud Mail `Content-Range` compatibility was accepted and merged in PR #21 (`f236e7591e969709e4c0cd40fb5196e97c57f27b`).
- The owner verified local Media3 MP3/FLAC playback and controls on Huawei in PR #27. **Not yet verified:** streaming MP3/FLAC over the real Cloud Mail connection on Desktop or Android, Android Cloud Mail playback, production-grade offline caching, or general user compatibility.

Evidence and current limitations: [STATUS.md](STATUS.md), [001-E report](docs/001-E-REPORT.md), [ROADMAP.md](ROADMAP.md).

## Documentation

| Resource | Description |
| --- | --- |
| [SPEC.md](SPEC.md) | Product requirements and acceptance expectations |
| [ARCHITECTURE.md](ARCHITECTURE.md) | Design and subsystem boundaries |
| [STATUS.md](STATUS.md) | Verified progress and unresolved limitations |
| [ROADMAP.md](ROADMAP.md) | Development phases and next steps |
| [DECISIONS.md](DECISIONS.md) | Architectural decisions and open questions |
| [docs/BUILD.md](docs/BUILD.md) | Developer build and test instructions |
| [docs/CI.md](docs/CI.md) | GitHub Actions checks and artifact policy |
| [docs/WINDOWS-TEST.md](docs/WINDOWS-TEST.md) | Owner's manual Windows test procedure |

Source code and test assets do not require access to the owner's personal music or login details. **Do not upload cloud credentials or personal media to issues, pull requests, or CI.**

## Development process

This is an **AI-assisted development project**. The owner defines goals, reviews results, and makes acceptance decisions; ChatGPT assists with requirements, planning and review; Codex implements scoped tasks through pull requests. Automated CI and recorded manual tests are used to evaluate changes. Use of AI is not a substitute for testing.

## Participation and security

This is an owner-directed personal project, **not a community-supported software product**. Outside suggestions or pull requests may be considered, declined, or closed without implementation. Read [CONTRIBUTING.md](CONTRIBUTING.md), [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md), and [SECURITY.md](SECURITY.md) before participating.

## License

**No project-specific software license has been selected or granted yet.** Public access to this repository does not by itself grant permission to reuse, modify, or redistribute its code. Third-party components retain their respective licenses; see [docs/DEPENDENCIES.md](docs/DEPENDENCIES.md). License selection remains a separate decision by the owner.
