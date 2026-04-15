cmake_minimum_required(VERSION 3.16)

if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")
endif()

project(trusttunnel-tray VERSION 1.4.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets SvgWidgets)

if(LINUX)
    find_package(Qt6 REQUIRED COMPONENTS DBus)
endif()

add_executable(trusttunnel-tray
    src/main.cpp
    src/TrayAgent.cpp
)

target_link_libraries(trusttunnel-tray PRIVATE Qt6::Widgets Qt6::SvgWidgets)

if(LINUX)
    target_link_libraries(trusttunnel-tray PRIVATE Qt6::DBus)
endif()

if(APPLE)
    set_target_properties(trusttunnel-tray PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.trusttunnel.tray"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/Info.plist"
    )
endif()
