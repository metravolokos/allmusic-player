# Проверки и артефакты GitHub Actions

CI проверяет изменения проекта. Обычный Draft PR не создаёт скачиваемые ZIP или APK.

## Режимы Pull Request

Оба workflow запускаются для всех PR в `main`; фильтров `paths` на уровне workflow нет, поэтому обязательные итоговые статусы существуют и для документации. Запускаются события `opened`, `synchronize`, `reopened`, `ready_for_review` и `converted_to_draft`. Изменение заголовка или текста PR (`edited`) тяжёлые проверки не запускает.

### Draft fast mode

Для Draft PR классификатор сравнивает последний commit исходной ветки с его первым родителем. Он не сканирует накопленную историю PR, поэтому обычный новый commit не повторяет прежние полные сборки.

Изменения workflow, классификатора и тестов CI в Draft также используют быстрые проверки: платформенные сборки пропускаются. При этом обязательные итоговые jobs завершаются ошибкой с объяснением, что полный CI перед merge не выполнен. При переводе в Ready запускается полный cumulative режим. Неизвестный путь или ошибка Git по-прежнему запускают полный fail-closed набор.

На каждом Draft событии запускаются:

- проверка документов `python3 scripts/check_repository.py`;
- все unit tests из `tests/ci/`;
- проверка синтаксиса Python CI-скриптов;
- лёгкие офлайн-тесты Android fixtures/package audit из `tests/android/`, без Qt, Android SDK, NDK или APK.

Эти проверки не подтверждают сборку Windows, Linux или Android binary. Draft PR не создаёт скачиваемый APK. Для Ready PR при запуске Android binary job после успешной сборки и package/license audit APK-комплект автоматически загружается в Artifacts того же Android workflow. Отдельный ручной `workflow_dispatch` для APK не требуется. Ручной `workflow_dispatch` с `source_ref` и `android_apk` сохраняется как дополнительная возможность.

### Ready full mode

При `ready_for_review`, а также при каждом последующем `synchronize` или `reopened`, если PR уже не Draft, классификатор проверяет накопленный `base...head` diff. Изменения платформы собираются и тестируются полностью; новая версия Draft-checks не используется как основание для merge.

| Изменённые файлы | Полные проверки |
|---|---|
| Android QML, Java, Gradle, Android assets и package scripts | Android build, tests и аудит APK; Desktop пропускается |
| Desktop-only playback, probe и тесты | Linux и Windows Desktop |
| Общие `src/cloud/` targets | Linux, Windows и Android |
| `src/playback/MailWebDavRangeDevice.*` | Linux, Windows и Android: класс входит в Desktop и Android CMake targets |
| `src/playback/LocalPlayback.*`, `WebDavRangeDevice.*`, Desktop tests | Linux и Windows Desktop |
| `src/app/`, корневой CMake и общие build files | Linux, Windows и Android; CMake также запускает нужные package audits |
| Упаковка или лицензии | Только сборки и аудит действительно затронутых комплектов |
| Только Markdown | Лёгкие документные проверки, без компиляторов |

Пустой diff, неизвестный путь/тип файла, неверный SHA или ошибка `git diff` включают полный набор платформ и аудитов (fail-closed). Изменения workflow, классификатора, их тестов или проверки документов запускают полный fail-closed набор в Ready PR. В Draft для этих известных CI-путей запускаются быстрые проверки, а обязательные итоговые статусы блокируются до полного PR CI.

## Обязательные статусы и `main`

Имена обязательных контекстов остаются **`Desktop CI result`** и **`Android CI result`**. Для PR итоговые jobs успешны только в режимах `ready-full` или `full-fail-closed`, и только если все обязательные проверки и выбранные платформенные jobs прошли. В режиме `draft-fast` быстрые jobs могут быть зелёными, но оба обязательных контекста предсказуемо завершаются ошибкой с пояснением «Draft: полная проверка перед merge не выполнена». Они не используют `skipped` или `neutral`, которые могут считаться успешными branch protection. Push в `main`, release и ручной `workflow_dispatch` сохраняют свою прежнюю логику.

Не затронутая платформа в полном PR режиме может быть `skipped` только если это разрешил классификатор; ожидаемая платформа должна завершиться успешно. Отчёт содержит режим, source SHA, проверяемый SHA, затронутые платформы и фактические результаты/пропуски jobs.

