DESTDIR = $$PWD/../
TEMPLATE = app
CONFIG += console c++17
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
        ../src/Win32NetworkConfig.cpp \
        ../src/main.cpp

LIBS += ole32.lib oleaut32.lib wbemuuid.lib

HEADERS += \
	../src/Win32NetworkConfig.h
