# GitHub Codespaces

Проект можно запускать без локальной установки WSL, LLVM, Flex и Bison через GitHub Codespaces.

## Как открыть

1. Открой репозиторий на GitHub.
2. Нажми `Code`.
3. Перейди на вкладку `Codespaces`.
4. Нажми `+` или `Create codespace on main`.

GitHub создаст Ubuntu-контейнер по настройкам из `.devcontainer/`.

При первом запуске Codespace автоматически:

1. Установит CMake, Flex, Bison, LLVM, Clang и системные dev-библиотеки.
2. Выполнит CMake configure.
3. Соберёт `mini-cc`.

## Проверить тесты

В терминале Codespace:

```sh
cmake --build build --target mini-check
```

Ожидаемый результат:

```text
Mini compiler self-check
target: x86_64-pc-linux-gnu

[valid inputs: must compile]
  OK   arithmetic               -> object + llvm-ir
  OK   do_while_basic           -> object + llvm-ir
  OK   do_while_break           -> object + llvm-ir
  OK   do_while_continue        -> object + llvm-ir
  OK   for_basic                -> object + llvm-ir
  OK   if_else                  -> object + llvm-ir

[invalid inputs: must be rejected]
  OK   break_outside_loop       -> rejected (semantic error)
  OK   continue_outside_loop    -> rejected (semantic error)
  OK   missing_return           -> rejected (semantic error)
  OK   redeclared_variable      -> rejected (semantic error)
  OK   undeclared_variable      -> rejected (semantic error)

summary: valid 6/6, invalid 5/5
result: PASSED
```

## Проверить пример вручную

```sh
./build/mini-cc tests/valid/do_while_basic.mc \
  -o build/do_while_basic.o \
  --emit-ir build/do_while_basic.ll \
  --target=x86_64-pc-linux-gnu

cc runtime/main.c build/do_while_basic.o -o build/run_do_while_basic
./build/run_do_while_basic
```

Ожидаемый вывод:

```text
10
```

## Если Codespace уже был создан раньше

Если `.devcontainer` добавлен после создания Codespace, старый Codespace может не увидеть новые зависимости.

Варианты:

- Создать новый Codespace.
- Или в существующем Codespace выполнить `Codespaces: Rebuild Container` через Command Palette.
