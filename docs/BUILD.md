# Сборка основания проекта (задача 000)

Приложение показывает только надпись «AllMusic Player — проект запущен».
Сетевых запросов, воспроизведения и базы данных нет.

## Зафиксированный набор

Qt **6.12.0**, C++17, CMake **3.30.5**, Ninja **1.12.1**.
Linux CI: Ubuntu **24.04 x64**, GCC **13.x** из репозитория Ubuntu.
Windows CI: **windows-2022** (Windows Server, не пользовательские Windows 10/11),
MSVC 2022 toolset **14.44**, x64. Полные версии пакетов/компилятора выводятся
в журнал каждого запуска: образы GitHub и обновления Ubuntu меняются.
Скрипты подготовки используют Python 3.12; установщик Qt v4.5.0 самостоятельно
выбирает Python >=3.14 (в первом успешном Linux CI это 3.15.1).
Linux использует aqtinstall 3.3.0; Windows — официальный исходный код aqt
с закреплённым commit `076e1659807d0b362a3ed684d54c2e9c775eb9c7`,
поддерживающий новый формат каталогов Windows Qt >=6.11.
Это фиксация версии Qt и основных инструментов, а не побитовая воспроизводимость.

Исходники Qt 6.12.0 и документация проверяются по тегам официальных репозиториев.
Ubuntu 24.04/GCC 13 и MSVC 2022 перечислены в поддерживаемых конфигурациях Qt.
CMake 3.30.5 выше указанного в исходниках Qt минимума 3.25.
Наличие исходников не доказывает доступность готового пакета в установщике.
Если установка Qt 6.12.0 не удалась, CI останавливается; подмена версии запрещена.

## Linux: повторение сборки

Нужна установленная открытая Qt 6.12.0 Desktop gcc_64. Укажите её путь через
`CMAKE_PREFIX_PATH` (путь зависит от места установки).

```sh
sudo apt-get update
sudo apt-get install -y g++-13 libgl1-mesa-dev libxkbcommon-x11-0 libxcb-cursor0 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-render-util0 xvfb xauth fonts-dejavu-core
python3 -m venv .venv
. .venv/bin/activate
python -m pip install cmake==3.30.5
git clone https://github.com/ninja-build/ninja.git build/ninja-source
git -C build/ninja-source checkout 2daa09ba270b0a43e1929d29b073348aa985dfaa
(cd build/ninja-source && python configure.py --bootstrap)
export PATH="$PWD/build/ninja-source:$PATH"
export CC=gcc-13 CXX=g++-13
export CMAKE_PREFIX_PATH=/path/to/Qt/6.12.0/gcc_64
cmake --preset desktop-release
cmake --build --preset desktop-release --parallel 2
QT_QPA_PLATFORM=xcb xvfb-run -a ctest --preset desktop-release
```

Ninja собирается из официальных исходников: Python-пакета `ninja==1.12.1`
в проверенном индексе PyPI нет. Версию самого инструмента это не меняет.

В обычном рабочем столе запустите `./build/desktop-release/allmusic-player`.
Для автоматической проверки без Xvfb используйте `ctest --preset desktop-release`
из сеанса с работающим графическим окружением.

## Что означает проверка окна

`--smoke-test screenshot.png [screen]` требует создания QML-окна, его видимости,
доступности экрану и отрисованного кадра; сохраняет изображение и завершает
приложение. Если кадра нет 15 секунд, возвращается ошибка. CTest ограничивает
весь запуск 30 секундами. В CI используется программная отрисовка. Необязательный
экран: `artists`, `artistTracks`, `artistAlbums`, `albums`, `albumTracks`,
`folders`, `now` или `settings`; без параметра открываются композиции.

Снимки `build/desktop-release/window-smoke*.png` сохраняют все состояния Desktop
UI в PR #51 для просмотра. Тест не проверяет реальные
драйверы, масштабирование экрана, Wayland, звук или пользовательские устройства.

## Windows

Пользователю компилятор и Qt не нужны: см. [инструкцию запуска архива](WINDOWS-TEST.md).
Для разработки нужен Visual Studio 2022 C++ x64 с toolset 14.44 и открытая
Qt 6.12.0 `msvc2022_64`. В терминале разработчика после установки CMake/Ninja:

```powershell
cmake --preset desktop-release
cmake --build --preset desktop-release --parallel 2
ctest --preset desktop-release
```

CI применяет `windeployqt`, добавляет app-local VC143 runtime, лицензии и
контрольные суммы. Запускает именно упакованную копию с очищенными путями Qt.
Это полезная проверка состава архива, но системные библиотеки runner могут
скрыть недостающую зависимость: нужна ручная проверка на Windows 10 и 11.

