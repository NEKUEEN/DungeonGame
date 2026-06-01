cmake_minimum_required(VERSION 3.16)
project(DungeonGame)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SFML_DIR "C:/SFML-3.1.0/lib/cmake/SFML")
find_package(SFML 3.1 REQUIRED COMPONENTS Graphics Window System)

set(SOURCES
    src/main.cpp
    src/Game.cpp
    src/TileMap.cpp
    src/Player.cpp
    src/Enemy.cpp
    src/FogOfWar.cpp
    src/Item.cpp
    src/Inventory.cpp
    src/Equipment.cpp
)

add_executable(DungeonGame ${SOURCES})
target_include_directories(DungeonGame PRIVATE src src/lib)
target_link_libraries(DungeonGame PRIVATE SFML::Graphics SFML::Window SFML::System)

add_custom_command(TARGET DungeonGame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "C:/SFML-3.1.0/bin/sfml-graphics-3.dll"
        "C:/SFML-3.1.0/bin/sfml-window-3.dll"
        "C:/SFML-3.1.0/bin/sfml-system-3.dll"
        "C:/SFML-3.1.0/bin/sfml-graphics-d-3.dll"
        "C:/SFML-3.1.0/bin/sfml-window-d-3.dll"
        "C:/SFML-3.1.0/bin/sfml-system-d-3.dll"
        $<TARGET_FILE_DIR:DungeonGame>
)

add_custom_command(TARGET DungeonGame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:DungeonGame>/assets"
)