Проверка на ветке PR #31 подтвердила этот gate: в Draft на commit 01336ef8b6937b211267afe9ace698003452c2c4 обе группы быстрых проверок прошли, платформенные сборки были пропущены, а оба обязательных результата завершились ошибкой с merge-gate причиной ([Desktop #41227798342](https://github.com/metravolokos/allmusic-player/actions/runs/41227798342), [Android #41227798470](https://github.com/metravolokos/allmusic-player/actions/runs/41227798470)). Эта ошибка в Draft ожидаема и сохраняет блокировку merge.

GitHub API подтвердил, что `main` защищена и требует оба точных контекста. Настройки защиты в этой задаче не менялись. Подробный endpoint branch protection недоступен GitHub-интеграции; существующие правила merge через PR и актуальности ветки остаются ответственностью настроек владельца.

После успешной проверки PR push в защищённую `main` сохраняет лёгкий режим: документы и `tests/ci`, без повторных компиляторов; это относится и к уже проверенным PR с изменениями CI. Перед merge сам PR с изменёнными workflow/classifier обязан пройти полный fail-closed CI. Невалидная пара SHA, push вне `main`, пустой diff или неизвестный путь запускают полный набор проверок. Релиз всегда запускает полные Desktop/Android проверки и package audits.

## Артефакты и безопасность

- Draft PR не загружает Windows ZIP или Android APK. Ready PR, требующий Android binary job, **автоматически публикует Android APK и комплект лицензий только после успешной сборки и аудита APK** (14 дней), чтобы владелец мог установить приложение без ручного запуска workflow. Готовые Windows ZIP и WebDAV test kit из обычного PR не публикуются; если изменена упаковка, её проверяют на runner без автоматической выдачи ZIP.
- Синтетические тестовые аудиофикстуры могут сохраняться только на один день для платформенных тестов. Диагностика при ошибке сохраняется на три дня.
- Ручной `workflow_dispatch` сохраняет ZIP/APK только при выбранном флажке; релиз сохраняет комплекты 90 дней. Ready PR Android и ручные артефакты хранятся 14 дней.
- APK audit проверяет manifest с единственным `INTERNET` разрешением, подпись и выравнивание, ABI, Media3/Qt runtime и Maven license metadata. Неизвестная Maven-лицензия останавливает аудит.
- Windows ZIP содержит FFmpeg/Qt notices; WebDAV test kit собирает Qt Base/Declarative notices; Android kit содержит Qt/NDK notices, Media3 Apache-2.0 и распознанные лицензии Gradle runtime.
- Workflow сохраняют `permissions: contents: read`, `persist-credentials: false` в checkout и `concurrency.cancel-in-progress`. В кэши не попадают Gradle home, Android SDK/NDK, signing files, credentials, секреты, токены, ключи или медиатека.

## Ручная сборка и упаковка

1. Откройте **Actions** и выберите Desktop workflow или Android workflow (`.github/workflows/android.yml`).
2. Нажмите **Run workflow**. В `source_ref` введите нужную ветку, например `issue-30-ci-opt-002`, чтобы проверить именно её, даже если workflow запущен со страницы `main`.
3. Для Windows отметьте `windows_zip`, `webdav_test_kit` или обе опции. Для Android отметьте `android_apk` для скачиваемого комплекта либо `full_tests` для сборки и аудита без сохранения APK. Desktop `full_tests` запускает Linux и Windows.
4. Скачайте выбранный артефакт со страницы run. Отчёт и `BUILD-INFO.json` содержат source SHA.

`source_ref`, `full_tests`, `android_apk` и package inputs сохранены. APK для Ready PR с Android binary job загружается автоматически после аудита. Ручная сборка остаётся для отдельных запросов, Windows ZIP — по ручному запросу или при релизе. Автоматическая выгрузка после Ready не разрешает merge без обязательных успешных CI и ручного подтверждения владельца.

## Кэши

| Кэш | Что хранится и как меняется ключ | Почему выбран |
|---|---|---|
| Qt Desktop | Кэш `install-qt-action`; ключевой префикс и параметры Qt 6.12.0, ОС, архитектура, модули, `aqt` и URL установки сохранены | Повторная установка Qt занимала примерно 0,97–2,13 минуты на Desktop runner. При промахе действие ставит закреплённую версию. |
| Qt Android + host tools | Только каталоги Qt 6.12.0 для Android arm64-v8a и Linux host tools; обе архитектуры, выбранные архивы и commit закреплённого `aqt` сохранены в ключе | Установка Qt/aqt ранее занимала около 1,15 минуты. После восстановления проверяются `qmake` и точная Qt-версия. |
| Ninja | Только собранный бинарный файл Ninja 1.12.1; ОС, архитектура, компилятор и commit исходников сохранены в ключе | Холодная сборка ранее занимала примерно 0,70–1,35 минуты; при промахе Ninja собирается из закреплённого SHA. |
| Python pip и aqt | `setup-python` кэширует pip downloads; Android workflow отдельно кэширует wheel форка `aqtinstall` по закреплённому commit | Кэшируются повторные загрузки и закреплённый wheel, без Python runtime или установочных секретов. |

