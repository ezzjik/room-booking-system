cmake_minimum_required(VERSION 3.16)
project(BookingSystem LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Простой способ - используем nlohmann_json как header-only библиотеку
# Скачиваем json.hpp если его нет
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/external/json/single_include/nlohmann/json.hpp")
    message(STATUS "Загружаем nlohmann_json...")
    file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/external)
    execute_process(
        COMMAND bash -c "cd ${CMAKE_SOURCE_DIR}/external && 
                        (git clone https://github.com/nlohmann/json.git || true) &&
                        cd json && git checkout v3.11.2"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()

# Добавляем путь к заголовкам nlohmann_json
include_directories(
    ${CMAKE_SOURCE_DIR}/inc
    ${CMAKE_SOURCE_DIR}/external/json/single_include
)

file(GLOB_RECURSE SOURCES ${CMAKE_SOURCE_DIR}/src/*.cpp)

set(MAIN_FILE ${CMAKE_SOURCE_DIR}/src/main.cpp)
list(REMOVE_ITEM SOURCES ${MAIN_FILE})

add_library(booking_core ${SOURCES})
set_target_properties(booking_core PROPERTIES
    OUTPUT_NAME booking_core
)

add_executable(booking_app ${MAIN_FILE})
target_link_libraries(booking_app booking_core)

# Ищем GTest
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

enable_testing()

file(GLOB_RECURSE TEST_SOURCES ${CMAKE_SOURCE_DIR}/tests/*.cpp)

add_executable(booking_tests ${TEST_SOURCES})
target_link_libraries(booking_tests booking_core GTest::GTest GTest::Main pthread)

add_test(NAME booking_tests COMMAND booking_tests)

# Настройка проверки кодстайла Яндекс
find_program(CLANG_FORMAT_EXECUTABLE NAMES clang-format)

if(CLANG_FORMAT_EXECUTABLE)
    # Проверяем наличие файла .clang-format
    if(EXISTS "${CMAKE_SOURCE_DIR}/.clang-format")
        message(STATUS "Найден файл .clang-format для Яндекс стиля")
        
        file(GLOB_RECURSE CLANG_FORMAT_SOURCES
            ${CMAKE_SOURCE_DIR}/inc/*.hpp
            ${CMAKE_SOURCE_DIR}/inc/*.h
            ${CMAKE_SOURCE_DIR}/src/*.cpp
            ${CMAKE_SOURCE_DIR}/tests/*.cpp
        )

        if(CLANG_FORMAT_SOURCES)
            # Цель для проверки стиля
            add_custom_target(
                clang_format_check
                COMMAND ${CLANG_FORMAT_EXECUTABLE} --dry-run -Werror --style=file ${CLANG_FORMAT_SOURCES}
                COMMENT "Проверка стиля Яндекс с помощью clang-format"
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            )
            
            # Цель для автоматического форматирования
            add_custom_target(
                clang_format_fix
                COMMAND ${CLANG_FORMAT_EXECUTABLE} -i --style=file ${CLANG_FORMAT_SOURCES}
                COMMENT "Автоматическое форматирование кода в стиль Яндекс"
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            )
            
            # Тест для проверки стиля
            add_test(
                NAME clang_format_test
                COMMAND ${CLANG_FORMAT_EXECUTABLE} --dry-run -Werror --style=file ${CLANG_FORMAT_SOURCES}
            )
            
            message(STATUS "clang-format: настроена проверка стиля Яндекс для ${CLANG_FORMAT_SOURCES} файлов")
        else()
            message(WARNING "clang-format: не найдены исходные файлы для проверки стиля")
        endif()
    else()
        message(WARNING "Файл .clang-format не найден. Загрузите конфиг Яндекс стиля.")
    endif()
else()
    message(WARNING "clang-format не найден. Установите: sudo pacman -S clang")
endif()
