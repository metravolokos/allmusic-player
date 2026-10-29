# Текущий статус проекта — AllMusic Player

**Дата:** 29.10.2026. **Этап:** 0. **Задача 000:** реализация подготовлена, ожидает ручной проверки и приёмки владельцем.

## Утверждено и подготовлено

- Документация v0.1 утверждена; [PR #1](https://github.com/metravolokos/allmusic-player/pull/1) объединён в `main` (подтверждено через GitHub).
- D-005 (облако только на чтение) и D-006 (Qt/C++/QML/CMake) утверждены 29.10.2026. Статусы остальных исходных решений сохранены. Новые прямые утверждения задачи 000 отражены в D-021–D-025.
- Изменения находятся в отдельной ветке `task/000-repository-setup`, [PR #2](https://github.com/metravolokos/allmusic-player/pull/2). Самостоятельного объединения нет.
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

## Ограничения, риски и решённые препятствия

- **НЕ ПРОВЕРЕНО на устройствах:** Windows 10, Windows 11, ALT Regular, CachyOS, Huawei nova 15 Pro / EMUI 15. Успех Windows Server CI не означает поддержку пользовательских Windows 10/11.
- **НЕ ПРОВЕРЕНО:** реальные GPU/масштабирование, Wayland/X11 пользователя, Android API Level/ABI, APK, отсутствие зависимости от GMS, фон и энергосбережение EMUI. Android SDK/NDK не устанавливались.
- WebDAV, звук, кодеки, seek/gapless, БД и кэши не реализованы и не испытывались. Личное облако не использовалось, аудиофайлы и секреты в изменения не добавлялись.
- Исходная среда Debian 13 блокирует `download.qt.io` (403 CONNECT). Установка открытых бинарных Qt 6.12.0 успешно проверена в обоих GitHub runner; версию Qt не меняли.
- Windows-репозиторий Qt >=6.11 разделён по компиляторам. aqt 3.3.0 ищет старый путь и получает 404. Использована закреплённая официальная ревизия aqt с поддержкой нового формата. Риск — версия установщика пока из исходников; перейти на подходящий опубликованный релиз отдельным проверенным обновлением. Разделение задачи на 000A/000B не потребовалось.
- CMake сообщает о недоступной необязательной связке `Qt6QmlAssetDownloaderPrivate` / `Qt6TaskTree` из установленного Qt. Приложение её не использует; конфигурация, сборка и запуск проходят. Дополнительный модуль не подключали.
- Windows EXE пока не подписан. Перед публичным распространением нужны полный контроль лицензий/соответствующих исходников, условия runtime Microsoft и выбор лицензии приложения. Архив CI — для испытаний владельцем, не объявляется готовым публичным релизом.
- Дополнительную сборку Qt из исходников в контейнере Ubuntu 24.04 начали из-за сетевого ограничения, затем остановили после успеха Linux CI. Полная сборка Qt из исходников не заявляется.

## Следующее действие

Владелец просматривает PR #2 и проверяет архив отдельно на Windows 10 и Windows 11
по инструкции, фиксируя версию ОС, ссылку на сборку, видимость текста, изменение
размера и закрытие окна. Рекомендация: принимать после этой ручной проверки.
PR не объединять автоматически. Задача 001 **не начата**; нужен отдельный запрос владельца.
