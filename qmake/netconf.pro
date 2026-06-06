DESTDIR = $$PWD/../_bin
TEMPLATE = app
CONFIG += c++17
CONFIG -= app_bundle
QT += core gui widgets

SOURCES += \
        ../src/InterfaceConfigDialog.cpp \
        ../src/MainWindow.cpp \
        ../src/Win32NetworkConfig.cpp \
        ../src/main.cpp

LIBS += ole32.lib oleaut32.lib wbemuuid.lib

HEADERS += \
	../src/InterfaceConfigDialog.h \
	../src/MainWindow.h \
	../src/Win32NetworkConfig.h

FORMS += \
	../src/InterfaceConfigDialog.ui \
	../src/MainWindow.ui
