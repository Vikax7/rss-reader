cmake_minimum_required(VERSION 3.20)
project(rss_reader CXX)

# стандарт C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# строгие флаги компилятора 
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Werror)
endif()

# 1. Библиотека ЯДРА 
add_library(rss_core
    core/article.hpp
    core/article.cpp
    core/feed.cpp 
    core/feed.hpp 
)

# где искать заголовочные файлы ядра
target_include_directories(rss_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# 2. Исполняемый файл TUI-приложения
add_executable(rss_tui
    tui/main.cpp
)

# Подключаем ядро к исполняемому файлу
target_link_libraries(rss_tui PRIVATE rss_core)

# Подключение doctest через FetchContent
include(FetchContent)
FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG        v2.4.11
)
FetchContent_MakeAvailable(doctest)

# Цель для тестов ядра
add_executable(unit_tests
    tests/core_test.cpp
)

# Линкуем ядро и doctest
target_link_libraries(unit_tests PRIVATE rss_core doctest::doctest)

# Включаем поддержку ctest
enable_testing()
add_test(NAME CoreTests COMMAND unit_tests)