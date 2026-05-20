# GitHub: загрузка проекта и запуск проверки

Эта инструкция показывает, как загрузить проект `mini_compiler` на GitHub и как запускать сборку через GitHub Actions.

## 1. Подготовить Git в WSL

Если Git ещё не установлен:

```sh
sudo apt update
sudo apt install -y git
```

Один раз настрой имя и email:

```sh
git config --global user.name "Your Name"
git config --global user.email "your_email@example.com"
```

## 2. Создать репозиторий на GitHub

1. Открой https://github.com/new
2. Repository name: `mini_compiler`
3. Выбери `Public` или `Private`
4. Не добавляй README, `.gitignore` и license через сайт
5. Нажми `Create repository`

GitHub покажет URL вида:

```text
https://github.com/USERNAME/mini_compiler.git
```

## 3. Загрузить проект

В WSL:

```sh
cd /mnt/c/Users/Пудж/Documents/компилятор/mini_compiler

git init
git add .
git status
git commit -m "Initial mini compiler project"
git branch -M main
git remote add origin https://github.com/USERNAME/mini_compiler.git
git push -u origin main
```

Замени `USERNAME` на свой GitHub-логин.

Если GitHub попросит пароль при `git push`, используй не пароль от аккаунта, а Personal Access Token.

## 4. Проверить сборку на GitHub

После `git push` открой репозиторий на GitHub и перейди во вкладку `Actions`.

Там должен появиться workflow:

```text
Mini compiler CI
```

Он автоматически делает:

1. Скачивает репозиторий.
2. Устанавливает CMake, Flex, Bison, LLVM и Clang.
3. Собирает `mini-cc`.
4. Запускает валидные и невалидные тесты.
5. Компилирует `do_while_basic.mc` в `.o`.
6. Линкует `.o` с `runtime/main.c`.
7. Проверяет, что программа выводит `10`.

Успешный результат выглядит так:

```text
summary: valid 6/6, invalid 5/5
result: PASSED
runtime output: 10
```

## 5. Запустить workflow вручную

На GitHub:

1. Открой вкладку `Actions`.
2. Выбери `Mini compiler CI`.
3. Нажми `Run workflow`.
4. Выбери ветку `main`.
5. Нажми зелёную кнопку `Run workflow`.

## 6. Скачать артефакты сборки

После завершения workflow открой конкретный запуск в `Actions`.

Внизу страницы будет блок `Artifacts`. Там можно скачать:

```text
mini-compiler-ci-artifacts
```

Внутри будут тестовые логи и пример сгенерированного LLVM IR/object-файла.

## 7. Запуск после клонирования на другом компьютере

```sh
git clone https://github.com/USERNAME/mini_compiler.git
cd mini_compiler

cmake -S . -B build
cmake --build build -j$(nproc)
cmake --build build --target mini-check
```

Проверка runtime:

```sh
./build/mini-cc tests/valid/do_while_basic.mc \
  -o build/do_while_basic.o \
  --emit-ir build/do_while_basic.ll

cc runtime/main.c build/do_while_basic.o -o build/run_do_while_basic
./build/run_do_while_basic
```

Ожидаемый вывод:

```text
10
```

## GitHub Codespaces

Проект содержит папку `.devcontainer/`. Поэтому на другом компьютере можно запускать проект прямо в браузере через GitHub Codespaces, без ручной установки LLVM, Flex, Bison и CMake.

Как запустить:

1. Открой репозиторий на GitHub.
2. Нажми зелёную кнопку `Code`.
3. Перейди на вкладку `Codespaces`.
4. Нажми `Create codespace on main` или кнопку `+`.
5. Дождись, пока Codespace создаст контейнер и выполнит автоматическую сборку.

После открытия терминала в Codespace запусти тесты:

```sh
cmake --build build --target mini-check
```

Проверка отдельного примера:

```sh
./build/mini-cc tests/valid/do_while_basic.mc \
  -o build/do_while_basic.o \
  --emit-ir build/do_while_basic.ll

cc runtime/main.c build/do_while_basic.o -o build/run_do_while_basic
./build/run_do_while_basic
```

Ожидаемый вывод:

```text
10
```

Если Codespace был создан до появления `.devcontainer/`, выбери в VS Code команду `Codespaces: Rebuild Container` или создай новый Codespace.