Кэш-конфигурация и формулы ключей в этом изменении не переписаны; они по-прежнему учитывают содержимое соответствующего workflow. Поэтому после первого изменения workflow может быть промах до создания нового кэш-объекта. Это не меняет список кэшируемых файлов. Кэш-проверка после изменения workflow/classifier запускается только после необходимых platform jobs и повторно приложение не компилирует.

SDK/NDK занимали около 0,77 минуты установки вместе; размер и стоимость переноса не оправданы без измеренной чистой выгоды. Gradle home остаётся одноразовым, так как содержит временные debug-signing материалы. Загрузка Gradle wrapper и аудит зависимостей ранее были короткими; весь Gradle каталог не кэшируется.

## Ранее измеренная база

Исторический baseline CI-OPT-001: семь merge-повторов на `main` после успешного Desktop PR израсходовали 69,06 фактических runner-минут (7 Linux и 7 Windows jobs), или 113,88 эквивалентных минут с коэффициентом Windows 2×. Это основание для лёгкого post-merge режима, но не измеренная экономия CI-OPT-002.

Два исторических PR только с Markdown потратили 20,37 runner-минут / 33,92 эквивалентных минут на Desktop jobs, в среднем 10,19 / 16,96 минут на PR. До оптимизации 001 полный трёхплатформенный запуск занял 18,59 runner-минут / 25,96 эквивалентных минут. Данные относятся к прежней схеме, а не к новому Draft fast mode.

## Измерения CI-OPT-002

### Полный fail-closed прогон

На первом commit ab1309047c80ced03ab9e20b8fc640ca72640917 изменения workflow и классификатора намеренно вызвали полный fail-closed профиль. Успешные запуски: [Desktop #41212231519](https://github.com/metravolokos/allmusic-player/actions/runs/41212231519) и [Android #41212231535](https://github.com/metravolokos/allmusic-player/actions/runs/41212231535). Linux, Windows и Android, обе проверки комплектов, валидация кэшей, классификаторы и итоговые контексты завершились успешно; Android fast job ожидаемо пропущен.

По GitHub Jobs API суммировано время между началом и завершением каждого исполнявшегося job. Пропущенные задания дают 0 секунд. Сравнение относится именно к этому fail-closed integration run и одному последующему Draft fast run, а не является прогнозом для любого изменения.

| Наблюдение | Значение |
|---|---:|
| Исполнявшиеся jobs | 12; ещё 1 Android fast job пропущен |
| Суммарное фактическое runner time | 20.22 мин. (1213 сек.) |
| Эквивалент с Windows runner ×2 | 28.55 мин. |
| Оценка округлённого эквивалента: каждое задание округлено вверх до минуты, Windows ×2 | 37 мин. |

В Desktop PR run опубликованы только законные синтетические аудиофикстуры на 1 день; Android workflow не опубликовал артефактов. ZIP, APK и комплекты лицензий не публиковались.

### Draft fast mode

Измерен один Draft запуск до добавления merge-gate защиты после документационного commit 97b6671450e31b854dcea24964953206892bf523: [Desktop #41213093024](https://github.com/metravolokos/allmusic-player/actions/runs/41213093024) и [Android #41213092971](https://github.com/metravolokos/allmusic-player/actions/runs/41213092971). Быстрые `documentation`, `tests/ci` и Android offline/package-audit checks прошли; Linux, Windows, Android binary, fixtures и cache validation jobs были пропущены. В том историческом запуске required results также были зелёными; после описанного выше исправления они намеренно блокируются в Draft. APK и другие артефакты не публиковались.

| Профиль | Фактическое runner time | Эквивалент с Windows ×2 | Округлённый эквивалент по минутам jobs |
|---|---:|---:|---:|
| Полный fail-closed reference | 20.22 мин. | 28.55 мин. | 37 мин. |
| Draft fast run | 0.43 мин. (26 сек.) | 0.43 мин. | 6 мин. |
| Разница для этой пары запусков | 19.78 мин. | 28.12 мин. | 31 мин. |

По взвешенному runner time измеренная разница составляет 98.5%. Это сравнение одного Draft fast запуска с одним намеренно fail-closed полным запуском, а не обещание такой же экономии для всех PR или сводная годовая экономия.
