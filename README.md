# Cask

Менеджер Wine-префиксов на C++/Qt (аналог Bottles). Версия 0.1: всё скачивается вручную.

## Портативная раскладка

```
Cask/
├── cask            бинарник
├── cask.ini        настройки (создаётся рядом с бинарником)
├── lib/            зависимости (Qt и др.), RPATH = $ORIGIN/lib
├── lang/           необязательные переводы <код>.json (см. ниже)
├── data/
│   ├── runners/<имя>/bin/wine      сборки Wine
│   ├── d7vk/<версия>/{x64,x32}/*.dll
│   ├── dxvk/<версия>/{x64,x32}/*.dll
│   ├── vkd3d-proton/<версия>/{x64,x86}/*.dll
│   ├── winetricks/                 скрипт winetricks (по желанию) и cache/
│   └── mangohud/lib64/             портативный MangoHud (по желанию), 64 бит
└── prefixes/<имя>/                 WINEPREFIX + cask.json
    └── cache/                      кеш шейдеров префикса
        ├── dxvk_shader/            DXVK_SHADER_CACHE_PATH (DXVK 3.0+),
        │                           DXVK_STATE_CACHE_PATH (DXVK < 2.7, D7VK)
        ├── mesa_shader/            MESA_SHADER_CACHE_DIR
        └── vkd3d_shader/           VKD3D_SHADER_CACHE_PATH
```

- Корень можно переопределить переменной `CASK_ROOT`.
- Папки `data` и `prefixes` меняются в «Настройках»; пути внутри корня хранятся относительными.
- Префикс не хранит абсолютных путей (имя = имя папки), Wine хранится по имени раннера, поэтому всё переносится вместе с папкой.
- Переменные кеша выставляются автоматически при каждом запуске wine в префиксе.

## Что умеет

- Создание префикса (win64/win32, выбор Wine), удаление.
- Смена Wine у существующего префикса (`wineboot -u`).
- D7VK, DXVK, VKD3D-Proton выбираются как в Bottles: у каждого свой список, по умолчанию «Никакой».
  Выбор версии применяет её (копирование DLL в `system32`/`syswow64`, бэкап оригиналов `*.cask-bak`,
  DLL overrides через `regedit`), выбор «Никакой» снимает компонент и возвращает оригиналы.
- Запуск программ. Инструменты: Проводник (`explorer`), Панель управления (`control`), winecfg,
  regedit, uninstaller, taskmgr.
- Список префиксов слева: можно перетаскивать вверх и вниз, порядок запоминается
  (`prefixes/.cask-order.json`, переносится вместе с префиксами).
- Папки профиля Wine (Desktop, Documents, Downloads, Music, Pictures, Videos...) не ссылаются на
  домашнюю папку: после создания префикса и после смены Wine ссылки заменяются пустыми
  обычными папками. Ваши файлы не трогаются.
  Диск `z:` (корень системы) остаётся как есть.
- Wine префикса меняется выбором в списке (как компоненты): выбранная сборка сразу применяется
  (`wineboot -u`).
- «Остановить все процессы»: `wineserver -k` для префикса, затем SIGKILL всем, у кого в окружении
  `WINEPREFIX` этого префикса (в том числе запущенным из терминала). Тем же пользуется удаление префикса.
- Winetricks (Инструменты → Winetricks…): список verbs с поиском и категориями, отметка установленных,
  установка выбранных с выводом в окне и кнопкой отмены (отмена останавливает и процессы Wine).
  Verbs `dxvk*`, `vkd3d*`, `d9vk` скрыты: этим занимаются «Компоненты». Скрипт Cask не скачивает:
  положите его в `data/winetricks/winetricks` или установите пакет `winetricks`. Скачанные winetricks
  файлы кешируются в `data/winetricks/cache` (`W_CACHE`). Нужны системные `cabextract`, `unzip`, `wget`
  или `curl`, как для любого winetricks.

### Настройки запуска (у каждого префикса свои, хранятся в `cask.json`)

| Настройка | Что делает |
|---|---|
| Язык Wine | Пусто = язык системы, иначе значение для `LANG` и `LC_ALL` (например `ru_RU.UTF-8`; можно ввести своё). |
| GameMode | Программы, запущенные через «Запустить программу…», стартуют через `gamemoderun` |
| MangoHud | То же через `mangohud` (системный пакет или портативный, см. ниже). Настройка оверлея: переменная `MANGOHUD_CONFIG` в своих переменных, например `fps,cpu_temp`. Если есть `data/mangohud/lib64/`, используется он, иначе системный `mangohud`. Вместе с GameMode: `gamemoderun mangohud wine ...` |
| Переменные окружения | Любые свои переменные (например `DXVK_HUD=fps`). **Не заменяют переменные, которыми управляет Cask**: `WINEPREFIX`, `WINEARCH`, `PATH`, `LANG`, `LC_ALL`, `DXVK_STATE_CACHE_PATH`, `DXVK_SHADER_CACHE_PATH`, `MESA_SHADER_CACHE_DIR`, `VKD3D_SHADER_CACHE_PATH`. Такие имена редактор не принимает. Остальные, включая `WINEDEBUG`, применяются |

**Портативный MangoHud (только 64 бит).** Из релиза с GitHub (`MangoHud-<версия>.tar.gz`) скопируйте
папку `MangoHud/pkg/usr/lib/mangohud/lib64/` как `Cask/data/mangohud/lib64/` (нужны `libMangoHud.so`,
`libMangoHud_shim.so`, `libMangoHud_opengl.so`). Cask сам задаёт `MANGOHUD=1`, `LD_PRELOAD` (OpenGL) и слой
Vulkan (манифест с путём к вашей копии создаётся в `data/mangohud/layer/`); скрипт `mangohud` не нужен.
В системе должны быть `libxkbcommon` и `libwayland-client`. 32-битные игры не поддерживаются.

Синхронизацию (ntsync и др.) Cask не настраивает: Wine 11 сам использует ntsync, если он доступен.
Нужен другой режим для особой сборки: добавьте нужную переменную сами (например `WINEFSYNC=1`).

Свои переменные и GameMode применяются только к запускам пользователя; внутренние
задачи Cask (`wineboot`, `regedit`) работают без них (язык учитывается).

## Язык интерфейса

Русский и английский. По умолчанию берётся язык системы, меняется в «Настройках» сразу, без перезапуска.
Строки в коде английские, перевод лежит в `resources/lang/<код>.json` (встроен в бинарник).

Свой язык без пересборки: положите `Cask/lang/<код>.json` вида
`{"_name": "Deutsch", "Settings": "Einstellungen", ...}`. Непереведённые строки остаются английскими.
Заготовку со всеми строками выдаёт `python3 scripts/check-translations.py --list`.

## Сборка

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/Cask/cask
```

Нужны Qt 6 (или 5) Widgets и CMake ≥ 3.16. Готовая портативная папка получается в `build/Cask/`.

Проверки (запускаются и в GitHub Actions):

```
python3 scripts/check-translations.py     # все строки переведены, плейсхолдеры совпадают
ctest --test-dir build --output-on-failure   # тесты ядра (нужен Qt Test)
```
