QT += core gui quick quickcontrols2 testlib
CONFIG += c++17 testcase
TARGET = omonitor_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/backend.h ../src/theme.h ../src/translator.h
SOURCES += omonitor_tests.cpp ../src/backend.cpp ../src/theme.cpp ../src/translator.cpp
