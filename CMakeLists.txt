cmake_minimum_required(VERSION 3.16)
project(trusttunnel-tray LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(trusttunnel-tray
    src/main.cpp
    src/TrayAgent.cpp
)

target_link_libraries(trusttunnel-tray PRIVATE Qt6::Widgets)
