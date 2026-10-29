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
выбирает Python >=3.14 (в первом успешном Linux CI это 3.15.1) и aqtinstall 3.3.0.
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

`--smoke-test screenshot.png` требует создания QML-окна, его видимости,
доступности экрану и отрисованного кадра; сохраняет изображение и завершает
приложение. Если кадра нет 15 секунд, возвращается ошибка. CTest ограничивает
весь запуск 30 секундами. В CI используется программная отрисовка.

Снимок `build/desktop-release/window-smoke.png` приложен к диагностике CI.
Он позволяет отдельно оценить текст и внешний вид. Тест не проверяет реальные
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
