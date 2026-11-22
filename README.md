# Earth And Legend — порт для PlayStation Vita

[Русский](#russian) · [English](#english)

<img src="extras/livearea/icon0.png" width="128" height="128" alt="Earth And Legend">

Порт Android-игры **Earth And Legend 3.0.0** (`com.dvidearts.earthandlegend`)
на PlayStation Vita. Порт работает на оригинальном ARMv7-коде движка
**ShiVa3D** (`libS3DClient.so`) и не содержит файлов самой игры — их нужно
подготовить из вашего APK.

<a id="russian"></a>

## Правовая информация

- Все права на игру **Earth And Legend**, её название, графику, звуки, музыку,
  тексты, шрифты, код движка ShiVa3D и игровые данные принадлежат **DVide Arts**
  и/или соответствующим правообладателям.
- Этот репозиторий содержит **только код порта**, сборочные скрипты и оформление
  LiveArea. В нём **нет** APK, `libS3DClient.so`, `S3DMain.stk`, текстур, музыки,
  шрифтов и других файлов игры.
- Для запуска необходимо самостоятельно иметь APK версии 3.0.0 и подготовить из
  него данные описанным ниже способом.
- Порт не связан с DVide Arts и не одобрен ими. Используйте на свой риск и
  только с легально приобретённой копией игры.
- Лицензия MIT в файле [LICENSE](LICENSE) относится только к коду порта, но не
  к игре и её материалам.

## Об игре

**Earth And Legend** — трёхмерная ролевая игра с открытым миром от студии
DVide Arts, выпущенная для Android и iOS в начале 2010-х. Действие происходит в
фэнтезийном мире, который можно свободно исследовать: города, подземелья, леса,
пустыни и снежные области. Игрок выполняет задания NPC, сражается с монстрами в
реальном времени, прокачивает персонажа, собирает оружие, броню и предметы,
использует боевые и магические умения, торгует, отдыхает и путешествует по карте
мира. В игре есть смена дня и ночи, погода, инвентарь, журнал заданий и
многопользовательский режим (серверы DVide Arts давно закрыты).

Порт нацелен на последнюю Android-версию **3.0.0**

## Что работает

- Рендеринг через vitaGL в нативном ландшафте 960×544.
- Загрузка оригинального игрового пака `S3DMain.stk`.
- Сенсорный экран (до 5 одновременных касаний).
- Физические кнопки и оба аналоговых стика.
- Музыка и звуковые эффекты без треска (собственный OpenAL-микшер).
- Сохранения в `ux0:data/earthandlegend/`.
- Корректный выход из игры без падения.

## Ограничения

- **Нет сети и мультиплеера.** Серверы DVide Arts закрыты, сетевые вызовы
  заглушены.
- **Только версия 3.0.0.** Данные других версий (другой размер `libS3DClient.so` и пака)
  не подходят и не поддерживаются.
- Встроенный шрифт не поставляется. Если игра не находит шрифт, положите
  TTF-файлы в `ux0:data/earthandlegend/fonts/` — обращения движка к
  `/system/fonts/...` перенаправляются туда.

## Управление

Игра изначально сенсорная, поэтому физические органы управления преобразуются в
события, понятные движку: Android-коды клавиш, синтетические касания и
перетаскивания.

| PS Vita | Действие |
|---|---|
| Левый стик | Ходьба (8 направлений, синтез D-pad) |
| Правый стик | Камера (виртуальное перетаскивание пальцем) |
| Cross | Взаимодействие с объектами и NPC, применение умения |
| Square | Умение |
| Circle | Умение / «назад» |
| L1 / R1 | Переключение наборов умений |
| START | Инвентарь (тап по портрету в левом верхнем углу) |
| SELECT | Карта (тап по глобусу в левом нижнем углу) |
| D-pad | Навигация по меню |

## Установка

Что нужно на консоли:

- PS Vita с HENkaku/ensō и VitaShell;
- плагин [kubridge](https://github.com/bythos14/kubridge) (нужен для
  RWX-памяти);
- `libshacccg.suprx` в `ur0:/data/` (нужен vitaGL).

Порядок:

1. Установите `earthandlegend.vpk` через VitaShell.
2. Подготовьте папку с данными игры (см. следующий раздел) и скопируйте её
   содержимое в `ux0:data/earthandlegend/`.
3. Запустите игру из LiveArea.

`config.txt` необязателен: без него используются значения по умолчанию.

## Подготовка файлов игры из APK

В репозитории нет файлов игры, поэтому их нужно извлечь из APK **3.0.0**
(`com.dvidearts.earthandlegend`). Другие версии не подходят.

### Автоматически

В репозитории есть скрипт `tools/prepare_data.py` (нужен Python 3):

```sh
python tools/prepare_data.py "Earth+And+Legend+[3.0.0].apk" earthandlegend
```

Скрипт создаст папку `earthandlegend/` с файлами `libS3DClient.so`,
`S3DMain.stk`, `config.txt` и пустыми каталогами `cache/`, `home/`, `fonts/`.
Содержимое этой папки скопируйте в `ux0:data/earthandlegend/`.

### Вручную

1. Убедитесь, что это APK версии 3.0.0: пакет `com.dvidearts.earthandlegend`,
   `versionName` = `3.0.0`.
2. Распакуйте APK как обычный ZIP-архив.

   Windows (PowerShell):

   ```powershell
   Rename-Item "Earth+And+Legend+[3.0.0].apk" earthandlegend.zip
   Expand-Archive earthandlegend.zip -DestinationPath apk
   ```

   Linux/macOS:

   ```sh
   unzip "Earth+And+Legend+[3.0.0].apk" -d apk
   ```

   Можно также открыть APK любым архиватором (7-Zip, WinRAR и т. п.).

3. Возьмите из архива два файла:

   | Файл в APK | Куда положить |
   |---|---|
   | `lib/armeabi-v7a/libS3DClient.so` | `earthandlegend/libS3DClient.so` |
   | `assets/S3DMain.smf` | `earthandlegend/S3DMain.stk` |

   **Важно:** `S3DMain.smf` нужно переименовать в `S3DMain.stk` — движок ищет
   пак именно с таким именем. Библиотеки из других ABI (`armeabi`, `x86`) не
   нужны.

4. Создайте рядом пустые каталоги `cache/`, `home/` и `fonts/`. Сохранения,
   кэш шейдеров и шрифты игра создаёт и использует сама.

5. При желании скопируйте `config.txt` из репозитория и настройте параметры
   (см. раздел «config.txt»).

6. Скопируйте получившуюся папку на консоль так, чтобы получился путь
   `ux0:data/earthandlegend/`. Удобнее всего через VitaShell (USB или FTP).

Для проверки, что версия правильная, можно сверить размеры:

| Файл | Ориентировочный размер |
|---|---|
| `libS3DClient.so` | 9 216 052 байт |
| `S3DMain.stk` | 47 934 549 байт |

Небольшое расхождение допустимо, но если размеры отличаются в разы — это
другая версия игры или повреждённый APK.

### Итоговая структура на консоли

```
ux0:data/earthandlegend/
+-- libS3DClient.so     # из APK: lib/armeabi-v7a/libS3DClient.so
+-- S3DMain.stk         # из APK: assets/S3DMain.smf (переименовать!)
+-- config.txt          # необязательный
+-- cache/              # создаётся автоматически
+-- home/               # создаётся автоматически (сохранения)
+-- fonts/              # создаётся автоматически (шрифты .ttf)
L-- eal.log             # диагностический лог (создаётся автоматически)
```

## config.txt

Файл необязательный. Формат — пары `ключ значение`, по одной на строку:

```
setting_openal 1
setting_mipmaps 0
setting_dxtDecode 1
setting_dxtFormat 1
setting_v3Sync 0
setting_cpuCount 0
setting_packCache 1
setting_vsync 1
setting_cameraAnchorX 720
setting_cameraAnchorY 272
setting_cameraSpeed 6
setting_startTapX 50
setting_startTapY 28
setting_selectTapX 50
setting_selectTapY 485
```

- `setting_openal` — `1` (по умолчанию) включает музыку и эффекты через OpenAL
  и `sceAudioOut`; `0` — полностью без звука.
- `setting_mipmaps` — `1` разрешает mipmap-фильтрацию, `0` (по умолчанию)
  принудительно использует базовый уровень. Может убрать пиксельные артефакты
  интерфейса.
- `setting_dxtDecode` — `1` (по умолчанию) распаковывает DXT-текстуры на CPU,
  `0` использует нативные GXM UBC-текстуры. Значение `1` убирает блочные
  артефакты на персонаже и земле.
- `setting_dxtFormat` — формат распакованных текстур при `setting_dxtDecode 1`:
  `0` — RGBA8888 (лучшее качество, 4 байта на тексель), `1` (по умолчанию) —
  RGB565/RGBA4444 (вдвое меньше памяти).
- `setting_v3Sync` — `0` (по умолчанию) использует встроенный асинхронный
  рендерер движка (быстрее), `1` принудительно включает синхронный режим
  (медленнее, для отладки).
- `setting_cpuCount` — сколько ядер сообщать движку: `0` (по умолчанию) —
  автоматически 3 пользовательских ядра, `1` — однопоточный режим, `2..3` —
  точное число.
- `setting_packCache` — `1` (по умолчанию) кэширует блоки `S3DMain.stk` в RAM и
  отдаёт мелкие чтения движка из памяти; `0` — читать файл напрямую.
- `setting_vsync` — `1` (по умолчанию) ждёт вертикальную синхронизацию (без
  разрывов); `0` — выводит кадр сразу.
- `setting_cameraAnchorX/Y` — точка, от которой начинается виртуальное
  перетаскивание для камеры.
- `setting_cameraSpeed` — скорость камеры в пикселях за 1/60 с при полном
  отклонении стика. Домножается на длительность кадра, поэтому скорость поворота
  не зависит от FPS. По умолчанию `6`; увеличьте до `10`–`16`, если хочется
  быстрее.
- `setting_startTapX/Y` — координаты кнопки инвентаря (портрет слева вверху) в
  системе 960×544.
- `setting_selectTapX/Y` — координаты кнопки карты (глобус слева внизу).

## Сборка из исходников

Требования:

- [VitaSDK-softfp](https://github.com/vitasdk-softfp#how-to-install), переменная
  окружения `VITASDK` указывает на него;
- `kubridge` на консоли;
- `libshacccg.suprx` в `ur0:/data/`;
- `make` (vitaGL собирается через Makefile);
- Python 3 — только для скрипта подготовки данных.

Сторонние библиотеки уже лежат в `lib/` и собираются вместе с портом:
`falso_jni`, `so_util`, `falso_ndk`, `vitagl`, `fios`, `sha1`, `kubridge` и
`libc_bridge`. Отдельно скачивать их не нужно. Из `lib/vitagl` удалены примеры
(`samples/`), чтобы репозиторий не раздувался, — на сборку это не влияет.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Результат — `build/earthandlegend.vpk`.

Для отладочной сборки с логами:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

Под Windows удобно собирать в WSL2 или MSYS2 с установленным VitaSDK.

## Метод портирования

Порт сделан по схеме **so-loader**: оригинальная ARMv7-библиотека движка
загружается и исполняется на Vita как есть, а всё окружение Android
воссоздаётся нативной обвязкой.

1. **Загрузка оригинального кода.** `libS3DClient.so` читается с карты памяти и
   загружается в память через `so_util`. `kubridge` даёт порту RWX-память,
   необходимую для сегментов кода. Импорты библиотеки (libc, pthread, EGL,
   GLES1, OpenAL, JNI) не загружаются из Android, а подставляются таблицей
   символов в `source/dynlib.c`.
2. **Воссоздание Java-слоя.** В оригинале игру запускает `Activity`, которая
   вызывает JNI-методы класса `S3DRenderer`. Порт вызывает те же точки входа
   напрямую из нативного `main()` в том же порядке: настройка камеры, модели
   устройства, версии, языка, каталогов и пака, затем
   `engineOnSurfaceCreated`, `engineOnSurfaceChanged(960, 544)`,
   `engineShutdown`, `engineInitialize` и цикл кадра 3.0.0.
3. **JNI-заглушки.** `FalsoJNI` создаёт `JavaVM`/`JNIEnv`, поэтому движок может
   вызывать Java-методы. Колбэки (звук, биллинг, локаль, сведения об устройстве,
   вибрация, сеть) реализованы нативно в `source/java.c`; `FalsoNDK` закрывает
   потребности NDK-совместимости.
4. **Графика.** Вызовы GLES1 с фиксированным конвейером транслируются в
   [vitaGL](https://github.com/Rinnegatamante/vitaGL), который работает поверх
   GXM. Шейдеры компилируются через `libshacccg` и кэшируются. DXT- и
   ETC1-текстуры при необходимости декодируются на CPU
   (`source/reimpl/dxt.c`), чтобы избежать блочных артефактов и проблем с
   форматами GXM.
5. **Звук.** Вместо Android OpenAL порт предоставляет собственную реализацию
   OpenAL (`source/reimpl/openal.c`): программный микшер, потоковые источники и
   вывод через `sceAudioOut` на 48 кГц. Музыка и эффекты сводятся без треска,
   состояние микшера пишется в лог.
6. **Ввод.** Движок не имеет точек входа для геймпада, поэтому физические органы
   управления переводятся в те события, которые он понимает: коды клавиш
   Android, синтетические касания и перетаскивания. Левый стик синтезирует
   D-pad, правый — перетаскивание для камеры, START/SELECT — тапы по HUD.
7. **Файлы и сохранения.** Пути Android (`/sdcard/...`, `/system/fonts/...`)
   перенаправляются в `ux0:data/earthandlegend/`. Пак `S3DMain.stk` читается
   мелкими блоками, поэтому включён RAM-кэш. Сохранения пишутся в `home/`.
8. **Кадр.** 3.0.0 умеет асинхронный рендерер: логика движка и отправка
   draw-вызовов идут в отдельном потоке, основной поток выводит кадр. Это даёт
   около 55–59 FPS в тяжёлых сценах; синхронный режим (`setting_v3Sync 1`)
   оставлен для отладки.

### Структура репозитория

```
EarthAndLegend-Vita/
+-- CMakeLists.txt              # сборка VPK, LiveArea, флаги vitaGL
+-- config.txt                  # конфиг по умолчанию (копируется в data)
+-- LICENSE                     # MIT для кода порта
+-- README.md
+-- extras/
|   +-- livearea/               # иконка, фон, заставка, template.xml
|   +-- cpuinfo, meminfo, ...   # файлы, которые движок читает из /proc
+-- lib/                        # вендоренные библиотеки (см. ниже)
+-- source/
|   +-- main.c                  # точка входа, кадровый цикл, ввод
|   +-- engine.c/.h             # обёртки над JNI-экспортами S3DRenderer
|   +-- java.c/.h               # FalsoJNI: методы и колбэки движка
|   +-- dynlib.c                # таблица импортов и мосты GLES1
|   +-- patch.c                 # патчи загрузчика
|   +-- reimpl/
|   |   +-- controls.c/.h       # кнопки, стики, тач, START/SELECT
|   |   +-- openal.c/.h         # OpenAL-микшер и sceAudioOut
|   |   +-- present.c/.h        # вывод кадра (в 3.0.0 — напрямую)
|   |   +-- dxt.c/.h            # декодирование текстур
|   |   +-- io.c, sys.c, ...    # libc/NDK-совместимость
|   +-- utils/
|       +-- settings.c/.h       # разбор config.txt
|       +-- trace.c/.h          # лог eal.log
+-- tools/
    +-- prepare_data.py         # извлечение данных из APK
    +-- prepare_livearea.py     # конвертация LiveArea PNG в 8-битную палитру
```

## Диагностика

Порт ведёт файл `ux0:data/earthandlegend/eal.log`. В него записываются этапы
запуска, вызовы звуковых колбэков и строка производительности раз в 5 секунд:

```
perf: 55.3 fps, engine 24.1 ms, present 4.2 ms, 240 draws/frame
```

`engine` — время процессорной работы движка и отправки draw-вызовов,
`present` — ожидание GPU и вертикальной синхронизации. Если `engine` намного
больше `present`, упор в CPU; если наоборот — в GPU.

Раз в ~8 секунд пишется состояние аудиомикшера:

```
openal: mix peak=0.61 underruns=0 clicks=0
```

`peak` — пик микса до ограничителя, `underruns` — недокорм потоковых
источников, `clicks` — резкие скачки в выходном сигнале. В норме
`underruns 0` и `clicks 0`.

Свободная память внутри пулов vitaGL пишется отдельной строкой:

```
vgl: free vram=30304K ram=3533K slow=4778K
```

Если игра вылетает, пришлите последние строки `eal.log`.

## Использованные проекты

| Проект | Что использовано | Ссылка |
|---|---|---|
| soloader-boilerplate | каркас порта, CMake, связка с so_util | https://github.com/v-atamanenko/soloader-boilerplate |
| vitaGL | реализация OpenGL ES поверх GXM | https://github.com/Rinnegatamante/vitaGL |
| so_util | загрузчик и линковщик Android `.so` | https://github.com/Rinnegatamante/so_util |
| kubridge | RWX-память и низкоуровневые патчи | https://github.com/bythos14/kubridge |
| FalsoJNI | подмена JavaVM/JNIEnv | https://github.com/Rinnegatamante/FalsoJNI |
| FalsoJNI (форк) | версия, использованная в порте | https://github.com/v-atamanenko/FalsoJNI |
| FalsoNDK | совместимость с Android NDK | https://github.com/elliencode/FalsoNDK |
| SceLibcBridge | мосты libc | https://github.com/Rinnegatamante/SceLibcBridge |
| fios | FIOS2-оверлей для файлового ввода-вывода | https://github.com/TheOfficialFloW/conduit_vita |
| sha1 | SHA-1 (Brad Conte) | https://github.com/B-Con/crypto-algorithms |
| VitaSDK-softfp | тулчейн | https://github.com/vitasdk-softfp |

Отдельно спасибо [TheFloW](https://github.com/TheOfficialFloW) за оригинальный
so-loader и [Rinnegatamante](https://github.com/Rinnegatamante) за vitaGL и
множество библиотек, без которых порт бы не собрался.

## Лицензия

Код порта распространяется по лицензии MIT. См. [LICENSE](LICENSE).
Исходная игра, её название, графика, звук, тексты и игровые данные принадлежат
DVide Arts и/или соответствующим правообладателям. Репозиторий не содержит
файлов игры.

---

<a id="english"></a>

# Earth And Legend — PlayStation Vita port

[Русский](#russian) · [English](#english)

<img src="extras/livearea/icon0.png" width="128" height="128" alt="Earth And Legend">

A port of the Android game **Earth And Legend 3.0.0**
(`com.dvidearts.earthandlegend`) to the PlayStation Vita. The port runs the
original ARMv7 **ShiVa3D** engine code (`libS3DClient.so`) and ships no game
files — you have to prepare them from your own APK.

## Legal notice

- All rights to **Earth And Legend**, its name, artwork, sounds, music, texts,
  fonts, the ShiVa3D engine code and the game data belong to **DVide Arts**
  and/or their respective owners.
- This repository contains **only the port's source code**, build scripts and
  LiveArea artwork. It does **not** contain the APK, `libS3DClient.so`,
  `S3DMain.stk`, textures, music, fonts or any other game file.
- To play, you must own the 3.0.0 APK and prepare the data as described below.
- This port is not affiliated with or endorsed by DVide Arts. Use it at your own
  risk and only with a legally obtained copy of the game.
- The MIT license in [LICENSE](LICENSE) covers the port's code only, not the
  game or its assets.

## About the game

**Earth And Legend** is a 3D open-world role-playing game by DVide Arts,
released for Android and iOS in the early 2010s. It is set in a fantasy world
you can explore freely: towns, dungeons, forests, deserts and snowy regions.
The player completes NPC quests, fights monsters in real time, levels up the
character, collects weapons, armour and items, uses combat and magic skills,
trades, rests and travels across the world map. The game features a day/night
cycle, weather, an inventory, a quest journal and an online multiplayer mode
(the DVide Arts servers have long been shut down).

The port targets the final Android release, **3.0.0**.

## What works

- Rendering through vitaGL in native 960×544 landscape.
- Loading the original `S3DMain.stk` game pack.
- Touch screen (up to 5 simultaneous touches).
- Physical buttons and both analog sticks.
- Music and sound effects without crackling (custom OpenAL mixer).
- Saves in `ux0:data/earthandlegend/`.
- Clean exit without a crash.

## Limitations

- **No network or multiplayer.** The DVide Arts servers are gone; network calls
  are stubbed out.
- **Version 3.0.0 only.** Data from other versions (different `libS3DClient.so`
  and pack sizes) will not work and is not supported.
- No built-in font is shipped. If the game cannot find a font, put TTF files
  into `ux0:data/earthandlegend/fonts/` — the engine's `/system/fonts/...`
  requests are redirected there.

## Controls

The game is touch-first, so physical controls are translated into the events the
engine understands: Android key codes, synthetic touches and drags.

| PS Vita | Action |
|---|---|
| Left stick | Walking (8 directions, synthesized D-pad) |
| Right stick | Camera (virtual finger drag) |
| Cross | Interact with objects and NPCs, use a skill |
| Square | Skill |
| Circle | Skill / "back" |
| L1 / R1 | Switch skill sets |
| START | Inventory (tap on the top-left portrait) |
| SELECT | Map (tap on the bottom-left globe) |
| D-pad | Menu navigation |

## Installation

Requirements on the console:

- PS Vita with HENkaku/ensō and VitaShell;
- the [kubridge](https://github.com/bythos14/kubridge) plugin (RWX memory);
- `libshacccg.suprx` in `ur0:/data/` (required by vitaGL).

Steps:

1. Install `earthandlegend.vpk` with VitaShell.
2. Prepare the game data folder (see the next section) and copy its contents to
   `ux0:data/earthandlegend/`.
3. Launch the game from the LiveArea.

`config.txt` is optional; the defaults are used without it.

## Preparing the game files from the APK

The repository ships no game files, so you have to extract them from the
**3.0.0** APK (`com.dvidearts.earthandlegend`). Other versions are not
supported.

### Automatic

The repository includes `tools/prepare_data.py` (requires Python 3):

```sh
python tools/prepare_data.py "Earth+And+Legend+[3.0.0].apk" earthandlegend
```

The script creates an `earthandlegend/` folder containing `libS3DClient.so`,
`S3DMain.stk`, `config.txt` and the empty `cache/`, `home/` and `fonts/`
directories. Copy the contents of that folder to `ux0:data/earthandlegend/`.

### Manual

1. Make sure this is the 3.0.0 APK: package `com.dvidearts.earthandlegend`,
   `versionName` = `3.0.0`.
2. Extract the APK as a regular ZIP archive.

   Windows (PowerShell):

   ```powershell
   Rename-Item "Earth+And+Legend+[3.0.0].apk" earthandlegend.zip
   Expand-Archive earthandlegend.zip -DestinationPath apk
   ```

   Linux/macOS:

   ```sh
   unzip "Earth+And+Legend+[3.0.0].apk" -d apk
   ```

   Any archive tool (7-Zip, WinRAR, etc.) works as well.

3. Take two files from the archive:

   | File in the APK | Destination |
   |---|---|
   | `lib/armeabi-v7a/libS3DClient.so` | `earthandlegend/libS3DClient.so` |
   | `assets/S3DMain.smf` | `earthandlegend/S3DMain.stk` |

   **Important:** rename `S3DMain.smf` to `S3DMain.stk` — the engine looks for
   the pack under exactly that name. Libraries from other ABIs (`armeabi`,
   `x86`) are not needed.

4. Create the empty `cache/`, `home/` and `fonts/` directories next to the
   files. The game creates and uses them for saves, the shader cache and fonts.

5. Optionally copy `config.txt` from the repository and adjust the settings
   (see the "config.txt" section).

6. Copy the resulting folder to the console so that the final path is
   `ux0:data/earthandlegend/`. VitaShell (USB or FTP) is the easiest way.

You can verify the version by file sizes:

| File | Approximate size |
|---|---|
| `libS3DClient.so` | 9,216,052 bytes |
| `S3DMain.stk` | 47,934,549 bytes |

A small difference is fine, but a large one means a different game version or a
corrupted APK.

### Resulting layout on the console

```
ux0:data/earthandlegend/
+-- libS3DClient.so     # from the APK: lib/armeabi-v7a/libS3DClient.so
+-- S3DMain.stk         # from the APK: assets/S3DMain.smf (renamed!)
+-- config.txt          # optional
+-- cache/              # created automatically
+-- home/               # created automatically (saves)
+-- fonts/              # created automatically (.ttf fonts)
L-- eal.log             # diagnostic log (created automatically)
```

## config.txt

The file is optional. The format is `key value` pairs, one per line:

```
setting_openal 1
setting_mipmaps 0
setting_dxtDecode 1
setting_dxtFormat 1
setting_v3Sync 0
setting_cpuCount 0
setting_packCache 1
setting_vsync 1
setting_cameraAnchorX 720
setting_cameraAnchorY 272
setting_cameraSpeed 6
setting_startTapX 50
setting_startTapY 28
setting_selectTapX 50
setting_selectTapY 485
```

- `setting_openal` — `1` (default) enables music and effects through OpenAL and
  `sceAudioOut`; `0` is completely silent.
- `setting_mipmaps` — `1` allows mipmap filtering, `0` (default) forces the base
  mip level. May remove pixel artefacts in the UI.
- `setting_dxtDecode` — `1` (default) decodes DXT textures on the CPU, `0` uses
  native GXM UBC textures. `1` removes block artefacts on the character and the
  ground.
- `setting_dxtFormat` — format of decoded textures when `setting_dxtDecode 1`:
  `0` — RGBA8888 (best quality, 4 bytes per texel), `1` (default) —
  RGB565/RGBA4444 (half the memory).
- `setting_v3Sync` — `0` (default) uses the engine's asynchronous renderer
  (faster), `1` forces the synchronous path (slower, for debugging).
- `setting_cpuCount` — number of cores reported to the engine: `0` (default) —
  automatically 3 user cores, `1` — single-threaded, `2..3` — exact number.
- `setting_packCache` — `1` (default) caches `S3DMain.stk` blocks in RAM and
  serves the engine's small reads from memory; `0` reads the file directly.
- `setting_vsync` — `1` (default) waits for vblank (no tearing); `0` presents
  immediately.
- `setting_cameraAnchorX/Y` — anchor point of the virtual camera drag.
- `setting_cameraSpeed` — camera speed in pixels per 1/60 s at full stick tilt.
  It is scaled by the frame time, so the turn rate does not depend on FPS. The
  default is `6`; raise it to `10`–`16` if you want a faster camera.
- `setting_startTapX/Y` — inventory button coordinates (top-left portrait) in
  the 960×544 space.
- `setting_selectTapX/Y` — map button coordinates (bottom-left globe).

## Building from source

Requirements:

- [VitaSDK-softfp](https://github.com/vitasdk-softfp#how-to-install) with the
  `VITASDK` environment variable pointing to it;
- `kubridge` on the console;
- `libshacccg.suprx` in `ur0:/data/`;
- `make` (vitaGL is built through its Makefile);
- Python 3 — only for the data preparation script.

All third-party libraries are vendored in `lib/` and built together with the
port: `falso_jni`, `so_util`, `falso_ndk`, `vitagl`, `fios`, `sha1`, `kubridge`
and `libc_bridge`. There is nothing to download separately. The vitaGL samples
(`samples/`) were removed to keep the repository small; this does not affect the
build.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The result is `build/earthandlegend.vpk`.

Debug build with logging:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

On Windows, build in WSL2 or MSYS2 with VitaSDK installed.

## How the port works

The port follows the **so-loader** approach: the original ARMv7 engine library
is loaded and executed on the Vita as is, while the whole Android environment is
recreated by a native shim.

1. **Loading the original code.** `libS3DClient.so` is read from the memory card
   and loaded by `so_util`. `kubridge` provides the RWX memory required for code
   segments. The library's imports (libc, pthread, EGL, GLES1, OpenAL, JNI) are
   not taken from Android; they are resolved through the symbol table in
   `source/dynlib.c`.
2. **Recreating the Java layer.** On Android the game is started by an
   `Activity` that calls the JNI methods of `S3DRenderer`. The port calls the
   same entry points directly from a native `main()`, in the same order: camera,
   device model, versions, locale, directories and pack setup, then
   `engineOnSurfaceCreated`, `engineOnSurfaceChanged(960, 544)`,
   `engineShutdown`, `engineInitialize`, followed by the 3.0.0 frame loop.
3. **JNI stubs.** `FalsoJNI` provides `JavaVM`/`JNIEnv`, so the engine can call
   Java methods. The callbacks (audio, billing, locale, device information,
   vibration, network) are implemented natively in `source/java.c`; `FalsoNDK`
   covers NDK compatibility.
4. **Graphics.** Fixed-function GLES1 calls are translated to
   [vitaGL](https://github.com/Rinnegatamante/vitaGL), which runs on top of GXM.
   Shaders are compiled by `libshacccg` and cached. DXT and ETC1 textures are
   decoded on the CPU when needed (`source/reimpl/dxt.c`) to avoid block
   artefacts and GXM format issues.
5. **Audio.** Instead of Android OpenAL, the port provides its own OpenAL
   implementation (`source/reimpl/openal.c`): a software mixer, streaming
   sources and output through `sceAudioOut` at 48 kHz. Music and effects are
   mixed without crackling, and the mixer state is written to the log.
6. **Input.** The engine exposes no gamepad entry points, so physical controls
   are translated into the events it does understand: Android key codes,
   synthetic touches and drags. The left stick synthesizes the D-pad, the right
   stick produces a camera drag, and START/SELECT are HUD taps.
7. **Files and saves.** Android paths (`/sdcard/...`, `/system/fonts/...`) are
   redirected to `ux0:data/earthandlegend/`. `S3DMain.stk` is read in very small
   blocks, so a RAM cache is enabled. Saves are written to `home/`.
8. **Frame loop.** 3.0.0 has an asynchronous renderer: engine logic and draw-call
   submission run on a worker thread while the main thread presents the frame.
   This gives around 55–59 FPS in heavy scenes; the synchronous mode
   (`setting_v3Sync 1`) is kept for debugging.

### Repository layout

```
EarthAndLegend-Vita/
+-- CMakeLists.txt              # VPK build, LiveArea, vitaGL flags
+-- config.txt                  # default config (copied to the data folder)
+-- LICENSE                     # MIT for the port's code
+-- README.md
+-- extras/
|   +-- livearea/               # icon, background, startup image, template.xml
|   +-- cpuinfo, meminfo, ...   # files the engine reads from /proc
+-- lib/                        # vendored libraries (see above)
+-- source/
|   +-- main.c                  # entry point, frame loop, input
|   +-- engine.c/.h             # wrappers around S3DRenderer JNI exports
|   +-- java.c/.h               # FalsoJNI: engine methods and callbacks
|   +-- dynlib.c                # import table and GLES1 bridges
|   +-- patch.c                 # loader patches
|   +-- reimpl/
|   |   +-- controls.c/.h       # buttons, sticks, touch, START/SELECT
|   |   +-- openal.c/.h         # OpenAL mixer and sceAudioOut
|   |   +-- present.c/.h        # frame presentation (direct in 3.0.0)
|   |   +-- dxt.c/.h            # texture decoding
|   |   +-- io.c, sys.c, ...    # libc/NDK compatibility
|   +-- utils/
|       +-- settings.c/.h       # config.txt parser
|       +-- trace.c/.h          # eal.log writer
+-- tools/
    +-- prepare_data.py         # APK data extraction
    +-- prepare_livearea.py     # converts LiveArea PNGs to 8-bit palette
```

## Diagnostics

The port writes `ux0:data/earthandlegend/eal.log`. It records the boot stages,
audio callback calls and a performance line every 5 seconds:

```
perf: 55.3 fps, engine 24.1 ms, present 4.2 ms, 240 draws/frame
```

`engine` is the CPU time spent in engine logic and draw-call submission,
`present` is waiting for the GPU and vblank. If `engine` is much larger than
`present`, the CPU is the bottleneck; if the opposite, it is the GPU.

Every ~8 seconds the audio mixer state is logged:

```
openal: mix peak=0.61 underruns=0 clicks=0
```

`peak` is the mix peak before the limiter, `underruns` counts starved streaming
sources, and `clicks` counts sharp jumps in the output signal. Normally
`underruns 0` and `clicks 0`.

Free memory inside the vitaGL pools is logged separately:

```
vgl: free vram=30304K ram=3533K slow=4778K
```

If the game crashes, attach the last lines of `eal.log` to your report.

## Projects used

| Project | Used for | Link |
|---|---|---|
| soloader-boilerplate | port skeleton, CMake, so_util glue | https://github.com/v-atamanenko/soloader-boilerplate |
| vitaGL | OpenGL ES implementation on top of GXM | https://github.com/Rinnegatamante/vitaGL |
| so_util | Android `.so` loader and linker | https://github.com/Rinnegatamante/so_util |
| kubridge | RWX memory and low-level patches | https://github.com/bythos14/kubridge |
| FalsoJNI | JavaVM/JNIEnv replacement | https://github.com/Rinnegatamante/FalsoJNI |
| FalsoJNI (fork) | the version used by this port | https://github.com/v-atamanenko/FalsoJNI |
| FalsoNDK | Android NDK compatibility | https://github.com/elliencode/FalsoNDK |
| SceLibcBridge | libc bridges | https://github.com/Rinnegatamante/SceLibcBridge |
| fios | FIOS2 overlay for file I/O | https://github.com/TheOfficialFloW/conduit_vita |
| sha1 | SHA-1 (Brad Conte) | https://github.com/B-Con/crypto-algorithms |
| VitaSDK-softfp | toolchain | https://github.com/vitasdk-softfp |

Additional thanks to [TheFloW](https://github.com/TheOfficialFloW) for the
original so-loader and to [Rinnegatamante](https://github.com/Rinnegatamante)
for vitaGL and the many libraries without which this port would not build.

## License

The port's code is distributed under the MIT license. See [LICENSE](LICENSE).
The original game, its name, artwork, audio, texts and game data belong to DVide
Arts and/or their respective owners. This repository contains no game files.
