TARGET = apicmanager_gui
QT += widgets
CONFIG += c++17
LIBS += -lexiv2 -lsqlite3
INCLUDEPATH += /usr/include/exiv2
TEMPLATE = app
SOURCES += apicmanager_gui.cpp
