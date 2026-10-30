# AllMusic Player — проектная документация

**Статус на 30.10.2026:** исследовательские задачи 000, 001-A, 001-C, 001-B1, 001-B2, 001-D и 001-E завершены в своих согласованных границах. [PR #11](https://github.com/metravolokos/allmusic-player/pull/11) объединён в `main` (`99274b5`); ручная проверка 001-E на Windows 11 пройдена (5/5 потоковых форматов, локальный Stop/restart и no-Range, замечаний нет). Реальный Cloud.Mail.ru и Android-аудио ещё не проверены.

AllMusic Player — персональный музыкальный плеер для Windows, Linux и Android. Музыкальные файлы находятся в папке `/AllMusic` в Облаке Mail, доступ — WebDAV (`https://webdav.cloud.mail.ru`). Ключевые принципы: безопасное чтение, общая кодовая база Qt 6/C++, быстрый локальный каталог, устойчивое потоковое воспроизведение, разумный дисковый кэш и обособленные интерфейсы Desktop/Android.

## С чего начать владельцу проекта

1. Задачи 000 и 001-A завершены и приняты.
2. [Проверка готового Windows-архива](docs/WINDOWS-TEST.md) — основной путь для владельца, Qt и компилятор устанавливать не нужно.
3. [Сборка и автоматические проверки](docs/BUILD.md) — инструкция разработчику; [зависимости и лицензии](docs/DEPENDENCIES.md).
4. Текущие результаты и ограничения находятся в `STATUS.md`. Сборка CI не заменяет ручные испытания Windows 10/11, Linux и Huawei.
5. Учётные данные и личная музыка для локальных тестов не нужны. Стенд 001-A не обращается к облаку.
6. Исследовательские этапы до 001-E приняты в своих границах; следующий функциональный шаг требует отдельного разрешения. Реальный WebDAV — только отдельно согласованное чтение, без передачи реквизитов в GitHub/CI.

## Документы

- `SPEC.md` — что именно делает программа и как проверить результат.
- `ARCHITECTURE.md` — как устроена программа; договорённости между подсистемами.
- `ROADMAP.md` — этапы и критерии их завершения.
- `AGENTS.md` — постоянные инструкции для Codex.
- `DECISIONS.md` — утверждённые, предложенные и исследуемые решения.
- `STATUS.md` — текущее фактическое состояние, без выдуманных успехов.
- `tasks/000-repository-setup.md` — первая задача Codex.
- `tasks/001-A-webdav.md` — проверка ключевых рисков.
- `docs/WEB-DAV-STREAM-TEST.md` — ручная демонстрация 001-B2 на Windows 11.

## Правило организации работы

**Владелец** описывает желаемое поведение и утверждает решения. **ChatGPT** помогает с ТЗ, приоритетами, задачами, проверкой отчётов. **Codex** реализует конкретные задачи, тестирует и предлагает PR. Задача не закончена без перечня проверок и ограничений. К следующему этапу переходить по явному разрешению владельца.

## Официальные технические источники

- Mail WebDAV: https://help.mail.ru/cloud/desktop/webdav/
- Qt Multimedia: https://doc.qt.io/qt-6/qtmultimedia-index.html
- Qt Android: https://doc.qt.io/qt-6/android.html
- Android background playback: https://developer.android.com/media/media3/session/background-playback
- Qt licensing: https://doc.qt.io/qt-6/licensing.html
- Codex development workflows: https://developers.openai.com/cookbook/examples/codex/iterating-development-workflows-with-codex

Документ предназначен для планирования, а не является доказательством работоспособности технологий в целевой среде.

[Установка тестового Android APK на Huawei](docs/ANDROID-TEST.md) — ранний тестовый экран 001-C; запуск минимального APK на Huawei подтверждён владельцем, но воспроизведение звука на Android не проверялось.


## Завершённая задача 001-B1

После завершения 001-C владелец разрешил только локальный Desktop-аудиопрототип.
[Отчёт](docs/001-B1-REPORT.md), [тест звука Windows/Linux](docs/AUDIO-TEST.md).
Qt Multimedia/FFmpeg остаётся кандидатом; Android audio и сеть не добавляются.
Ручная приёмка со слышимым звуком на Windows 11 отражена в `STATUS.md`.

## Завершённые потоковые прототипы и следующие задачи

001-B2 и 001-E проверялись на локальном loopback WebDAV mock, **не** на реальном Cloud.Mail.ru. Материалы: [отчёт 001-B2](docs/001-B2-REPORT.md), [отчёт 001-E](docs/001-E-REPORT.md), [инструкция Windows 11](docs/WEB-DAV-STREAM-TEST.md), [итог приёмки](https://github.com/metravolokos/allmusic-player/pull/11#issuecomment-6518876866).

Следующая функциональная задача: [Issue #12 — локальные «Избранное» и плейлисты](https://github.com/metravolokos/allmusic-player/issues/12); начинать её только по решению владельца. Оптимизация CI [CI-OPT-001, PR #15](https://github.com/metravolokos/allmusic-player/pull/15) уже объединена в `main`; сборки после проверенного PR не повторяются при штатных изменениях. После отдельного разрешения возможна read-only проверка реального WebDAV и Android audio feasibility.
