# 002-D — Android Qt FFmpeg feasibility spike

## Граница этой проверки

По Issue #24 проверяется только локальный Android audio backend как шлюз перед
любой интеграцией Cloud Mail. Ветка не добавляет WebDAV, сеть, разрешения Android,
чтение пользовательских файлов или полноценный телефонный плеер. Основной desktop
код не меняется; прежний короткий WAV-тест 002-A остаётся доступен в отдельном
режиме Android экрана. У spike отдельный package ID, поэтому его можно установить
рядом с 002-A.

Сборка явно подключает Qt FFmpeg media plugin, выбирает backend `ffmpeg` и
исключает Qt Android media plugin. В Qt 6.12 аудиовывод использует отдельный
Android audio-device слой из Qt Multimedia; наличие Android media plugin для
вывода декодированного звука не требуется. Экран показывает runtime-сообщение
Qt FFmpeg. Аудит итогового APK проверяет plugin и точные имена FFmpeg runtime-библиотек,
arm64-v8a, manifest, notices, подпись, 16-KiB ELF alignment и размер.

Скрипт генерирует короткие синтетические стерео MP3 и FLAC тоны 440 Гц с
пометкой CC0-1.0; FFmpeg/ffprobe нужны только при Android-сборке. Файлы встраиваются
как ресурсы. Контроллер открывает каждый compressed fixture в read-only `QBuffer`
и передаёт его в `QMediaPlayer::setSourceDevice`; UI предоставляет Play/Pause,
Stop, повтор и seek. Это проверяет именно seekable `QIODevice` путь.

## Результаты на коммите `b655394b988d6c31ffcca02524e66398584d3183`

- Локально пройдены `python3 -m unittest discover -s tests/android -v`
  (12 тестов), `python3 -m unittest discover -s tests/ci -v` (19 тестов),
  `python3 scripts/check_repository.py`, Python compile изменённых модулей и
  `git diff --check`.
- Независимый read-only аудит Qt 6.12 подтвердил разделение FFmpeg media backend
  и Android audio-device слоя.
- **Android APK/C++ сборка локально не выполнялась:** в рабочей среде нет CMake,
  Ninja, Android SDK и adb. Проверка ожидает Android и Desktop CI PR #25.
- **Содержимое реального APK, размер и APK artifact пока не получены.** Их статус
  обновляется после завершения workflow.
- **Звук и управление на физическом Huawei не проверены.** APK предназначен для
  ручной проверки владельцем; инструкция: [ANDROID-FFMPEG-SPIKE-TEST.md](ANDROID-FFMPEG-SPIKE-TEST.md).

## Границы вывода

Успешная сборка и сообщение о версии FFmpeg подтверждают выбор backend и упаковку,
но не слышимый звук на устройстве. Даже успешная проверка MP3/FLAC на Huawei
относится только к открытому локальному сценарию этого spike. Она не закрывает
остальные требования Issue #24, не выбирает production audio engine и не разрешает
следующий этап интеграции Cloud Mail. D-008/D-009 остаются открытыми.
