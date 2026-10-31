# SEL-001 — выборочное сравнение трёх реализаций AllMusic

Сверено 31.10.2026: `metravolokos/allmusic-player`, `main` `9c5d144c58319fece454da372744583ac10e9523`; `allmusic-deepseek`, `main` `64a440ccaced37cf5272e46f601fc0531ac964da`. Локальный Creator `D:\Projects\AllMusic-Mail-Local` из облачной среды недоступен, его результаты учтены по сообщению владельца. Код иных проектов не копировали.

## Заимствовано как идея, реализовано независимо

- **Реальный вызов QML → C++.** Дополнен Desktop CTest `android-media3-state`: `QQmlEngine` исполняет QML `QtObject`, который на настоящем `AndroidCloudTrackListModel` меняет `searchText` и `sortMode`, вызывает `containsSourceIndex()`, наблюдает Q_PROPERTY/NOTIFY и обновление списка. Только фиктивные строки, без сети, звука и Android SDK.
- **Поверхность Android-контроллера.** Offline Python unittest сверяет реально вызываемые `media3.method()` из Android QML с `public slots`, а используемые поля с `Q_PROPERTY`; модель должна иметь `Q_INVOKABLE containsSourceIndex`. Этот уровень является статическим и дополняет, но не заменяет, исполняемый тест.

## Уже присутствовало (не переносили повторно)

- Статусы/буфер: `AndroidMedia3State.h`, `tests/android_media3/tst_android_media3_state.cpp`, `LocalPlayback::buffering`.
- Cancel и отсутствие устаревших данных: `mailCancelClosesDelayedRangePromptly`, `mailSeekCancelsStaleRangeAndReadsNewPosition`, `userStopCancelsActiveRangeAndCanRestart`, `trackSwitchCancelsOldLoadAndContinues` в `tests/streaming/tst_streaming.cpp`.
- Seek не подтверждается одним `position()`: streaming-тесты проверяют декодированные PCM после `setPosition` и фактический новый Range-запрос. Это всё ещё не утверждение о слышимой перемотке в настоящем Cloud Mail на любой ОС.
- Видимость окна: `window-smoke` требует реальный кадр `QQuickWindow::frameSwapped` и сохраняет PNG.
- Windows deployment: `scripts/package-windows.ps1` проверяет обязательные Qt/FFmpeg DLL и запускает готовый EXE/PCM/streaming тесты из развернутого каталога с очищенными Qt и PATH переменными. Не является исчерпывающим PE dependency graph-аудитом.

## Отклонено сейчас

- Перенос Windows Cloud Mail playback из Creator: Desktop уже использует `QMediaPlayer::setSourceDevice(device)` с `MailWebDavRangeDevice`, а Android — JNI handle Media3. URL с реквизитами не передаётся в QMediaPlayer Cloud-пути; изменять ядро незачем.
- Переход на QTP0001/QTP0002: текущие Qt 6.12 сборка и window smoke уже работают; политики не меняем без воспроизводимой ошибки в этом репозитории.
- Новая машина буферизации, WebDAV-клиент, SQLite, офлайн, CI/workflow, кеши: проблема для оправдания такого вмешательства не подтверждена.

## Ограничения и приёмка

Тест QML здесь — исполняемый контракт модели, не полноценный запуск Android-экрана или JNI. Платформенный QtTest проверять на Windows/Linux CI; Android Python contract — без SDK. Оставить CI в Draft fast, при готовности — однократно перевести PR в Ready для обязательного полного CI. Не передавать в GitHub личные файлы, URL/пароли Cloud Mail или реальные названия. Результат теста/сборки не означает ручную приёмку.
