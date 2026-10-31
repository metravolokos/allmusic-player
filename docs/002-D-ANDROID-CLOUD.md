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

## Результаты на code head `4804f9c524c7694a7ae0c959544188fcf284657b`

Android CI собрал PR merge commit `0ee15828e9f82a683dc8f600a417f9ae9ddf012c`
как arm64-v8a APK на Qt 6.12.0 / FFmpeg 9.0.1. Успешные прогоны:

- [Android run #78](https://github.com/metravolokos/allmusic-player/actions/runs/41144063840):
  CMake APK build, fixture/package tests, Gradle runtime, итоговый manifest,
  FFmpeg runtime inventory и notices, подпись, ZIP alignment, 16-KiB ELF LOAD
  alignment, APK artifact и Android warm-cache — **успешно**.
- [Desktop run #112](https://github.com/metravolokos/allmusic-player/actions/runs/41144063850):
  Linux и Windows build/tests, Windows ZIP audit, оба warm-cache job и aggregate
  required status — **успешно**.

Первый Android run #76 остановился на проверке упаковки: аудит ошибочно ожидал
Linux-style суффиксы `.so.61`, хотя Android Qt пакет содержит ровно
`libavcodec.so`, `libavformat.so`, `libavutil.so`, `libswresample.so` и
`libswscale.so`. Ожидание уточнено по фактическому APK; run #78 прошёл полный аудит.

Фактический APK: **63 649 117 байт (60.70 MiB)**. В 002-A APK было
17 936 522 байта (run #41058685269): прирост **45 712 595 байт**, или примерно
**3.55 раза**. FFmpeg plugin присутствует, Android system media plugin отсутствует,
`QT_MEDIA_BACKEND=ffmpeg`; APK audit подтвердил arm64-v8a, ноль permissions,
Qt/FFmpeg attribution и notices, подпись и native alignment. Состав включает
`libplugins_multimedia_ffmpegmediaplugin_arm64-v8a.so` и пять указанных выше
FFmpeg библиотек. Qt attribution фиксирует FFmpeg 9.0.1 и LGPL-2.1-or-later с
BSD/ISC/MIT/MPL компонентами; GPL/LGPLv3-only компоненты в официальной Qt сборке
отсутствуют. Это вывод о закреплённой Qt Android package metadata, не юридическое
заключение для будущего публичного релиза.

Проверочный artifact:
[AllMusicPlayer-002D-QtFFmpeg-Android-arm64-v8a-debug-0ee15828e9f8-41144063840-1](https://github.com/metravolokos/allmusic-player/actions/runs/41144063840/artifacts/13019789056)
с APK, инструкцией, audit evidence и license bundle доступен до 13.11.2026.
Размер ZIP-комплекта Actions — 27 201 099 байт; APK внутри — 63 649 117 байт.

- Локально пройдены `python3 -m unittest discover -s tests/android -v`
  (12 тестов), `python3 -m unittest discover -s tests/ci -v` (19 тестов),
  `python3 scripts/check_repository.py`, Python compile изменённых модулей и
  `git diff --check`.
- Независимый read-only аудит Qt 6.12 подтвердил разделение FFmpeg media backend
  и Android audio-device слоя.
- **Android APK/C++ сборка локально не выполнялась:** в рабочей среде нет CMake,
  Ninja, Android SDK и adb; сборку выполнил CI.
- **Huawei не подключён и playback не был фактически запущен на физическом
  устройстве.** Поэтому слышимый звук, состояние `Qt FFmpeg подтверждён runtime-
  сообщением` и Play/Pause/Stop/Seek из QBuffer остаются НЕ ПРОВЕРЕНЫ на устройстве.
  Пакетный аудит доказывает, что Qt FFmpeg — единственный собранный media plugin,
  но не доказывает слышимый PCM. Ручные шаги: [ANDROID-FFMPEG-SPIKE-TEST.md](ANDROID-FFMPEG-SPIKE-TEST.md).

## Границы вывода

Изолированный spike выполнил сборочную часть gateway. Прирост APK примерно в 3.55
раза — значимая стоимость; функциональную Android Cloud интеграцию в этом PR не
начинать. Рекомендуется после ручного испытания Huawei отдельно решить, приемлема
ли эта цена для сохранения QMediaPlayer/QIODevice пути. Альтернатива — отдельным
разрешённым экспериментом проверить Android system backend на MP3/FLAC из
seekable `QIODevice`; его поддержка пока неизвестна. Размер, состав и successful
CI не подтверждают sound/playback acceptance. D-008/D-009 остаются открытыми.
