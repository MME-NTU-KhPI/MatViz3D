#-------------------------------------------------
#
# Project created by QtCreator 2013-11-07T16:42:27
#
#-------------------------------------------------

QT       += gifimage

TARGET = creategif
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app


SOURCES += main.cpp
DEFINES += SRCDIR=\\\"$$PWD/\\\"

# 安装到例子里面
target.path = $$[QT_INSTALL_EXAMPLES]/qtgifimage
INSTALLS += target
CONFIG += install_ok
