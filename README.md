# Система бронирования переговорных комнат

Система управления бронированием переговорных комнат с поддержкой расширенных сценариов:
права доступа, повторяющееся бронирование, атомарные операции с возможностью отмены/возврата,
политики разрешения конфликтов, уведомления и корректная работа при параллельных запросах.

## Особенности
- **Паттерны проектирования**: Command, Strategy, Repository, Memento, Adapter
- **Стратегии конфликтов**: Reject, AutoBump, Preempt, Quorum
- **Многопоточность**: безопасные операции с мьютексами
- **Хранение данных**: in-memory и JSON файловое хранилище
- **Отмена/повтор**: история операций (до 300 команд)
- **Тестирование**: 15 юнит-тестов (Google Test)

## Команды приложения
```
login <id> <name> <role:Admin|Manager|User>  -- аутентификация пользователя
create <room> <hours> <title> <description>   -- создание бронирования
list <room>                                    -- список бронирований комнаты
cancel <id>                                    -- отмена бронирования
undo                                           -- отмена последней операции
redo                                           -- повтор отмененной операции
exit                                           -- выход
```

## Быстрый старт

### Сборка
```bash
cmake -S . -B build
cmake --build build
```

### Тестирование
```bash
cd build && ctest --output-on-failure
```

### Запуск приложения
```bash
./build/booking_app
```

## Структура проекта
```
├── CMakeLists.txt          # Конфигурация сборки
├── .clang-format          # Стиль кода Яндекс
├── README.md              # Документация
├── INSTALL.md             # Инструкция по сборке
├── inc/                   # Заголовочные файлы
│   ├── BookingManager.hpp
│   ├── calendar.hpp
│   ├── commands.hpp
│   ├── FileJsonStorage.hpp
│   ├── models.hpp
│   ├── storage.hpp
│   └── strategies.hpp
├── src/                   # Исходный код
│   ├── BookingManager.cpp
│   ├── FileJsonStorage.cpp
│   └── main.cpp
└── tests/                 # Юнит-тесты
    └── booking_tests.cpp
```
