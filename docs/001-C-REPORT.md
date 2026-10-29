# 001-C — отчёт раннего Android эксперимента

## Для владельца

Подготовлена отдельная Android-сборка общего Qt-проекта с мобильным экраном.
Экспериментальный arm64-v8a APK не подтверждает совместимость Huawei до установки.
Инструкция: [ANDROID-TEST.md](ANDROID-TEST.md).

## Реализация

Общий C++ запуск, отдельный QML в ui/android. Android-конфигурация не создаёт
WebDAV-инструменты и Desktop CTest. Существующие Desktop workflow/сценарии сохранены.
SDK 36, min API 28, target 36, build-tools 36.0.0, NDK 27.2.12479018, JDK 21;
Qt 6.12.0, CMake 3.30.5, Ninja 1.12.1. Версии не подменяются при ошибке установки.

Workflow Android 001-C запускается на PR в main только при затрагивающих APK
изменениях и вручную. Нет push-триггера; предыдущая сборка PR отменяется, timeout
45 минут, артефакты 14 дней. В PR проверяется merge commit, SHA указан в пакете.
Debug-ключ создаётся средствами сборки на runner, не передаётся в Git, артефакт,
логи или cache. Отпечаток публичного сертификата сохраняется. Обновления между
разными runner могут требовать удаления предыдущей тестовой копии.

Проверяется готовый APK: подпись, ZIP/ELF 16 KiB alignment, только arm64-v8a,
bundled Qt, zero permissions, единственная QtActivity без служб/receiver/provider,
нет GMS/Firebase в manifest/Gradle runtime/DEX. AndroidX не является GMS.
Проверка известных зависимостей не является доказательством отсутствия любого
возможного сетевого поведения; INTERNET отсутствует в итоговом manifest.

## Реальные проверки

Локально в облачной рабочей среде пройдены:

- `python3 scripts/check_repository.py` и `git diff --check`;
- `python3 -m py_compile scripts/package-android.py scripts/collect-licenses.py`;
- `python3 -m unittest discover -s tests/android -v`: 4 теста с отрицательными
  сценариями permissions/components/API/ABI/identity/GMS/Firebase/unresolved dependencies;
- actionlint 1.7.7 для Android и существующего Desktop workflow
  (`-shellcheck= -pyflakes=` — эти дополнительные анализаторы не запускались);
- разбор CMakePresets.json и XML-шаблона manifest.

Windows/Linux CI [40898773590](https://github.com/metravolokos/allmusic-player/actions/runs/40898773590)
успешен: на обеих платформах `webdav-safety` и `window-smoke` (2/2 CTest);
Linux QtTest 86 passed, 0 failed. Упаковка Windows приложения и WebDAV kit также
пройдена. Это CI Windows Server 2022, не повторный тест на устройстве владельца.
Android CI [40899932754](https://github.com/metravolokos/allmusic-player/actions/runs/40899932754)
**ПРОЙДЕН** для head `09cca4ef2bfb4d76602f760ee5e51b3dd3090159`, merge SHA
`b2f012e16ac6` с main `6003527`. `cmake --preset android-debug` и
`cmake --build --preset android-debug --parallel 2`: сборка и Gradle упаковка успешны.
`python scripts/package-android.py`: подпись (`apksigner verify`), ZIP alignment
(`zipalign -c -P 16`), ELF LOAD >=16 KiB, ABI, manifest и runtime/DEX аудит пройдены.
4 теста аудитора прошли и в Android CI.

[Скачать проверенный APK kit](https://github.com/metravolokos/allmusic-player/actions/runs/40899932754/artifacts/12908837814)
(войти в GitHub): `AllMusicPlayer-001C-Android-arm64-v8a-debug-b2f012e16ac6-40899932754-1`,
ZIP около 17 МБ, доступен до 12.11.2026. Внутри один APK, SHA256SUMS.txt,
BUILD-INFO.json, инструкция, лицензии и аудит. Метка экрана:
`001-C-b2f012e16ac6-40899932754-1`.

Итоговый manifest: min API 28, target/compile 36, ноль разрешений,
только QtActivity, без service/receiver/provider. Runtime и DEX не содержат
GMS/Firebase ссылок. Native Qt: Core, Gui, Network, OpenGL, Qml, QmlMeta,
QmlModels, QmlWorkerScript, Quick плюс штатные плагины Base/Declarative и libc++.
Qt Network и debug/QML плагины могут присутствовать как зависимости Qt; это
не реализованная сеть приложения и не GMS. INTERNET в APK отсутствует, сетевые
отладочные аргументы не задаются. Полный inventory/публичный отпечаток подписи
приложены в audit. Дополнительные Controls/Dialogs/Test/Widgets/SVG не включены.

Последующие изменения отчёта/статуса не меняют проверенный код APK. Фильтр путей
не запускает Android заново только ради этих документов. APK не переименовывается
под новый SHA: его маркировка сохраняет точную проверенную сборку.
 Проверки на Huawei,
эмуляторе, установка APK, реальная отрисовка, повороты и жизненный цикл
**НЕ ПРОВЕРЕНЫ**. Ручные результаты 001-A Windows 10 (86 passed, 0 failed)
сообщены владельцем; они не являются результатом Android-тестов.

## Существенные детали сборки

Для Qt Android >=6.7 metadata находится в all_os. Прямой закреплённый aqtinstall
позволяет избежать старого linux/android preflight обёртки. Host-комплект требует
архив ICU; в Android APK ICU не добавляется. В target устанавливаются только
qtbase/qtdeclarative, чтобы не включать необязательные Qt SVG плагины.
Qt установлена в RUNNER_TEMP вне исходников: иначе qmlimportscanner может
просканировать установленную Qt и включить посторонние QML-модули в APK.

`android/build.gradle` основан на точном шаблоне Qt 6.12.0. Единственное
функциональное дополнение — исключение транзитивного androidx.profileinstaller,
подключаемого lifecycle-runtime: для экрана не нужны его startup provider и
broadcast receiver. Версии Qt/Gradle/AGP не менялись. Неиспользуемый FileProvider
и служебное разрешение AndroidX исключены manifest merger; нулевой итоговый
набор разрешений и дополнительных компонентов проверяется по APK.

Qt сообщает о недоступном необязательном Qt6QmlAssetDownloaderPrivate/Qt6TaskTree;
экран эту возможность не использует. Подключать дополнительные модули ради
устранения предупреждения не стали. На Android SDK/Qt binaries из текущей
облачной среды локальная cross-сборка не заявляется: используется реальный CI.

## Ограничения и приёмка

Нет звука, WebDAV, медиа, БД, кэша, службы, уведомлений или личных разрешений.
Не исследовались драйверы Huawei, фоновые ограничения EMUI, аудиофокус и GMS
на конкретном устройстве. При API/ABI за поддерживаемыми пределами — остановка,
без самостоятельной смены Qt/архитектуры. Лицензия собственного кода не назначена;
пакет предназначен для эксперимента владельца, публичный релиз не объявлен.

До положительного ручного результата приёмка Huawei не заявляется. PR объединяет
владелец. После отчёта остановиться; 001-B/001-D не начинать.


## Передача на приёмку

[PR #6](https://github.com/metravolokos/allmusic-player/pull/6) открыт, не объединён.
Рекомендация: перейти к ручной проверке Huawei по инструкции; не считать целевое
устройство принятым до отчёта владельца. После реализации остановиться и ждать
приёмки. Никаких автоматических переходов к 001-B/001-D.
