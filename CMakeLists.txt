cmake_minimum_required(VERSION 3.20)

project(rocketship LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(RAYLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(external/raylib)

target_compile_definitions(raylib PRIVATE
    SUPPORT_FILEFORMAT_BDF=1
)


add_executable(rocketship
    src/main.cpp
    src/lib/font-parser/bdf.cpp
    src/window_manager.cpp
    src/assets.cpp
    src/lib/ui/Utils.cpp
    src/lib/ui/Input.cpp
    src/lib/ui/Widget.cpp
    src/lib/ui/Container.cpp
    src/lib/ui/Label.cpp
    src/lib/ui/Spacer.cpp
    src/lib/ui/Button.cpp
    src/lib/ui/TextBox.cpp
    src/lib/ui/Utf8.cpp
    src/lib/ui/TextArea.cpp
)

target_link_libraries(rocketship PRIVATE raylib)

option(ROCKETSHIP_SANITIZE "Build rocketship_tests with ASan+LeakSanitizer" OFF)

add_executable(rocketship_tests
    tests/main.cpp
    tests/FakeInput.cpp
    tests/test_container.cpp
    tests/test_input_driven.cpp
    src/lib/font-parser/bdf.cpp
    src/assets.cpp
    src/lib/ui/Utils.cpp
    src/lib/ui/Input.cpp
    src/lib/ui/Widget.cpp
    src/lib/ui/Container.cpp
    src/lib/ui/Label.cpp
    src/lib/ui/Spacer.cpp
    src/lib/ui/Button.cpp
    src/lib/ui/TextBox.cpp
    src/lib/ui/Utf8.cpp
    src/lib/ui/TextArea.cpp
)
target_include_directories(rocketship_tests PRIVATE external/doctest/doctest)
target_link_libraries(rocketship_tests PRIVATE raylib)
if(ROCKETSHIP_SANITIZE)
    target_compile_options(rocketship_tests PRIVATE -fsanitize=address,leak -g)
    target_link_options(rocketship_tests PRIVATE -fsanitize=address,leak)
endif()

enable_testing()
add_test(NAME ui_tests COMMAND rocketship_tests)
