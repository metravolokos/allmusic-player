# 002-A — локальный Android аудио-прототип

**Дата:** 30.10.2026. **Issue:** #18. **PR:** [#19](https://github.com/metravolokos/allmusic-player/pull/19), открыт, не объединён. Проверенный кодовый commit: `5e460738dd9880daa2e059844bf28e547394c27c`.

## Результат и проверки

Android-экран воспроизводит встроенный синтетический сигнал 440 Гц через Android media backend Qt Multimedia. Есть воспроизведение/продолжение, пауза, стоп, повтор, статус источника/аудиовыхода и видимые ошибки. Экран прокручивается в узком/альбомном окне. FFmpeg backend исключён; приложение не использует сеть, пользовательские файлы или Android-разрешения. Производственный аудиодвижок не утверждён.

- Локально: `python3 -m unittest discover -s tests/android -v` — **8/8**, `py_compile`, `scripts/check_repository.py` и `git diff --check` — пройдены.
- [Android CI, run 41055782629](https://github.com/metravolokos/allmusic-player/actions/runs/41055782629) — успешно: APK `arm64-v8a` собран и прошёл аудит подписи, выравнивания, API 28–36, нулевых разрешений, состава native-библиотек и отсутствия GMS/Firebase. Android state-control QtTest выполнен как Desktop CTest: 7/7 на Linux и Windows.
- [Desktop CI, run 41055782664](https://github.com/metravolokos/allmusic-player/actions/runs/41055782664) — успешно: Linux 5/5 и Windows 5/5 CTest; Windows ZIP также прошёл аудит.

## APK и ручная проверка

[Скачать APK-комплект](https://github.com/metravolokos/allmusic-player/actions/runs/41055782629/artifacts/12976693161) — 17,936,532 байта, хранится до 13.11.2026. Имя: `AllMusicPlayer-002A-Android-arm64-v8a-debug-ce287d77c2a1-41055782629-1`. Подпись временная debug. Удаление предыдущей копии 002-A может потребоваться перед установкой этой сборки.

Слышимость на Huawei nova 15 Pro / EMUI 15, установка, динамик и гарнитуры — **НЕ ПРОВЕРЕНЫ**: устройство не подключалось к CI. Приёмка слышимого звука остаётся за владельцем; шаги приведены в [инструкции](ANDROID-TEST.md).

**Рекомендация:** PR готов к рассмотрению как Android аудио-прототип; не считать слышимость подтверждённой до ручного прослушивания владельцем.
