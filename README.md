# Cask

Менеджер Wine-префиксов на C++/Qt (аналог Bottles). Версия 0.1: всё скачивается вручную.

## Портативная раскладка

```
Cask/
├── cask            бинарник
├── cask.ini        настройки (создаётся рядом с бинарником)
├── lib/            зависимости (Qt и др.), RPATH = $ORIGIN/lib
├── data/
│   ├── runners/<имя>/bin/wine      сборки Wine
│   ├── d7vk/<версия>/{x64,x32}/*.dll
│   ├── dxvk/<версия>/{x64,x32}/*.dll
│   └── vkd3d-proton/<версия>/{x64,x86}/*.dll
└── prefixes/<имя>/                 WINEPREFIX + cask.json
    └── cache/                      кеш шейдеров префикса
        ├── dxvk_shader/            DXVK_SHADER_CACHE_PATH (DXVK 3.0+),
        │                           DXVK_STATE_CACHE_PATH (DXVK < 2.7, D7VK)
        ├── mesa_shader/            MESA_SHADER_CACHE_DIR
        └── vkd3d_shader/           VKD3D_SHADER_CACHE_PATH
```

Переменные кеша выставляются автоматически при каждом запуске wine в префиксе
(создание, запуск программ, winecfg и т. д.), так что кеш переносится вместе с префиксом.

- Корень можно переопределить переменной `CASK_ROOT`.
- Папки `data` и `prefixes` меняются в «Настройках»; пути внутри корня хранятся относительными.
- Префикс не хранит абсолютных путей (имя = имя папки), Wine хранится по имени раннера, поэтому всё переносится вместе с папкой.

## Что умеет

- Создание префикса (win64/win32, выбор Wine), удаление.
- Смена Wine у существующего префикса (`wineboot -u`).
- Применение/снятие DXVK, D7VK, VKD3D-Proton: копирование DLL в `system32`/`syswow64`,
  бэкап оригиналов (`*.cask-bak`), DLL overrides через `regedit`.
- Запуск программ, winecfg, regedit, uninstaller, taskmgr.

## Сборка

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/Cask/cask
```

Нужны Qt 6 (или 5) Widgets и CMake ≥ 3.16. Готовая портативная папка получается в `build/Cask/`.
