# Инструкция по сборке и запуску

## Требования:
- C++20 компилятор (g++ 10+ или clang 12+)
- CMake 3.16+
- Git (для загрузки зависимостей)

## Сборка проекта:

1. Распакуйте архив: `tar -xzf RoomBookingSystem_*.tar.gz`
2. Перейдите в директорию: `cd RoomBookingSystem`
3. Соберите проект:
   ```bash
   cmake -S . -B build
   cmake --build build
   ```

## Запуск тестов:

```bash
cd build && ctest --output-on-failure
```

## Проверка кодстайла:

```bash
cmake --build build --target clang_format_check
# Автоматическое форматирование (если нужно):
cmake --build build --target clang_format_fix
```

## Запуск приложения:

```bash
./build/booking_app
```

## Основные команды приложения:

```
login <id> <name> <role:Admin|Manager|User>  -- аутентификация пользователя
create <room> <hours> <title> <description>   -- создание бронирования
list <room>                                    -- список бронирований комнаты
cancel <id>                                    -- отмена бронирования
undo                                           -- отмена последней операции
redo                                           -- повтор отмененной операции
exit                                           -- выход
```

## Архитектурные особенности:
- Использование паттернов Command, Strategy, Repository
- Поддержка многопоточности и атомарных операций
- Гибкая система разрешения конфликтов
- История операций с возможностью отмены/повтора
