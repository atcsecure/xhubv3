#-------------------------------------------------
#
# Project created by QtCreator 2014-11-20T11:31:03
#
#-------------------------------------------------

QT       += core gui concurrent

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = blocknet-dht
TEMPLATE = app

DEFINES += \
    _CRT_SECURE_NO_WARNINGS

INCLUDEPATH += \
    d:/work/openssl/include

SOURCES += \
    src/main.cpp\
    src/statdialog.cpp \
    src/blocknetapp.cpp \
    src/dht/dht.cpp \
    src/util.cpp \
    src/logger.cpp

HEADERS += \
    src/statdialog.h \
    src/dht/dht.h \
    src/blocknetapp.h \
    src/util.h \
    src/logger.h \
    src/uint256.h

LIBS += \
    -lws2_32 \
    -Ld:/work/openssl/lib \
    -llibeay32 \
    -lssleay32
