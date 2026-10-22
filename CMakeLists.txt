cmake_minimum_required(VERSION 3.20)
project(rss_reader CXX)

# Стандарт C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Строгие флаги компилятора 
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Werror)
endif()

# Ищем системную библиотеку libcurl для сетевых запросов (HttpClient)
find_package(CURL REQUIRED)

# 1. Библиотека ядра и слоя приложения
add_library(rss_core
    core/article.hpp
    core/feed.cpp 
    core/feed.hpp 
    core/atom_parser.hpp
    core/feed_parser_factory.hpp
    core/feed_parser.hpp
    core/rss_parser.hpp
    app/feed_manager.hpp
    app/http_client.hpp
    app/http_response.hpp
    core/html_renderer.hpp
)

# Где искать заголовочные файлы ядра и приложения
target_include_directories(rss_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Подключаем pugixml и curl к ядру
target_link_libraries(rss_core PUBLIC pugixml CURL::libcurl)

# Подключение FTXUI через FetchContent 
include(FetchContent)
FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG        v5.0.0
)
FetchContent_MakeAvailable(ftxui)

# 2. Исполняемый файл TUI-приложения
add_executable(rss_tui
    tui/main.cpp
)

# Подключаем ядро и компоненты FTXUI к исполняемому файлу
target_link_libraries(rss_tui PRIVATE 
    rss_core 
    ftxui::screen 
    ftxui::dom 
    ftxui::component
)

# 3. Подключение doctest через FetchContent
FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG        v2.4.11
)
FetchContent_MakeAvailable(doctest)

# 4. Тесты ядра
add_executable(unit_tests
    tests/core_test.cpp
)
target_link_libraries(unit_tests PRIVATE rss_core doctest::doctest)

# 5. Тесты приложения
add_executable(app_test
    tests/app_test.cpp
)
target_link_libraries(app_test PRIVATE 
    rss_core    
    doctest::doctest
)

# Включаем поддержку ctest
enable_testing()
add_test(NAME CoreTests COMMAND unit_tests)
add_test(NAME AppTests COMMAND app_test)