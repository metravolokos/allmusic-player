Warning: truncated output (original token count: 10520)
Total output lines: 372

# Текущий статус проекта — AllMusic Player

## Актуальная сводка — 01.11.2026

- **main**: база PR #51 — `5251e518b827d171978a38888c285d64a6b16776` (содержит PR #49). **PR #51** остаётся открытым в `issue-50-fusion-player` и не объединяется до проверки владельцем.
- В PR добавляются полноценные разделы «Композиции», «Исполнители», «Альбомы», «Альбомы исполнителя», «Папки», «Сейчас играет» и «Настройки» для Android и Desktop; Desktop сохраняет локальное открытие файлов. Все экраны используют Fusion, темы системная/светлая/тёмная.
- Добавлены реальный фоновый индекс `/AllMusic`, чтение ID3/Vorbis тегов безопасными Range-запросами до 64 КиБ, локальный кэш метаданных и общая временная очередь для всех треков/исполнителя/альбома/текущей папки. Repeat Off/All/One, Shuffle, Previous/Next, Stop и автопереход используют Android Media3 и Desktop QMediaPlayer. Папочная очередь включает только непосредственные файлы текущей папки.
- «Избранное» скрыто в QML, существующая Android-модель, данные и API оставлены; плейлисты не добавлены.
- Локально пройдены Android offline tests (32), CI tests (31), `py_compile`, `scripts/check_repository.py` и `git diff --check`. Qt/C++/QML локально не собираются: в окружении нет CMake/Ninja/Qt 6. Для кода на `272d98e4caad2f3ddccedbfe03fa1b9deb3abfc7` успешны Android build/package audit, Desktop Linux и Windows build/test/package jobs. APK kit: [Android run](https://github.com/metravolokos/allmusic-player/actions/runs/41330757941); Windows ZIP и Desktop screenshots: [Desktop run](https://github.com/metravolokos/allmusic-player/actions/runs/41330757926).
- Ручная проверка Android на Huawei и Windows 11 остаётся **НЕ ПРОВЕРЕНА** до проверки владельцем. Реальное Cloud Mail не подключалось; личные данные и пароли не использовались.
- **Merge не выполнялся и не выполнять до ручной проверки владельцем.** Исторические записи ниже сохранены как журнал.

## История этапов

## 002-E — Issue #32: Android-медиатека (31.10.2026)

### Ответ на UX-ревью PR #33

В [замечании к PR](https://github.com/metravolokos/allmusic-player/pull/33#issuecomment-6547825802)
обнаружено, что Play/Pause/Stop/Seek располагались в `ListView.footer`, а поиск —
в прокручиваемом `ListView.header`. Компоновка разделена: закреплённый верхний
`topPanel` содержит подключение, каталог, поиск, сортировку и счётчик; между
панелями прокручивается только `cloudList`; закреплённый снизу `playerPanel`
содержит выбор/Play, Play/Pause/Stop, позицию и seek. Форма реквизитов скрывается
после подключения; отключение и отмена подключения доступны сверху. Локальные
синтетические треки свёрнуты в панели плеера.
Контрактные Android tests теперь проверяют отсутствие `header`/`footer` у списка,
закрепление панелей, виртуализированный источник, размещение Play/Seek вне списка
и сворачивание формы подключения.

Быстрые локальные проверки после правки: Android Python tests (20), CI tests
(31), repository check (100 файлов), `py_compile` и `git diff --check` — пройдены.
Полные Android/Desktop сборки не запускались по плану ревью. Реальная проверка
удобства и QML-компиляция на Android остаются **НЕ ПРОВЕРЕНЫ**.

Уточнение владельца от 31.10.2026: Android-приложение поддерживает только
вертикальную ориентацию. Для QtActivity в `android/AndroidManifest.xml`
установлен `screenOrientation="portrait"`, а Android contract test проверяет
это значение из XML. Отдельная landscape-компоновка не требуется. Проверка
поиска с открытой экранной клавиатурой на физическом устройстве остаётся
**НЕ ПРОВЕРЕНА** до сборки целевого APK и теста Huawei.

Новая ветка `issue-32-android-cloud-library` и [Draft PR #33](https://github.com/metravolokos/allmusic-player/pull/33)
созданы от `main` SHA `6f91d6a2ed202e9b7c9ab28050536d00cfdf8cdd`. Android-каталог переведён на
виртуализированный `ListView`. Локальная `QSortFilterProxyModel` фильтрует
Unicode-названия без учёта регистра, применяет русскую числовую сортировку
А→Я / Я→А и сохраняет исходный `sourceIndex` при фильтрации и перестановке.
Скрытый фильтром выбор очищается; новый снимок списка при смене каталога или
сессии сбрасывает выбор. Добавлены счётчик результатов, сообщение «Совпадений
нет», очистка поиска и `adjustResize` для экранной клавиатуры. Сетевые классы,
Range-клиент, Media3 и безопасность WebDAV не менялись.

Добавлены QtTest сценарии для кириллицы, Unicode-нормализации, дублирующихся
отображаемых имён, двух направлений сортировки, соответствия source row,
сброса снимка и 5 000 синтетических элементов. Python-проверки Android проверяют
контракт виртуализированного списка, source index и локальное управление
фильтром. Выполнены Android Python tests (18), CI tests (31), проверка документов
(100 файлов) и `git diff --check`. C++/QML сборка, QtTest с 5 000 строками и
числовой подсчёт созданных QML-делегатов пока **НЕ ПРОВЕРЕНЫ**: в рабочей среде
нет CMake, Qt 6 и Android SDK/NDK; найденный `qmltestrunner` ожидает отсутствующий
Qt 5.

Draft fast CI на кодовом SHA `d69ecf9808bd2a0d8abf34609b238633312cc4d3`:
[Desktop run #41231949506](https://github.com/metravolokos/allmusic-player/actions/runs/41231949506)
и [Android run #41231949533](https://github.com/metravolokos/allmusic-player/actions/runs/41231949533).
Классификаторы, документация, CI-тесты и быстрые Android offline tests прошли;
platform jobs были пропущены. Обязательные `Desktop CI result` и
`Android CI result` завершились ожидаемой ошибкой merge gate для режима
`draft-fast`. Полная платформенная проверка не запускалась.

APK пока не сформирован: workflow не сохраняет APK в обычном Draft CI; нужен
ручной `workflow_dispatch` Android workflow с `source_ref=issue-32-android-cloud-library`
и `android_apk=true`. Доступная GitHub-интеграция не предоставляет запуска
`workflow_dispatch`. Реальный Cloud Mail не подключался; ручная проверка на
Huawei не выполнена.

## CI-OPT-002 — Issue #30 (31.10.2026)

Рабочая ветка `issue-30-ci-opt-002` создана отдельно от PR #29 и основана на
актуальной `main` `c511cf252f5c8dd4d5a481b3b91e5e18379accc5`. Реализованы
Draft fast mode, fail-closed merge gate, полная классификация cumulative diff
для Ready PR, общие Desktop/Android зависимости Cloud WebDAV и отчёты workflow.
PR #31 открыт в Ready для полного CI по рекомендации ревью; он не объединён.
В Draft на commit 01336ef8b6937b211267afe9ace698003452c2c4 быстрые jobs прошли,
платформы были пропущены, а оба обязательных статуса завершились ожидаемой
ошибкой ([Desktop](https://github.com/metravolokos/allmusic-player/actions/runs/41227798342),
[Android](https://github.com/metravolokos/allmusic-player/actions/runs/41227798470)).
Локально прошли 31 тест CI, 16 Android offline tests и
`scripts/check_repository.py`. Политика и измерения описаны в docs/CI.md;
настройки защиты `main` не менялись.

**Дата:** 31.10.2026. **Текущий статус:** 002-A принята владельцем в [PR #19](https://github.com/metravolokos/allmusic-player/pull/19); 002-B и Desktop 002-C приняты в `main`. Android Media3 по Issue #26 прошёл Android/Desktop CI и ручную проверку на Huawei. Android Cloud Mail по Issue #24 принят владельцем и объединён в `main` через [PR #29](https://github.com/metravolokos/allmusic-player/pull/29), commit `c511cf252f5c8dd4d5a481b3b91e5e18379accc5`; CI и согласованные Huawei сценарии пройдены. Навигация по вложенным папкам для медиатеки владельца неприменима; неудобство длинного списка отложено в отдельный UX backlog. CI использует только mock; пароль и медиатека владельца не используются.

## Утверждено и подготовлено

- Документация v0.1 утверждена; [PR #1](https://github.com/metravolokos/allmusic-player/pull/1) объединён в `main` (подтверждено через GitHub).
- D-005 (облако только на чтение) и D-006 (Qt/C++/QML/CMake) утверждены 29.10.2026. Статусы остальных исходных решений сохранены. Новые прямые утверждения задачи 000 отражены в D-021–D-025.
- [PR #2](https://github.com/metravolokos/allmusic-player/pull/2) объединён в `main`, merge commit `1746065852904d6e19e709e804ce73eec5c6a764`. Завершение задачи 000 подтверждено владельцем 29.10.2026.
- Созданы CMake-проект и preset, `src/app/main.cpp`, `ui/desktop/Main.qml`: только экран «AllMusic Player — проект запущен».
- Добавлены `.gitignore`, проверки документов и GitHub Actions для Ubuntu 24.04 и Windows x64.
- Добавлен…4520 tokens truncated…ных Linux/Windows C++ тестов эту версию не принимать
  и не передавать как проверенную сборку для реального Cloud Mail.

## 002-B — третье ручное испытание и узкая совместимость (30.10.2026)

- Windows-сборка CI #97 (SHA `deadd006`): успешны авторизация, просмотр списка
  элементов и получение HTTP 206 с 65536/65536 байт.
- Обезличенная классификация `Content-Range` выявила `bytes` + `=` + три
  десятичных числа вместо предусмотренного RFC 9110 пробела. Имена файлов,
  пароль и raw headers не передавались.
- Добавлена узкая поддержка `bytes=first-last/total` исключительно в
  отдельном клиенте диагностики Cloud Mail. Проверки диапазона, размера, ETag,
  framing, TLS, host/path pinning, no redirects и лимит 64 КиБ сохранены.
  Дополнены положительные и отрицательные loopback mock-тесты.
- **CI и реальный Range после изменения ещё не проверены**. PR #21 не объединять
  до зелёных Linux/Windows C++ тестов и успешного ручного испытания владельцем.

## 002-B — итог по актуальному `main`

Более позднее состояние подтверждено основанием Issue #22: владелец проверил
узкую форму `Content-Range` на Windows 11, PR #21 принят и объединён в `main`
с коммитом `f236e7591e969709e4c0cd40fb5196e97c57f27b`. Поэтому приведённые выше
промежуточные отметки «не принято / CI не проверен» не описывают текущий `main`.
Issue #22 фиксирует, что ручная проверка получила HTTP 206 и 65 536 байт; личные
имена файлов, реквизиты и значения заголовков в этом статусе не записываются.

## 002-C — потоковое воспроизведение Cloud Mail (Issue #22)

В ветке `codex/002-c-cloud-playback` добавлен read-only `QIODevice` для ограниченных Range-запросов
через закреплённый клиент Mail WebDAV и отдельный worker-поток. Буфер ограничен
четырьмя страницами по 64 КиБ; Desktop UI даёт явное воспроизведение MP3/FLAC,
навигацию по непосредственным подпапкам `/AllMusic`, seek и индикацию буферизации.
Остановка, Disconnect и закрытие очищают временную сессию и прерывают запрос.

Mock-покрытие добавлено для авторизации challenge, стандартного и узкого Mail
`Content-Range`, seek/отмены, слабого/отсутствующего ETag, одного повтора 429,
отказа при 200, перезапуска и MP3/FLAC PCM. C++/Qt тесты прошли в Desktop CI
на Linux и Windows; Android CI, оба Desktop warm-cache job и итоговые обязательные
статусы тоже прошли. Проверена ветка на кодовом коммите
`18ac040cb0e53dbb221e7d1d2b7c98e6951cf149` (Desktop run #103,
Android run #69). Локально C++ сборка недоступна: в среде нет CMake, Ninja и Qt SDK.
Windows ZIP `AllMusicPlayer-Windows-x64-41131451763` собран и доступен как artifact
Desktop workflow до 13.11.2026. Реальный Cloud Mail не используется Codex и CI.

Сильный ETag отправляется в `If-Range`. При слабом или отсутствующем ETag каждый
ответ по-прежнему проверяет HTTP 206, точный диапазон, общий размер, framing,
encoding и фактическую длину, но гарантии неизменной версии между запросами нет.
Пользовательская приёмка на Windows 11, включая звук MP3/FLAC, Pause/Seek/Stop,
повторный Play и Disconnect во время загрузки, остаётся обязательной.
Подробности: [docs/002-C-CLOUD-PLAYBACK.md](docs/002-C-CLOUD-PLAYBACK.md).

## 002-D-M3 — Android Media3 (Issue #26, 31.10.2026)

Рабочая ветка `codex/002-d-media3-spike` основана на актуальном `main`
`aa7e18b05c034eb42b53b51ebbc5ecfe76a1f53d`. Android экран Qt Quick управляет
Media3 ExoPlayer 1.11.1 через Java/JNI адаптер; вывод — Android AudioTrack и
системные MediaCodec декодеры. В APK подготовлены только синтетические MP3/FLAC,
WAV для регрессии и преднамеренно отсутствующий файл для ошибки. Android-путь
больше не подключает Qt Multimedia/FFmpeg. Desktop playback и Windows/Linux
источники не менялись.

Локально пройдены Python проверки фикстур и package audit, тесты классификатора
CI, проверка документов, `py_compile`, `ffprobe` для формата/длительности трёх
образцов и `git diff --check`. В среде отсутствуют CMake, Ninja, Android SDK/NDK,
Gradle и ADB; Android сборка, Desktop C++ тесты, аудит фактического APK и его
размер будут указаны после GitHub Actions. Huawei пока **НЕ ПРОВЕРЕН**.

Обязательная ручная проверка владельца: MP3 и FLAC по отдельности — слышимый звук,
Pause/Resume, seek, Stop/Play и переключение трека; WAV — регрессионный сценарий,
пропавший файл — видимая ошибка; сворачивание — остановка. Инструкция:
[docs/ANDROID-MEDIA3-SPIKE-TEST.md](docs/ANDROID-MEDIA3-SPIKE-TEST.md).
Кодовый SHA `83df8f95e0531d54ba446d0d3a9c80fa21a44544` прошёл [Android CI #98](https://github.com/metravolokos/allmusic-player/actions/runs/41179403741)
и [Desktop CI #132](https://github.com/metravolokos/allmusic-player/actions/runs/41179403707)
успешно; Linux и Windows CTest/build jobs завершились без ошибок.
APK audit проверил debug signature, 16 KiB alignment, только arm64-v8a,
отсутствие разрешений и Qt Multimedia/FFmpeg runtime, Media3 classes и все
лицензии runtime dependencies. APK имеет размер 50 799 168 bytes; ZIP-комплект
с APK, BUILD-INFO.json, SHA256SUMS, audit evidence, инструкцией и лицензиями —
19 789 522 bytes. [Скачать Actions artifact](https://github.com/metravolokos/allmusic-player/actions/runs/41179403741/artifacts/13034078403),
он хранится до 14.11.2026.

Draft [PR #27](https://github.com/metravolokos/allmusic-player/pull/27) остаётся
открытым. Ручное испытание слышимого звука и контролов на Huawei ещё
**НЕ ПРОВЕРЕНО**. Не объединять PR до его успешного завершения и подтверждения
владельцем.

## 002-D Cloud Mail — Issue #24 (31.10.2026)

Реализация начата в `codex/002-d-android-cloud-media3` от обязательного SHA
`a24a1834845b592ec27d98f1706a53d5727aecb3`. Android target подключает общий C++
Mail WebDAV Range клиент и использует Media3 `ProgressiveMediaSource` с
непрозрачным JNI handle. Java получает только handle и диапазонные байты; QML
получает отображаемые подписи, но не URL или внутренние пути WebDAV. Заголовки и
реквизиты не передаются Java/QML. Android APK получает только `INTERNET`; Desktop
playback оставлен прежним.

Добавлены мобильный вход и список MP3/FLAC внутри `/AllMusic`, явный запуск
выбранного файла, custom `DataSource` для Media3 и отмена запроса при Stop/close /
Disconnect. Текущие сетевые ограничения остаются у C++ Range-клиента: GET не
больше 64 КиБ, кэш до 256 КиБ, обязательный HTTP 206, строгий TLS, без редиректов
и без полной загрузки при HTTP 200. Для Disconnect добавлены асинхронное
отзывание opaque handle и отложенное уничтожение Range-устройства после
завершения активного read.

Mock-тест отмены и повторного чтения добавлен; Android package audit обновлён под
единственное INTERNET-разрешение и отдельный package ID. Локально пройдены
Android Python tests (13), CI classifier tests (19), repository documentation
check (94 файла), `py_compile` и `git diff --check`. C++/Qt тесты, Android APK,
Desktop/Linux/Windows CI ещё не запускались: в текущей среде нет CMake, Ninja,
Android SDK/NDK и Gradle; они запускаются GitHub Actions после создания draft PR.
Из ручной проверки владельца в PR #29 поступил сигнал: приложение сообщает
«Сетевая ошибка при проверке Cloud Mail» после ввода реквизитов. Причина по этой
строке пока неизвестна. Аудит предыдущего APK подтвердил, что qopensslbackend
включён, а `libssl`/`libcrypto` отсутствуют. В продолжении PR #29 добавляются
no-auth TLS preflight и фиксированные сетевые категории; APK будет собираться с
OpenSSL 3.5.9 из SHA-256-закреплённого официального исходного архива.
Локально пройдены Android Python tests (14), CI classifier tests (19),
repository documentation check (95 файлов), `py_compile` и `git diff --check`.
GitHub Android run #117 успешно собрал APK и прошёл package/license audit: в APK
есть `libcrypto_3.so` и `libssl_3.so`, arm64 SONAME/NEEDED, 16 KiB alignment,
INTERNET-only permissions и Media3 runtime; OpenSSL source digest и Apache-2.0
лицензия включены в скачиваемый комплект. GitHub Desktop run #151 прошёл Linux и
Windows сборки и CTest. APK/artifact подготовлен, но Android TLS backend на
устройстве и реальный Cloud Mail на Huawei остаются **НЕ ПРОВЕРЕНЫ**. По прежнему
APK доказано отсутствие OpenSSL runtime, однако исходная ошибка Huawei без новой
safe diagnostic строки точно не классифицирована. Изменения остаются в draft PR
#29; объединять только после ручной приёмки владельца.
[Инструкция](docs/002-D-ANDROID-CLOUD.md).
