QT += core gui
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

TEMPLATE = app
TARGET = task5.2GUI

SOURCES += src/pwm_gui.cpp

INCLUDEPATH += /usr/include
LIBS += -lpigpio -lrt -lpthread

