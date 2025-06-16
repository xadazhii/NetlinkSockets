QT += core gui widgets
CONFIG += c++17

TARGET = usb-monitor
TEMPLATE = app

SOURCES += \
    main.cpp \
    usbmonitor.cpp \
    usbworker.cpp

HEADERS += \
    usbmonitor.h \
    usbworker.h