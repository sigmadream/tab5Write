cmake_minimum_required(VERSION 3.22)
project(tabwrite_tests)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../main
)

add_executable(test_ime
    test_text_input_composer.cpp
    ../main/text_input_composer.cpp
)

add_executable(test_editor_core
    test_editor_core.cpp
    ../main/piece_table.cpp
    ../main/undo_stack.cpp
    ../main/editor_core.cpp
)
