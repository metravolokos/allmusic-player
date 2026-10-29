# Зависимости и лицензии задачи 000

Выбрана открытая Qt с динамическим подключением библиотек. Лицензия собственного
кода AllMusic Player отдельно владельцем не выбрана; этот документ её не назначает.
Коммерческая лицензия Qt не приобретается и не предполагается.

| Компонент | Версия / область | Лицензия / источник |
|---|---|---|
| Qt Base: Core, Gui, косвенные Network/OpenGL и инструменты Qt | 6.12.0 | Для используемых библиотек LGPLv3 (либо альтернативные GPL/commercial); [исходники и лицензии](https://github.com/qt/qtbase/tree/v6.12.0/LICENSES) |
| Qt Declarative: Qml, QmlMeta, QmlModels, QmlWorkerScript, Quick, QtQuick.Window | 6.12.0 | LGPLv3 для используемых библиотек; [исходники и лицензии](https://github.com/qt/qtdeclarative/tree/v6.12.0/LICENSES) |
| Инструменты Qt: moc, rcc, qmlcachegen, windeployqt | 6.12.0 | Условия различаются по инструментам: GPL с исключениями либо LGPL/commercial; тексты в исходниках Qt |
| Встроенные сторонние компоненты Qt | По составу конкретного пакета Qt | Собственные лицензии, включая MIT/BSD/FreeType/zlib и др.; `qt_attribution.json` и тексты в `src/3rdparty` |
| CMake | 3.30.5, только сборка | [BSD-3-Clause и сторонние уведомления](https://github.com/Kitware/CMake/tree/v3.30.5/Licenses) |
| Ninja | 1.12.1, только сборка | [Apache-2.0](https://github.com/ninja-build/ninja/blob/v1.12.1/COPYING) |
| GCC / libstdc++ | Ubuntu GCC 13.x | [GPLv3; для runtime — GCC Runtime Library Exception](https://gcc.gnu.org/onlinedocs/libstdc++/manual/license.html) |
| MSVC 2022 / VC143 runtime / D3DCompiler 47 | toolset 14.44; runtime/D3D в Windows-архиве | [Условия Microsoft и список разрешённых к распространению файлов](https://learn.microsoft.com/en-us/visualstudio/releases/2022/redistribution) |
| Python | 3.12, только инструменты | [PSF License](https://docs.python.org/3.12/license.html) |
| install-qt-action / aqtinstall | Установщик CI, точный action закреплён SHA | [MIT](https://github.com/jurplel/install-qt-action/blob/v4/LICENSE), [MIT](https://github.com/miurahr/aqtinstall/blob/master/LICENSE) |

FFmpeg, TagLib, SQLite и Qt Multimedia в задаче 000 не подключаются.
Qt Quick Controls не требуется: экран использует Qt Quick и QtQuick.Window.
В Windows-пакете исключены необязательные плагины imageformats/iconengines,
чтобы текстовый экран не подтягивал Qt SVG. PNG для снимка поддерживает Qt Gui.
Не следует считать все модули Qt доступными под LGPL: каждый новый модуль
проверяется отдельно, GPL-only модули автоматически не добавляются.

## Уведомления и замена библиотек

Windows-архив содержит динамические DLL Qt, каталог `licenses` с текстами Qt
Base/Declarative и сторонними уведомлениями из соответствующих исходников.
Пользователь может заменить совместимые DLL Qt собственной сборкой; запрет
модификации Qt или обратной разработки для отладки таких изменений не вводится.
Использование LGPL требует сохранения уведомлений и соблюдения условий LGPL/GPL,
а не только наличия DLL рядом с EXE.

Исходники соответствующей версии:

- [Qt Base 6.12.0](https://github.com/qt/qtbase/tree/v6.12.0), commit `025bdad181de241e81bf853c8a2d7bf3d19261a9`.
- [Qt Declarative 6.12.0](https://github.com/qt/qtdeclarative/tree/v6.12.0), commit `4332fbbb4c340463dc8cee36dda0c85d932122d8`.
- [Полный официальный архив Qt 6.12.0](https://download.qt.io/official_releases/qt/6.12/6.12.0/single/).
- [Условия Qt](https://doc.qt.io/qt-6.12/licensing.html).

Перед публичным распространением бинарных пакетов необходимо сверить фактические
DLL/плагины и их сторонние компоненты, доступность соответствующих исходников,
условия предоставления этих исходников и runtime Microsoft. Сборочный архив CI
предназначен для проверки владельцем, не объявляется юридически проверенным релизом.
`SHA256SUMS.txt` фиксирует фактический состав Windows-папки до упаковки.

## Проверенные технические источники

Установщик aqt 3.3.0 не учитывает разделение Windows-репозитория Qt >=6.11
по компиляторам. В Windows CI используется официальный исправленный исходный
код aqt, commit `076e1659807d0b362a3ed684d54c2e9c775eb9c7` (MIT),
а не локальная правка установщика. Это инструмент сборки, в приложение не входит.
Риск: исправление пока взято из исходников, не из опубликованной версии PyPI;
после выхода подходящего релиза заменить его отдельным проверяемым обновлением.

- [Поддерживаемые платформы Qt 6.12](https://github.com/qt/qtdoc/blob/v6.12.0/doc/src/platforms/supported-platforms.qdocinc): Ubuntu 24.04/GCC 13.x, Windows 10 1809+/11, MSVC 2022; Qt 6.12 — последняя ветка с Windows 10.
- [Параметры Android Qt 6.12](https://github.com/qt/qtbase/blob/v6.12.0/doc/global/macros.qdocconf): JDK 21, NDK r27c (`27.2.12479018`, Clang 17.0.2).
- [Требования CMake](https://github.com/qt/qtbase/blob/v6.12.0/.cmake.conf): минимум 3.25; выбранная 3.30.5 удовлетворяет этому условию.

Наличие тега проверено через `git ls-remote`, исходники и лицензии прочитаны.
Сервер `download.qt.io` из исходной облачной среды отвечает 403 на CONNECT;
доступность готового открытого Linux-пакета подтверждена успешным GitHub Actions;
Windows-пакеты и итоговые результаты фиксируются отдельно в [статусе проекта](../STATUS.md).
