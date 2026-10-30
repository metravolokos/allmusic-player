# Текущий статус проекта — AllMusic Player

**Дата:** 30.10.2026. **Текущий статус:** 002-A реализована и принята владельцем в [PR #19](https://github.com/metravolokos/allmusic-player/pull/19); обязательные CI успешны, слышимость Android на Huawei проверена в границах короткого локального теста. Issue #20 отдельно разрешает работу над 002-B. Реальные запросы Cloud Mail выполняет только владелец из Windows-приложения; пока его ручной проверки нет, live WebDAV остаётся **НЕ ПРОВЕРЕНО**. История 001-E: принята владельцем на Windows 11 (5/5 потоковых форматов, локальный Stop/restart, отказ сервера без Range; без замечаний). [PR #11](https://github.com/metravolokos/allmusic-player/pull/11) объединён в `main` (`99274b51469907b515715fcd048f6345acb3972b`). Post-merge Desktop CI для PR #11 завершилась успешно. [CI-OPT-001, PR #15](https://github.com/metravolokos/allmusic-player/pull/15) объединён в `main` (`2f71fb0`); первые post-merge Desktop/Android CI успешны, повторные тяжёлые сборки пропущены. Следующие этапы требуют отдельного согласования.

## Утверждено и подготовлено

- Документация v0.1 утверждена; [PR #1](https://github.com/metravolokos/allmusic-player/pull/1) объединён в `main` (подтверждено через GitHub).
- D-005 (облако только на чтение) и D-006 (Qt/C++/QML/CMake) утверждены 29.10.2026. Статусы остальных исходных решений сохранены. Новые прямые утверждения задачи 000 отражены в D-021–D-025.
- [PR #2](https://github.com/metravolokos/allmusic-player/pull/2) объединён в `main`, merge commit `1746065852904d6e19e709e804ce73eec5c6a764`. Завершение задачи 000 подтверждено владельцем 29.10.2026.
- Созданы CMake-проект и preset, `src/app/main.cpp`, `ui/desktop/Main.qml`: только экран «AllMusic Player — проект запущен».
- Добавлены `.gitignore`, проверки документов и GitHub Actions для Ubuntu 24.04 и Windows x64.
- Добавлены тест отрисовки с PNG, Windows-упаковка Qt DLL/QML/плагинов и app-local VC runtime, уведомления о лицензиях, контрольные суммы и запуск упакованной копии с очищенными путями разработчика.
- Обновлены SPEC, ARCHITECTURE, ROADMAP, DECISIONS и README: Windows 10/11, Linux CI/пользовательские дистрибутивы, ранняя проверка Huawei, доступность устройств по Москве.
- Инструкции: [сборка](docs/BUILD.md), [проверка Windows-архива](docs/WINDOWS-TEST.md), [зависимости и лицензии](docs/DEPENDENCIES.md).

## Реально использованные инструменты

| Область | Версии |
|---|---|
| Общие | Открытая Qt 6.12.0, C++17, CMake 3.30.5, Ninja 1.12.1 |
| Linux CI | Ubuntu 24.04.5 x64, GCC 13.3.0 (`13.3.0-6ubuntu2~24.04.1`) |
| Windows CI | Windows Server 2022 x64, MSVC 19.44.35229.0, toolset 14.44 |
| Python | Подготовка Linux 3.12.15 / Windows 3.12.10; установщик Qt 3.15.1 |
| Установка Qt | install-qt-action v4.5.0 (SHA в workflow); Linux aqt 3.3.0; Windows aqt 3.3.1.dev166, commit `076e1659807d0b362a3ed684d54c2e9c775eb9c7` |
| Дополнительные проверки | actionlint 1.7.7; PyYAML 6.0.3 для чтения workflow |

Образы CI и системные пакеты обновляются; точные версии выводятся в журнале.
Это не обещание побитовой воспроизводимости.

## Фактически исполненные проверки

[Успешный запуск CI 40877776037](https://github.com/metravolokos/allmusic-player/actions/runs/40877776037),
проверенный код `0b32b37645333a973de7cbe71df2b23851770406`:

| Проверка | Результат |
|---|---|
| Документы: `python3 scripts/check_repository.py`, `git diff --check` | ПРОЙДЕНО, также отдельное задание GitHub Actions |
| Ubuntu: `cmake --preset desktop-release`, `cmake --build --preset desktop-release --parallel 2` | ПРОЙДЕНО |
| Ubuntu: `QT_QPA_PLATFORM=xcb xvfb-run -a ctest --preset desktop-release` | ПРОЙДЕНО, 1/1 тест, 0.28 сек |
| Windows: те же команды конфигурации и сборки, затем `ctest --preset desktop-release` | ПРОЙДЕНО, 1/1 тест, 0.39 сек |
| Windows: `./scripts/package-windows.ps1` | ПРОЙДЕНО: установка в отдельную папку, windeployqt, runtime/лицензии, запуск упакованного EXE с очищенными путями, ZIP |
| Синтаксис workflow: `docker run --rm -i rhysd/actionlint:1.7.7 -shellcheck= -pyflakes= - < .github/workflows/desktop.yml` | ПРОЙДЕНО |
| Python: `python3 -m py_compile scripts/check_repository.py scripts/collect-licenses.py` | ПРОЙДЕНО |
| Сбор уведомлений `collect-licenses.py` на официальных исходниках Qt Base/Declarative | ПРОЙДЕНО локально и в Windows CI |
| `.gitignore`: `git check-ignore .env secrets.yaml credentials.json build/desktop-release/CMakeCache.txt Android/sdk/local.properties personal.mp3` | Все перечисленные пути исключены |

Тест окна требует отрисованного кадра и успешного сохранения PNG; раннее закрытие
без снимка не считается успехом. В CI используется программная отрисовка.
Снимки и логи сохранены в `diagnostics-ubuntu-24.04` и `diagnostics-windows-2022`.
Это автоматическая проверка; ручная визуальная приёмка текста и поведения отдельно.

## Готовый Windows-архив

[AllMusicPlayer-Windows-x64, артефакт проверенного запуска](https://github.com/metravolokos/allmusic-player/actions/runs/40877776037/artifacts/12900165165).
Размер артефакта около 35 МБ; срок хранения GitHub ограничен, в этом запуске до 27.01.2027.
Внутри ZIP приложения; все файлы нужно распаковать вместе. Подробные шаги в
[инструкции владельцу](docs/WINDOWS-TEST.md). Установка Qt/IDE не требуется.

Лог развёртывания подтверждает Qt6Core, Gui, Network, OpenGL, Qml, QmlMeta,
QmlModels, QmlWorkerScript и Quick. Qt SVG и необязательные плагины изображений/
иконок исключены. Добавлены D3DCompiler и VC143 runtime Microsoft.

## Исторические ограничения и препятствия задачи 000

- **НЕ ПРОВЕРЕНО на устройствах:** Windows 11, ALT Regular, CachyOS, Huawei nova 15 Pro / EMUI 15. Успех Windows Server CI не означает поддержку пользовательских Windows 10/11.
- **НЕ ПРОВЕРЕНО:** реальные GPU/масштабирование, Wayland/X11 пользователя, Android API Level/ABI, APK, отсутствие зависимости от GMS, фон и энергосбережение EMUI. В задаче 000 Android SDK/NDK не устанавливались; текущее состояние Android — в разделе 001-C ниже.
- WebDAV, звук, кодеки, seek/gapless, БД и кэши не реализованы и не испытывались. Личное облако не использовалось, аудиофайлы и секреты в изменения не добавлялись.
- Исходная среда Debian 13 блокирует `download.qt.io` (403 CONNECT). Установка открытых бинарных Qt 6.12.0 успешно проверена в обоих GitHub runner; версию Qt не меняли.
- Windows-репозиторий Qt >=6.11 разделён по компиляторам. aqt 3.3.0 ищет старый путь и получает 404. Использована закреплённая официальная ревизия aqt с поддержкой нового формата. Риск — версия установщика пока из исходников; перейти на подходящий опубликованный релиз отдельным проверенным обновлением. Разделение задачи на 000A/000B не потребовалось.
- CMake сообщает о недоступной необязательной связке `Qt6QmlAssetDownloaderPrivate` / `Qt6TaskTree` из установленного Qt. Приложение её не использует; конфигурация, сборка и запуск проходят. Дополнительный модуль не подключали.
- Windows EXE пока не подписан. Перед публичным распространением нужны полный контроль лицензий/соответствующих исходников, условия runtime Microsoft и выбор лицензии приложения. Архив CI — для испытаний владельцем, не объявляется готовым публичным релизом.
- Дополнительную сборку Qt из исходников в контейнере Ubuntu 24.04 начали из-за сетевого ограничения, затем остановили после успеха Linux CI. Полная сборка Qt из исходников не заявляется.

## Ручная проверка Windows 10

Владелец подтвердил запуск на физическом компьютере Windows 10 и видимый экран «AllMusic Player — проект запущен». Подробности: [комментарий к PR #2](https://github.com/metravolokos/allmusic-player/pull/2#issuecomment-6497659321).
Проверен архив запуска CI 40877776037; последующие изменения PR затронули только документацию. Финальный CI 40878772126 успешен. Изменение размера и закрытие окна этим сообщением не подтверждены; звук и WebDAV не проверялись.

## Следующее действие

Утверждён порядок 001: A — mock WebDAV; C — ранний минимальный APK Huawei; B — воспроизведение; D — архитектурные выводы. Основной приоритет ручных испытаний — Windows 10.
001-A завершена и принята. Разрешена только 001-C в отдельной ветке и PR; 001-B/001-D не разрешены.
Личную медиатеку и пароль не использовать в автоматических тестах; реальное облако не изменять. Реальные проверки чтения требуют отдельного согласования и безопасной локальной настройки. Текущая подготовка не подтверждает технические возможности Mail WebDAV или аудиодвижка.

## 001-A — реализация и проверка

План 001 утверждён владельцем 29.10.2026: A → C → B → D. 001-A завершена; автоматических переходов нет. Подготовлены [задача](tasks/001-A-webdav.md), локальный mock и Qt Network клиент, автоматические Windows/Linux сценарии. Проверки реализации завершены успешно; 001-A принята владельцем 29.10.2026. Результаты и понятные выводы — [отчёт](docs/001-A-REPORT.md). Реальное облако и реквизиты не использовались; QMediaPlayer не выбран. Переход к 001-C разрешён владельцем отдельно.

### Подтверждённые результаты 001-A

- [CI 40885314679](https://github.com/metravolokos/allmusic-player/actions/runs/40885314679), код `59d15745ca985fe8a4e13e0ccc81bff9d633f287`: документы, Ubuntu/GCC 13.3.0 и Windows Server/MSVC 19.44.35229.0 — успешно, на обеих ОС 2/2 CTest.
- Linux QtTest: 86 passed, 0 failed, 0 skipped; отрицательные пути/редиректы/TLS и аудит допустимых методов включены. Windows webdav-safety также завершён с exit 0.
- [Windows-стенд](https://github.com/metravolokos/allmusic-player/actions/runs/40885314679/artifacts/12902911480) упакован и проверен с очищенным PATH; инструкции и понятные выводы — в отчёте.
- Клиент ограничен loopback; личные данные и реальное облако не использовались. В проверенных сценариях выходов за /AllMusic и запросов записи от клиента нет.
- Ручная проверка 001-A на Windows 10 завершилась успешно: **86 passed, 0 failed** (сообщение владельца 29.10.2026; это отдельный результат от CI). Windows 11 и пользовательский Linux НЕ ПРОВЕРЕНЫ; реальные возможности Mail, звук и Android НЕ ПРОВЕРЕНЫ.
- [PR #3](https://github.com/metravolokos/allmusic-player/pull/3) и [PR #4](https://github.com/metravolokos/allmusic-player/pull/4) объединены 29.10.2026; main `60035278fd79241a781e9d786a4863c42c9fda18`. Владелец принял 001-A и отдельно разрешил реализацию 001-C.

## 001-C — минимальный Android APK

План Issue #5 утверждён владельцем 29.10.2026 с уточнениями: отдельная ветка,
Android CI по `pull_request` с фильтром путей и `workflow_dispatch`, первоначальный
ABI `arm64-v8a` разрешён без предположений о телефоне. Windows/Linux сохраняются.
Подробности и результаты — [отчёт](docs/001-C-REPORT.md),
[инструкция Huawei](docs/ANDROID-TEST.md), [задача](tasks/001-C-android.md).

Экран без звука, сети, доступа к личным файлам, медиатеки, кэша и служб.
Совместимость Huawei nova 15 Pro / EMUI 15, его API/ABI, установка и реальная
отрисовка **НЕ ПРОВЕРЕНЫ**. Сборка CI не заменяет приёмку владельцем.
001-B/001-D и реальное облако не разрешены. PR не объединять автоматически.

### Проверенный Android артефакт 001-C

[Android CI 40899932754](https://github.com/metravolokos/allmusic-player/actions/runs/40899932754)
успешен для head `09cca4e`, проверенный merge SHA `b2f012e16ac6`.
[Скачать APK kit](https://github.com/metravolokos/allmusic-player/actions/runs/40899932754/artifacts/12908837814),
около 17 МБ, до 12.11.2026; метка `001-C-b2f012e16ac6-40899932754-1`.
Пройдены компиляция/упаковка, 4 теста аудитора, подпись, ZIP/ELF 16 KiB alignment,
arm64-v8a, min API 28/target 36, нулевой набор разрешений, единственная QtActivity
без служб/receiver/provider, отсутствие GMS/Firebase в manifest/runtime/DEX.
Windows/Linux [CI 40898773590](https://github.com/metravolokos/allmusic-player/actions/runs/40898773590)
успешен: 2/2 CTest на каждой платформе, Linux QtTest 86 passed, 0 failed,
Windows упаковка приложения и WebDAV kit. Подробности/команды — в отчёте.

[PR #6](https://github.com/metravolokos/allmusic-player/pull/6) открыт; не объединять
без решения владельца. Huawei остаётся **НЕ ПРОВЕРЕНО**. Документационный
коммит после указанной сборки не меняет прикладной код APK. GitHub paths
оценивает полный diff PR, поэтому такой коммит в Android PR может повторить CI;
новый запуск получает отдельный артефакт, старый сохраняет исходную маркировку.


## 001-B1 — локальное воспроизведение (основная ручная приёмка Windows 11 пройдена)

Владелец сообщил о завершении 001-A/001-C и разрешил 001-B1 29.10.2026.
PR #6 объединён; актуальный main `6bc62c12460b80b917e4f1aed9fc545b2c9db1ec`.
Исторические запреты 001-B выше заменены этим точечным разрешением; B2/D,
Android audio и реальное облако не разрешены.

Реализован Desktop-кандидат QMediaPlayer/FFmpeg: локальный выбор,
play/pause/stop/seek/volume/время/ошибки; синтетические фикстуры пяти
кодеков/контейнеров; Qt PCM-тесты; расширенная Windows-упаковка.
[Задача](tasks/001-B1-local-audio.md), [отчёт](docs/001-B1-REPORT.md),
[ручные испытания](docs/AUDIO-TEST.md).

Локально: генерация и ffprobe/SHA256 — ПРОЙДЕНО; 4 теста Android-аудитора —
ПРОЙДЕНО; документы и whitespace — ПРОЙДЕНО. Qt SDK/CMake здесь отсутствуют.
Проверки CI и ссылки на ZIP — в отчёте 001-B1 ниже. Слышимый звук и управление
**на физической Windows 11 подтверждены владельцем (8/8 сценариев)**. Windows 10
и пользовательский Linux отдельно не проверялись. Окончательный движок не выбран.
До объединения PR и запуска 001-B2 необходимо отдельное решение владельца.

### Уточнение принятой 001-C

В [Issue #7](https://github.com/metravolokos/allmusic-player/issues/7) владелец
зафиксировал проверку прежнего APK на Huawei: установка, повороты, сворачивание,
блокировка и повторный запуск без сети. Исторические пометки НЕ ПРОВЕРЕНО выше
описывали состояние до этой ручной проверки. Это не проверка Android-звука и
не ручное испытание нового регрессионного APK из PR #8.

### Подтверждённые результаты 001-B1 (PR #8)

[PR #8](https://github.com/metravolokos/allmusic-player/pull/8), head df1846a,
проверенный merge 4c59f2b4b561:

- [Desktop CI 40928437106](https://github.com/metravolokos/allmusic-player/actions/runs/40928437106):
  Ubuntu/GCC 13.3.0 и Windows Server/MSVC 19.44.35229.0 — сборка и 3/3 CTest
  на каждой ОС. Linux Playback 9 passed / 0 failed; WebDAV 86 passed / 0 failed.
  На Windows тот же playback-decode завершён с exit 0. Qt 6.12.0 / FFmpeg 9.0.1.
- Windows: полный ZIP, повтор PCM-теста из отдельной папки с очищенными путями
  и снимок упакованного окна — ПРОЙДЕНО. WebDAV kit также упакован/проверен.
  [Windows x64 архив](https://github.com/metravolokos/allmusic-player/actions/runs/40928437106/artifacts/12922830320),
  около 79 МБ, до 27.01.2027. Инструкция AUDIO-TEST.md включена в архив.
- [Android CI 40928437092](https://github.com/metravolokos/allmusic-player/actions/runs/40928437092):
  APK и строгий аудит — ПРОЙДЕНО. Сохранены нулевые разрешения, arm64-v8a,
  min API 28/target 36, подпись, 16 KiB alignment и прежний набор модулей;
  без GMS/Firebase/служб и Desktop-аудио.
- Локальные ffprobe/SHA256, 4 теста Android-аудитора, документы/whitespace
  пройдены; синтетические фикстуры в Git не включены, доступны в артефактах.
- Слышимый звук и базовое UI-управление на **Windows 11** — ПРОЙДЕНО,
  по сообщению владельца (8/8 ручных проверок). На **Windows 10** и
  пользовательском Linux B1 отдельно не тестировалась; другие аудиоустройства,
  VBR/HE-AAC/прочие профили — НЕ ПРОВЕРЕНО.
  Новый регрессионный APK на Huawei в этой задаче не устанавливался.

D-008/D-009 остаются ПРОВЕРИТЬ. [Полный отчёт](docs/001-B1-REPORT.md).
Архив предназначен для ручной приёмки, не для объявления публичного релиза.
После отчёта работа остановлена; 001-B2/001-D не начинать без разрешения.
Позднейший коммит обновляет только документацию и не меняет проверенные бинарники.

## Актуальное решение о Windows и результат 001-B1 — 29.10.2026

Владелец утвердил D-026: для текущих и будущих задач категория **Windows Desktop
(11/10)** принимается по ручным испытаниям на **Windows 11**; Windows 10 больше
не является приоритетом и не требует обязательного дублирования каждого теста.
Это не подтверждение фактической совместимости B1 с Windows 10.

Владелец сообщил о **8/8 успешных ручных сценариев** с физическим звуком на
Windows 11: MP3, FLAC, AAC ADTS, AAC M4A, ALAC M4A, пауза/продолжение/стоп,
перемотка/громкость, повторный запуск. [Результат в PR #8](https://github.com/metravolokos/allmusic-player/pull/8#issuecomment-6505572332).
[Финальный Desktop CI 40929848648](https://github.com/metravolokos/allmusic-player/actions/runs/40929848648)
и [Android CI 40929848919](https://github.com/metravolokos/allmusic-player/actions/runs/40929848919)
на head `677a8626a35c55b5e88359fb3b5788d7af8aaf9a` — success.
Проверки повреждённого файла/офлайна на физическом ПК и Linux остаются отдельно
неподтверждёнными, не блокируют обычную Windows-приёмку B1 по D-026.
PR #8 готов к рассмотрению; сам факт этих результатов **не разрешает merge**.
001-B2 начинать только после решения владельца.

## 001-B2 — текущая работа

Разрешение владельца получено 29.10.2026. Scope ограничен loopback mock и
`/AllMusic`; без реального Mail, авторизации, записи/удаления, Android audio,
медиатеки, постоянного аудиокэша и gapless.

Реализованы `WebDavRangeDevice` (64-КиБ страницы, максимум 4 страницы в RAM,
ETag/If-Range, fail-closed при HTTP 200), Desktop-интеграция, loopback mock и
Windows one-click launcher. Ветка `task/001-b2-webdav-stream` создана от PR #8
merge `b2232ff`; текущий PR #9.

[Desktop CI 40943552384](https://github.com/metravolokos/allmusic-player/actions/runs/40943552384)
на head `f4bb308` прошёл Linux и Windows: Linux 4/4 CTest, включая streaming
10/10 и WebDAV 86/86; Windows сборка, тесты, smoke и Windows x64 архив прошли.
[Android CI 40943552169](https://github.com/metravolokos/allmusic-player/actions/runs/40943552169)
прошёл как регрессия 001-C. Архив доступен в [AllMusicPlayer-Windows-x64](https://github.com/metravolokos/allmusic-player/actions/runs/40943552384/artifacts/12927309414)
и [WebDAV kit](https://github.com/metravolokos/allmusic-player/actions/runs/40943552384/artifacts/12928921132).

Измерение Qt 6.12.0 / FFmpeg 9.0.1 на Linux: 31 мс до первого ненулевого PCM,
65 536 байт Range payload из 385 604 байт файла, кэш адаптера 254 532 байта
(предел 262 144), 9 216 PCM frames, peak 0.0441908, 4 HTTP requests. Автотест
подтвердил seek на незагруженную область; 200, смена ETag, неожиданный сервер,
обрыв и отмена обрабатываются fail-closed. Это декодирование PCM, не слышимый
звук. Точный размер внутреннего кэша FFmpeg неизвестен.

Основная ручная функциональная приёмка B2 на **Windows 11 — ПРОЙДЕНА (5/5)**,
по сообщению владельца 29.10.2026: слышимый потоковый FLAC, пауза/продолжение/стоп,
seek к ~25-й секунде, новый GET Range, ошибка HTTP 200 с прекращением воспроизведения.
[Протокол владельца в PR #9](https://github.com/metravolokos/allmusic-player/pull/9#issuecomment-6507500307).
[Desktop CI 40944887711](https://github.com/metravolokos/allmusic-player/actions/runs/40944887711)
и [Android CI 40944887824](https://github.com/metravolokos/allmusic-player/actions/runs/40944887824)
для проверенного head `ab8320a` — success; 4/4 проверки PR зелёные.
Решение о приёмке ограничено loopback mock, без реального Mail, Android audio,
оценки внешней сети и полного спектра потоковых форматов. D-027 остаётся
исследовательским вопросом для настоящего WebDAV и выбора архитектуры.
[Задача](tasks/001-B2-webdav-stream.md), [инструкция](docs/WEB-DAV-STREAM-TEST.md),
[отчёт](docs/001-B2-REPORT.md). PR #9 не объединять без отдельного разрешения владельца.


## 001-D — архитектурные выводы после 001-A, 001-B1 и 001-B2

Задача выполнена документационно; код прототипов не переписывался. PR #9 объединён в main коммитом 93e8cb8d7314673052f141940d2bd896188b2fac. Post-merge [Desktop CI run 40951045356](https://github.com/metravolokos/allmusic-player/actions/runs/40951045356) завершился success: документация, Ubuntu/Linux и Windows Server jobs успешны. Android audio эта проверка не покрывала.

Подготовлены архитектурные уточнения в ARCHITECTURE.md, предложения D-028/D-029 в DECISIONS.md, обновление этапа 1 и дальнейших приоритетов в ROADMAP.md, постоянное правило языка в AGENTS.md и отдельный отчёт 001-D. Производственные решения остаются предложениями; окончательный движок не выбран.

**Проверенные факты:** локальные MP3, FLAC, AAC-LC ADTS/M4A и ALAC M4A с синтетическими файлами дали PCM в Qt 6.12.0 / FFmpeg 9.0.1; владелец подтвердил звук этих образцов на Windows 11. На loopback mock потоковый FLAC через QMediaPlayer::setSourceDevice() дал PCM на CI; владелец подтвердил слышимый звук, pause/seek и отказ при HTTP 200 вместо 206. Range-клиент валидирует 206, Content-Range, размер и ETag; адаптер держит до четырёх страниц по 64 КиБ (256 КиБ) и останавливается при ошибке.

**Не проверено:** настоящий Cloud.Mail.ru, реальные интернет-задержки и потеря сети, потоковые MP3/AAC/ALAC, пользовательский Linux, Android audio/фон/аудиофокус, gapless и общий размер памяти Qt/FFmpeg/приложения. Число 256 КиБ относится только к кэшу WebDavRangeDevice, не ко всему плееру.

**Рекомендация:** оставить Qt Multimedia / QMediaPlayer Desktop-кандидатом за сменной абстракцией; отложить собственный PCM pipeline до измеренного основания. Между provider и движком закрепить асинхронный контроллер диапазонов. HTTP 200 не превращать молча в полную загрузку; полную загрузку можно предложить отдельным явным действием. Постоянный кэш и медиатеку не реализовывать до проверки источника, сетевой устойчивости и Android audio.

Предлагаемые следующие этапы: локальная матрица streaming-форматов и искусственной сетевой деградации; отдельная разрешённая read-only проверка настоящего Cloud.Mail.ru; Android audio feasibility; затем владелец утверждает контракты и разрешает Desktop вертикальный срез/медиатеку. Реальное облако в 001-D не использовалось; пароли, токены и музыка не запрашивались и не сохранялись.


## 001-E — результаты CI и статус PR (30.10.2026)

PR #10 объединён в `main` коммитом `d2c599d16f177e5f232869c0e1ab89d0759a3888`;
post-merge Desktop CI run 40954984722 успешен. PR #11 для 001-E открыт и ожидает
приёмки владельца: [потоковые форматы и устойчивость к сбоям сети](https://github.com/metravolokos/allmusic-player/pull/11).

Desktop CI run 40969906289 успешен: Ubuntu 24.04 и Windows Server 2022, CTest 4/4
на обеих платформах; Linux streaming 24/24, WebDAV regression 86/86, Windows streaming
24/24 за 25,09 с. Android APK workflow run 40969906265 также успешен.

Несколько промежуточных Windows прогонов выявили тестовую гонку: отложенный Range от
предыдущего slow ALAC PCM сценария попадал в тест 429 и нарушал предположение о порядке
запросов. Источник теперь явно отменяется после отсоединения плеера, а проверка требует
повтор именно диапазона, получившего 429. Последний Windows CTest прошёл полностью.
Созданы автономный [Windows x64 ZIP](https://github.com/metravolokos/allmusic-player/actions/runs/40969906289/artifacts/12940192439)
и [WebDAV loopback kit](https://github.com/metravolokos/allmusic-player/actions/runs/40969906289/artifacts/12940880129); [APK](https://github.com/metravolokos/allmusic-player/actions/runs/40969906265/artifacts/12940461473) сохранён.

Синтетические MP3, FLAC, AAC-LC ADTS/M4A и ALAC M4A выдали PCM до полной загрузки,
прошли seek и завершились; подробные Linux метрики и пределы измерений — в
[отчёте 001-E](docs/001-E-REPORT.md). Пользовательский Stop теперь проходит через
контроллер: активный Range отменяется до остановки и отсоединения источника.
Автотест проверяет, что активный mock-ответ действительно закрыт (Linux 0 мс,
Windows 2 мс при естественной задержке 2 500 мс), и что выбранный сетевой трек
можно запустить заново. Локальный Stop сохраняет источник и тоже проверен на
повторный запуск. Смена трека и уничтожение источника с активной загрузкой
прошли в CI без наблюдавшихся зависаний или ошибок времени жизни.

На момент первичного отчёта ручное прослушивание на Windows 11 ожидало владельца; затем успешно проведено 30.10.2026 — см. итог ниже. PCM callback
gaps не доказывают и не опровергают слышимые прерывания. Ни настоящий Cloud.Mail.ru,
ни учётные данные, ни пользовательские файлы не использовались. D-028/D-029
остаются предложениями; архитектурный выбор не утверждён. Прототип не готов к
прямому подключению к реальному WebDAV: текущий клиент работает только с loopback
mock, автоматическое продолжение того же источника после сетевого обрыва не
проверяется как готовое пользовательское поведение. Следующая такая проверка
требует отдельного решения владельца.


## 001-E — итоговая ручная приёмка и объединение (30.10.2026)

**Решение владельца: ПРИНЯТО** в границах физической Windows 11 и локального loopback WebDAV mock. На Windows 11 владелец подтвердил воспроизведение всех пяти потоковых форматов: MP3, FLAC, AAC-LC ADTS, AAC-LC M4A, ALAC M4A. Дополнительно проверены локальный MP3 Stop/restart и корректная реакция на отсутствие Range (HTTP 200 вместо 206). **Замечаний нет.**

- [Протокол ручной приёмки владельца в PR #11](https://github.com/metravolokos/allmusic-player/pull/11#issuecomment-6518876866).
- [PR #11](https://github.com/metravolokos/allmusic-player/pull/11) объединён в `main` коммитом [99274b51469907b515715fcd048f6345acb3972b](https://github.com/metravolokos/allmusic-player/commit/99274b51469907b515715fcd048f6345acb3972b).
- Проверенный head `b86e8995b97d97951d91ae17352e36d96f4f157d`: [Desktop CI 40981125301](https://github.com/metravolokos/allmusic-player/actions/runs/40981125301) успешен (CTest 4/4 на Linux и Windows, streaming QtTest 25/25, Windows ZIP); [Android APK CI 40981125207](https://github.com/metravolokos/allmusic-player/actions/runs/40981125207) успешен (только проверка сборки APK).
- [Post-merge Desktop CI 41029396796](https://github.com/metravolokos/allmusic-player/actions/runs/41029396796) завершился **успешно**.

**Не подтверждено:** настоящий Cloud.Mail.ru, интернет-соединение вне mock, пользовательский Linux, Android audio и gapless; окончательные production-контракты и fallback ещё не утверждены. В [Issue #13](https://github.com/metravolokos/allmusic-player/issues/13) отдельно поставлена задача CI-OPT-001 по безопасному кэшированию инструментов; она не выполнялась в рамках PR #11 и позднее завершена отдельно в PR #15. Следующие функциональные этапы не запускаются без разрешения владельца.


## 002-A — локальное аудио Android (Issue #18)

[PR #19](https://github.com/metravolokos/allmusic-player/pull/19) открыт от ветки
`codex/002-a-android-audio`, основанной на актуальном `main`. Проверенный head-код:
`8a3c075f289b44403bf5238853b1f63fccc052e0` (финальная проверенная кодовая версия перед документированием приёмки). PR не объединён.

- [Desktop CI run 41058685268](https://github.com/metravolokos/allmusic-player/actions/runs/41058685268): **успешно**, Ubuntu 24.04 и Windows Server 2022, 5/5 CTest на каждой платформе; Windows ZIP audit пройден.
- [Android CI run 41058685269](https://github.com/metravolokos/allmusic-player/actions/runs/41058685269): **успешно**. Собран и проверен APK `arm64-v8a`, min API 28 / target 36; 8 Python unit tests пройдены; APK audit подтвердил подпись, alignment, нулевые разрешения, отсутствие GMS/Firebase и FFmpeg runtime. В Android UI используется только Qt Android media plugin.
- [APK artifact, 17,936,522 bytes](https://github.com/metravolokos/allmusic-player/actions/runs/41058685269/artifacts/12978740288), имя `AllMusicPlayer-002A-Android-arm64-v8a-debug-558cfaf8a1de-41058685269-1`, истекает 13.11.2026.
- Локально: 8/8 Python-тестов, Python compile, документационная проверка и whitespace check — пройдены. Локальная Qt/Android сборка отсутствовала; сборку подтвердил CI.

Реализация — отдельный Android экран с синтетическим тоном, play/resume, pause, stop, restart и видимыми ошибками. Desktop проигрыватель не менялся. Доступ к облаку, сети, личным файлам и разрешениям Android отсутствует. Выбор production-аудиодвижка не сделан.

**Ручная приёмка владельца на Huawei — ПРИНЯТО (30.10.2026).** Владелец сообщил: «Всё работает, в фоне работает, после блокировки экрана работает, при возврате в приложение работает». Таким образом, локальный тестовый звук и управление работают по отчёту владельца, а сворачивание/блокировка/возврат прошли в его проверенных сценариях. [Протокол приёмки в PR #19](https://github.com/metravolokos/allmusic-player/pull/19#issuecomment-6524008425). Отдельное прослушивание через гарнитуру **не подтверждено**. Важно: тон длится только около 6 секунд; не проверены длительное фоновое воспроизведение, foreground service, media session, медиакнопки, взаимодействие с системным аудиофокусом и реальный WebDAV. [Инструкция](docs/ANDROID-TEST.md), [отчёт](docs/002-A-ANDROID-AUDIO.md).


## 002-B — безопасная диагностика Cloud Mail WebDAV (Issue #20)

Реализуется отдельный Windows Desktop прототип: локальная форма учётных данных,
один `PROPFIND Depth: 1` для `/AllMusic/` и отдельно подтверждаемая Range-проверка
одного аудиофайла до 64 КиБ. Старый loopback `WebDavProbe`, Android и
`LocalPlayback` не превращаются в универсальный облачный клиент. Автоматические
сетевые испытания используют только loopback mock; к аккаунту Mail приложение
Codex и CI не обращаются.

Учётные данные не сохраняются в файлы или настройки и удаляются из временной
сессии при отключении. В клиенте закреплён HTTPS host `webdav.cloud.mail.ru:443`,
путь ограничен `/AllMusic`, перенаправления и ошибки TLS не обходятся.
Инструкция владельцу: [docs/002-B-CLOUD-WEBDAV.md](docs/002-B-CLOUD-WEBDAV.md).
Статус ручной проверки настоящего Cloud Mail — **НЕ ПРОВЕРЕНО**, её выполняет
владелец на своём Windows компьютере после получения ZIP.