## Документы

```sh
python3 scripts/check_repository.py
git diff --check
```

Проверяются наличие основных документов, локальные Markdown-ссылки, кодировка
и отсутствие аудиофайлов в перечне файлов. Это не полный поиск секретов;
изменения дополнительно просматриваются перед коммитом.

Android SDK/NDK не устанавливаются в задаче 000. Ранняя проверка APK на Huawei
запланирована отдельно и требует разрешения владельца на следующую задачу.

## Android Media3 002-D-M3

Android использует AndroidX Media3 / ExoPlayer через Java/JNI адаптер. Для Cloud
Mail подключён существующий C++ WebDAV/Range код; Qt Multimedia и Qt FFmpeg в
Android target не подключаются. Основной путь — workflow `.github/workflows/android.yml`:
`pull_request`, `workflow_dispatch`, push в main и опубликованный release;
классификатор выбирает нужные проверки по diff. Для Ready PR, где запускается
Android binary job, APK комплект автоматически сохраняется после успешной
сборки и package/license audit; в PR #51 он доступен в Artifacts Android
workflow 14 дней. Для Huawei APK и полного Android CI вручную укажите
`source_ref=codex/002-d-android-cloud-media3`, `android_apk=true`,
`full_tests=true`. Ручной `workflow_dispatch` остаётся дополнительным способом
сохранить APK kit.

На подготовленной Linux-машине с Android SDK, JDK 21, NDK 27.2.12479018,
SDK platform 36 / build-tools 36.0.0, Qt 6.12.0 Android arm64 и host gcc_64:

```sh
export ANDROID_HOME=/path/to/Android/Sdk
export QT_ANDROID_ROOT=/path/to/Qt/6.12.0/android_arm64_v8a
export QT_HOST_PATH=/path/to/Qt/6.12.0/gcc_64
cmake --preset android-debug -DALLMUSIC_BUILD_LABEL=002-D-cloud
cmake --build --preset android-debug --parallel 2
```

Для Android-конфигурации нужны Qt Base/Declarative 6.12.0 и Qt Shader Tools;
Qt Multimedia для Android не требуется. Gradle использует закреплённый
`androidx.media3:media3-exoplayer:1.11.1`. Три короткие искусственные записи
MP3/FLAC/WAV хранятся в `android/assets/audio/`; при необходимости их можно
пересоздать командой `python3 scripts/generate_android_media3_fixtures.py` с
FFmpeg, который служит только локальным генератором. Для Huawei используйте
пошаговую инструкцию `docs/002-D-ANDROID-CLOUD.md`.

Для маркировки CI использует SHA проверяемого merge commit, run ID и attempt;
versionCode — номер запуска workflow. Пакет самодостаточный, debug, подпись
временная; keystore не сохраняется. Устройство не подключается к runner.
Скрипт `scripts/package-android.py` рассчитан на workflow с подготовленными
runtime-отчётом, лицензиями и переменными сборки. Он публикует только APK,
инструкции и аудит, а не build tree или Gradle cache. Успешный CI не подтверждает
слышимый звук на Huawei; до ручной проверки совместимость устройства остаётся
**НЕ ПРОВЕРЕНА**.


## Дополнение 001-B1: Desktop audio

Установите дополнительно Qt 6.12.0 Multimedia и Shader Tools; Quick Controls,
Dialogs и Layouts входят в Qt Declarative. Qt-версию не менять. Для генератора
нужны FFmpeg/ffprobe с libmp3lame/aac/flac/alac (на Ubuntu пакет `ffmpeg`).
На Linux для обычного звука нужен настроенный PulseAudio/PipeWire/ALSA.

```sh
python3 scripts/generate-audio-fixtures.py build/audio-fixtures
python3 scripts/generate-audio-fixtures.py build/audio-fixtures --verify-only
cmake --preset desktop-release
cmake --build --preset desktop-release --parallel 2
QT_MEDIA_BACKEND=ffmpeg QT_QPA_PLATFORM=xcb xvfb-run -a ctest --preset desktop-release --verbose
```

В Windows те же CMake/CTest команды после получения фикстур артефакта
synthetic-audio-001B1 в build/audio-fixtures (или локальной генерации FFmpeg).
Проверка PCM автоматическая; [проверка слышимого звука](AUDIO-TEST.md) отдельно.
Прежние разделы о text-only приложении описывают историческую задачу 000.

После изоляции app target в src/app промежуточная Android-папка находится в
`build/android-debug/src/app/android-build`. Команды preset/apk не изменились;
Desktop EXE остаётся в `build/desktop-release`.
