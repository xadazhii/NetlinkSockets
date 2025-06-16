cmake_minimum_required(VERSION 3.10)
project(USBdevice)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 COMPONENTS Widgets REQUIRED)

set(CMAKE_AUTOMOC ON)

add_executable(USBdevice main.cpp usbworker.cpp
        usbmonitor.cpp)

target_link_libraries(USBdevice Qt5::Widgets)
