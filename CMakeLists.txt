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
    src/lib/ui/Widget.cpp
    src/lib/ui/Container.cpp
    src/lib/ui/Label.cpp
    src/lib/ui/Button.cpp
    src/lib/ui/TextBox.cpp
)

target_link_libraries(rocketship PRIVATE raylib)