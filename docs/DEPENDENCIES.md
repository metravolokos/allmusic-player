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

## Android 001-C: runtime и инструменты

Qt 6.12.0 Base/Declarative динамически упаковываются в APK как `.so`; отдельная
установка Qt не нужна. CMake 3.30.5/Ninja 1.12.1 сохранены. Инструменты: JDK 21,
Android SDK platform 36, build-tools 36.0.0, NDK r27c/27.2.12479018 (Clang 17.0.2).
JDK/SDK/Gradle/AGP — средства сборки, не GMS runtime телефона. Шаблон Qt 6.12.0
использует Gradle 9.5.1 / AGP 9.2.1 и AndroidX Core 1.17.0; его транзитивные
зависимости фиксируются отчётом `audit/gradle-runtime.txt`. AndroidX и Kotlin
не являются Google Play Services. Runtime GMS/Firebase запрещён и проверяется
в зависимостях, DEX и manifest. Свои Java/Kotlin-исходники не добавляются.

- [Qt Android prerequisites](https://doc.qt.io/qt-6.12/android-configure-dev-environment.html).
- [Шаблон Gradle Qt 6.12.0](https://github.com/qt/qtbase/blob/v6.12.0/src/android/templates/build.gradle).
- [AndroidX Core: лицензия Apache-2.0](https://android.googlesource.com/platform/frameworks/support/+/androidx-main/LICENSE.txt).
- [Kotlin: Apache-2.0](https://github.com/JetBrains/kotlin/blob/master/license/LICENSE.txt).
- [Android NDK libc++: Apache-2.0 с LLVM exceptions](https://github.com/llvm/llvm-project/blob/llvmorg-17.0.2/libcxx/LICENSE.TXT).

В пакет включены уведомления Qt Base/Declarative, найденные уведомления из
Java runtime архивов и NDK NOTICE/NOTICE.toolchain (они охватывают больше
компонентов, чем фактически включённый libc++). Native inventory находится
в `audit/native-libraries.txt`, сведения о сборке — BUILD-INFO.json.
Новый модуль Qt за пределами Base/Declarative требует проверки и останавливает
аудит; Multimedia/FFmpeg/TagLib/SQLite не добавлялись.

LGPL-обязательства распространяются и на тестовые бинарные пакеты; персональное
назначение не является освобождением. Сохранены upstream notices и ссылки на
соответствующие исходники выше. Получатель имеет право изменять Qt и выполнять
обратную разработку для отладки этих изменений; запреты не вводятся.
Для замены Qt в APK недостаточно заменить DLL: собрать совместимую динамическую
Qt 6.12.0 Android arm64, пересобрать проект командами из BUILD.md (в репозитории
получателя), перепаковать стандартной целью apk и подписать собственным debug-
ключом. Удалить прежнюю тестовую копию перед установкой с другой подписью;
исходный CI-ключ не нужен, проверок привязки к сертификату нет. Запросить помощь
по этим действиям можно у владельца репозитория. Исходники библиотек должны
оставаться доступными получателю; при распространении за пределы эксперимента
повторно проверить способ предоставления complete corresponding source,
включённые сторонние notices и условия Android runtime. Пакет не объявляется
юридически проверенным публичным релизом; собственная лицензия проекта не выбрана.


## Desktop 001-B1 — исследовательский звук

Подключены динамические Qt Multimedia и Quick Controls / Dialogs / Layouts
из Qt Declarative 6.12.0 (используемые библиотеки — LGPLv3, альтернативные
GPL/commercial). Android остаётся в прежней области Base/Declarative.
В Desktop CI дополнительно устанавливается Qt Shader Tools как сборочная
зависимость; GPL-инструменты с Qt exception не становятся лицензией приложения.
Новые модули и их third-party notices включаются через исходники qtmultimedia
v6.12.0 и qtdeclarative v6.12.0. См. [Qt Multimedia](https://github.com/qt/qtmultimedia/tree/v6.12.0)
и [FFmpeg deployment](https://doc.qt.io/qt-6.12/qtmultimedia-deploying.html).

В Windows распространяются только FFmpeg DLL из официального бинарного Qt,
не системная GPL-сборка FFmpeg генератора. inspect-ffmpeg.py запрашивает версию,
лицензию и configuration непосредственно avutil DLL, отклоняет GPL/nonfree
и нерелизную версию. FFMPEG-INFO.json и SHA256SUMS фиксируют конкретный комплект.
При неизвестной сборке упаковка останавливается для уточнения исходников.
FFmpeg LGPL (2.1-or-later либо 3.0 в зависимости от actual configuration);
тексты COPYING и upstream source соответствующего релиза включены в архив.
Динамические библиотеки можно заменить совместимой собственной сборкой;
запрет обратной разработки для отладки таких изменений не вводится.
Upstream [FFmpeg](https://github.com/FFmpeg/FFmpeg), Qt build/dependency сведения
в qtmultimedia; Qt 6.12.0 attribution и сценарий provisioning указывают FFmpeg 9.0.1;
проверено по исходникам. Сценарий Windows загружает официальный tarball без
применения патчей. Упаковка сверяет runtime version с attribution и включает
upstream source + coin/provisioning Qt (commit ef25002ed292b132d1ed3246bfcee1d24a9cd966).
Фактическая DLL/configuration подтверждаются только соответствующим CI.
Перед публичным распространением требуется полный аудит corresponding source
и фактического состава. Этот отчёт не выбирает лицензию собственного кода.

Системный FFmpeg/libmp3lame на Ubuntu — только генерация/ffprobe в CI;
его исполняемые файлы и GPL runtime в Windows-архив не копируются.
Синтетические тоны/manifest — CC0-1.0, создаются скриптом, личной музыки нет.
Новые декодеры увеличивают поверхность обработки недоверенных медиа;
001-B1 ограничен локальными тестовыми файлами, без сетевых протоколов.


## Android 002-A — локальный тест звука

## Android 002-A — локальный WAV через системный media backend

В прежнем локальном Android-прототипе 002-A подключается Qt Multimedia 6.12.0; дополнительно
устанавливается Qt Shader Tools как зависимость сборки этого модуля. Android
плагин и библиотека Qt Multimedia упаковываются в тестовый APK. В сборке ожидается
плагин `androidmediaplugin`, который использует платформенный аудиовывод Android.
Qt FFmpeg multimedia plugin исключён из APK вместе с его бинарными зависимостями:
локальный сигнал декодирует Android backend. Это тестовая реализация, не
производственный выбор аудиодвижка.

Проверка лицензий использует исходники Qt Multimedia commit
`6ca885f084a3984749d53bb50a0e08bdc60e86cc` (ветка Qt 6.12.0); CI собирает из него
лицензии и `qt_attribution.json` в комплект APK. Новые разрешения Qt Android
плагина для камеры, записи аудио, Bluetooth и изменения настроек звука явно
удаляются из manifest: прототип только воспроизводит локальный тестовый сигнал.
Аудитор APK проверяет нулевой список разрешений, наличие Qt Multimedia и
Android media plugin, arm64-v8a и отсутствие GMS/Firebase. Фактический состав
подтверждается только успешным Android CI-аудитом.

Тон 440 Гц — синтетический PCM WAV, воспроизводимо создаваемый Python-скриптом
при конфигурации CMake и добавляемый в Android resource. Исходный WAV не хранится
в Git. Qt Multimedia получает локальную копию из ресурса во внутреннем кэше
приложения; внешнее хранилище и Android media permissions не нужны.


## Android 002-D — изолированный Qt FFmpeg feasibility spike

Issue #24 ограничен локальным аудио-испытанием и не добавляет Cloud Mail или
полноценный Android UI плеера. Отдельный Android package `spike002d` включает
`QFFmpegMediaPlugin` и его динамические FFmpeg библиотеки, а
`QAndroidMediaPlugin` исключён. Приложение задаёт `QT_MEDIA_BACKEND=ffmpeg` и
ограничивает FFmpeg protocol whitelist значением `file`;
на экране проверяется runtime-сообщение Qt с категорией `qt.multimedia.ffmpeg`
и версией FFmpeg. Поэтому успешное чтение MP3/FLAC из `QBuffer` при отсутствии
system plugin нельзя объяснить Android MediaCodec backend.

Qt Multimedia 6.12.0 указывает поставляемый FFmpeg **9.0.1** под LGPL-2.1-or-later
и перечисленные BSD/ISC/MIT/MPL notices. Запись Qt attribution сообщает, что
официальные prebuilt binaries не включают optional GPL/LGPLv3 компоненты;
проверяются `src/3rdparty/ffmpeg/qt_attribution.json` и относящиеся к нему
лицензионные тексты из закреплённого qtmultimedia commit. Это аудит выбранной
сборки Qt и её атрибуции, а не юридическое заключение для публичного релиза.

Qt FFmpeg plugin metadata объявляет CAMERA, RECORD_AUDIO, BLUETOOTH и
MODIFY_AUDIO_SETTINGS. Для локального вывода manifest удаляет эти permissions;
APK audit проверяет итоговый merged manifest и останавливается при любом
оставшемся разрешении. Проверка также требует FFmpeg plugin и avcodec, avformat,
avutil, swresample, swscale библиотек, исключает Android system plugin, проверяет
arm64-v8a и записывает фактический APK size.

MP3 и FLAC — короткие синтетические CC0 тоны, создаваемые системным FFmpeg только
во время сборки и добавляемые в APK как Qt resources. В приложении они открываются
через seekable `QBuffer`/`QIODevice`; исходные аудио-файлы не хранятся в Git.
FFmpeg/ffprobe на runner нужны только для генерации и проверки тестовых файлов,
их исполняемые файлы в APK не копируются. Результат этой задачи относится к
feasibility на arm64 APK; проверка слышимого звука и управления остаётся отдельным
ручным испытанием на Huawei. См. [проверочную инструкцию](ANDROID-FFMPEG-SPIKE-TEST.md).
