cmake_minimum_required(VERSION 3.20)

project(sol LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(RAYLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(Vendor/raylib)

target_compile_definitions(raylib PRIVATE
    SUPPORT_FILEFORMAT_BDF=1
)

add_executable(sol
    Src/main.cpp
    Src/Lib/FontParser/Bdf.cpp
    Src/WindowManager.cpp
    Src/Shell.cpp
    Src/Assets.cpp
    Src/Lib/UI/Utils.cpp
    Src/Lib/UI/Input.cpp
    Src/Lib/UI/Widget.cpp
    Src/Lib/UI/LayerStacker.cpp
    Src/Lib/UI/Container.cpp
    Src/Lib/UI/Label.cpp
    Src/Lib/UI/Spacer.cpp
    Src/Lib/UI/Button.cpp
    Src/Lib/UI/TextBox.cpp
    Src/Lib/UI/Utf8.cpp
    Src/Lib/UI/TextArea.cpp
)

target_link_libraries(sol PRIVATE raylib)

option(SOL_SANITIZE "Build sol_tests with ASan+LeakSanitizer" OFF)

add_executable(sol_tests
    Tests/main.cpp
    Tests/FakeInput.cpp
    Tests/TestContainer.cpp
    Tests/TestInputDriven.cpp
    Tests/TestWindowManager.cpp
    Tests/TestLayerStacker.cpp
    Src/Lib/FontParser/Bdf.cpp
    Src/WindowManager.cpp
    Src/Shell.cpp
    Src/Assets.cpp
    Src/Lib/UI/Utils.cpp
    Src/Lib/UI/Input.cpp
    Src/Lib/UI/Widget.cpp
    Src/Lib/UI/LayerStacker.cpp
    Src/Lib/UI/Container.cpp
    Src/Lib/UI/Label.cpp
    Src/Lib/UI/Spacer.cpp
    Src/Lib/UI/Button.cpp
    Src/Lib/UI/TextBox.cpp
    Src/Lib/UI/Utf8.cpp
    Src/Lib/UI/TextArea.cpp
)
target_include_directories(sol_tests PRIVATE Vendor/doctest/doctest)
target_link_libraries(sol_tests PRIVATE raylib)

if(SOL_SANITIZE)
    target_compile_options(sol_tests PRIVATE -fsanitize=address,leak -g)
    target_link_options(sol_tests PRIVATE -fsanitize=address,leak)
endif()

enable_testing()
add_test(NAME ui_tests COMMAND sol_tests)
