# mini_compiler

Учебный мини-компилятор для варианта 7: дополнительная конструкция `do { ... } while (...);`, целевая архитектура x86, генерация LLVM IR и объектного файла `.o`.

Компилятор написан на C++17, лексер сделан через Flex, парсер через GNU Bison. Парсер строит AST, затем отдельная семантическая фаза проверяет области видимости, типы, `return`, `break`/`continue`, после чего LLVM codegen выпускает `.ll` и `.o`.

## Поддерживаемый язык

- `int` как 64-битное целое, LLVM `i64`.
- `float` распознается лексером и парсером, но семантика выдает ошибку `float is not implemented yet`.
- Арифметика: `+`, `-`, `*`, `/`, `%`, унарный `-`.
- Сравнения и логика: `==`, `!=`, `<`, `<=`, `>`, `>=`, `&&`, `||`, `!`.
- `if (...) { ... } else { ... }`, `else` необязателен.
- `do { ... } while (...);`.
- `for (init; cond; step) { ... }`.
- Локальные переменные, присваивание, блочная область видимости.
- `return expression;`, `break;`, `continue;`.
- Комментарии `//` и `/* ... */`.

Точка входа учебной программы:

```c
int compiled_fn(int arg) {
    return arg;
}
```

LLVM-сигнатура:

```llvm
define i64 @compiled_fn(i64 %arg)
```

## Зависимости

Нужны:

- CMake 3.16+
- C++17 compiler, например `clang++` или `g++`
- Flex
- GNU Bison 3.5+
- LLVM development package, желательно LLVM 14+
- C-компилятор для линковки runtime-проверки

Пример для Ubuntu/WSL:

```sh
sudo apt update
sudo apt install -y build-essential cmake flex bison llvm-dev clang
```

Если установлено несколько версий LLVM, можно указать путь к конфигу:

```sh
cmake -S . -B build -DLLVM_DIR=/usr/lib/llvm-17/lib/cmake/llvm
```

## Сборка

```sh
cd mini_compiler
mkdir -p build
cd build
cmake ..
cmake --build .
```

Исполняемый файл: `build/mini-cc`.

## Запуск компилятора

```sh
./mini-cc ../tests/valid/do_while_basic.mc \
  -o do_while_basic.o \
  --emit-ir do_while_basic.ll \
  --target=x86_64-pc-linux-gnu
```

`--emit-ir` необязателен. Если его не передать, компилятор сохранит `.ll` рядом с `.o`, например `do_while_basic.ll`.

Поддерживаемые x86-триплы зависят от установленного LLVM. Основной:

```sh
--target=x86_64-pc-linux-gnu
```

Также можно пробовать 32-битный x86:

```sh
--target=i386-pc-linux-gnu
```

Для `i386` нужна 32-битная системная среда при последующей линковке.

## Проверка символа

```sh
llvm-nm do_while_basic.o | grep compiled_fn
```

или:

```sh
nm do_while_basic.o | grep compiled_fn
```

Ожидается экспортируемый символ `compiled_fn`.

## Линковка с внешним main.c

```sh
cc ../runtime/main.c do_while_basic.o -o run_do_while_basic
./run_do_while_basic
```

Для `tests/valid/do_while_basic.mc` и входа `7` runtime должен напечатать:

```text
10
```

## Примеры тестов

В `tests/valid` лежат корректные программы:

- `do_while_basic.mc`
- `do_while_continue.mc`
- `do_while_break.mc`
- `for_basic.mc`
- `if_else.mc`
- `arithmetic.mc`

В `tests/invalid` лежат программы, которые должны завершаться семантической ошибкой:

- `break_outside_loop.mc`
- `continue_outside_loop.mc`
- `undeclared_variable.mc`
- `redeclared_variable.mc`
- `missing_return.mc`

## Запуск набора тестов

После сборки можно прогнать все валидные и невалидные тесты одной командой:

```sh
cmake --build ~/mini_compiler_build --target mini-check
```

Или напрямую через bash:

```sh
bash tests/run_suite.sh ~/mini_compiler_build/mini-cc ~/mini_compiler_build/test-output
```

Пример формата вывода:

```text
Mini compiler self-check
target: x86_64-pc-linux-gnu

[valid inputs: must compile]
  OK   arithmetic               -> object + llvm-ir
  OK   do_while_basic           -> object + llvm-ir

[invalid inputs: must be rejected]
  OK   break_outside_loop       -> rejected (semantic error)

summary: valid 6/6, invalid 5/5
result: PASSED
```

## GitHub Codespaces

Проект можно открыть в GitHub Codespaces. В репозитории есть `.devcontainer/`, который устанавливает CMake, Flex, Bison, LLVM и Clang, а затем автоматически собирает `mini-cc`.

Запуск:

1. GitHub repository -> `Code` -> `Codespaces`.
2. `Create codespace on main`.
3. После открытия терминала:

```sh
cmake --build build --target mini-check
```

Подробная инструкция: `CODESPACES.md`.
