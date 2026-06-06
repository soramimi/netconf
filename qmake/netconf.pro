DESTDIR = $$PWD/../
TEMPLATE = app
CONFIG += console c++17
CONFIG -= app_bundle
QT += core gui widgets

SOURCES += \
        ../src/MainWindow.cpp \
        ../src/Win32NetworkConfig.cpp \
        ../src/main.cpp

LIBS += ole32.lib oleaut32.lib wbemuuid.lib

HEADERS += \
	../src/MainWindow.h \
	../src/Win32NetworkConfig.h

FORMS += \
	../src/MainWindow.ui
