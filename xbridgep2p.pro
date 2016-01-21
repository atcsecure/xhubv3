#-------------------------------------------------
#
# Project created by QtCreator 2014-11-20T11:31:03
#
#-------------------------------------------------

QT       += core gui
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

#CONFIG   -= qt
#CONFIG   += console
#CONFIG   -= app_bundle
CONFIG   += app
CONFIG   += static

#-------------------------------------------------
TEMPLATE = app
CONFIG(release, debug|release) {
    TARGET = ../bin/xbridgep2p
}
else:CONFIG(debug, debug|release){
    TARGET = ../bin/xbridgep2p-debug
}


#-------------------------------------------------
!include($$PWD/config.pri) {
    error(Failed to include config.pri)
}

#-------------------------------------------------
withbreakpad {

message("msvc build, breakpad enabled")

DEFINES += BREAKPAD_ENABLED

INCLUDEPATH += \
    $$PWD/src \
    $$PWD/src/3rdparty/breakpad/src

QMAKE_CFLAGS_RELEASE += -Zi
QMAKE_CXXFLAGS_RELEASE += -Zi
QMAKE_LFLAGS_RELEASE += /DEBUG /INCREMENTAL:NO

}

#-------------------------------------------------
win32 {

DEFINES += \
    _CRT_SECURE_NO_WARNINGS \
    _SCL_SECURE_NO_WARNINGS

DEFINES +=\
    _WIN32_WINNT=0x0600

} #win32

#-------------------------------------------------
SOURCES += \
    src/main.cpp\
    src/dht/dht.cpp \
    src/util/util.cpp \
    src/util/logger.cpp \
    src/xbridgeapp.cpp \
    src/xbridge.cpp \
    src/xbridgesession.cpp \
    src/xbridgeexchange.cpp \
    src/xbridgetransaction.cpp \
    src/util/settings.cpp \
    src/xbridgetransactionmember.cpp \
    src/ui/xbridgetransactionsview.cpp \
    src/ui/xbridgetransactionsmodel.cpp \
    src/ui/xbridgetransactiondialog.cpp \
    src/ui/xbridgeaddressbookview.cpp \
    src/ui/xbridgeaddressbookmodel.cpp \
    src/bitcoinrpc.cpp \
    src/json/json_spirit_reader.cpp \
    src/json/json_spirit_value.cpp \
    src/json/json_spirit_writer.cpp

#-------------------------------------------------
HEADERS += \
    src/dht/dht.h \
    src/util/util.h \
    src/util/logger.h \
    src/util/uint256.h \
    src/xbridgeapp.h \
    src/xbridge.h \
    src/xbridgesession.h \
    src/xbridgepacket.h \
    src/xbridgeexchange.h \
    src/xbridgetransaction.h \
    src/util/settings.h \
    src/xbridgetransactionmember.h \
    src/version.h \
    src/config.h \
    src/ui/xbridgetransactionsview.h \
    src/ui/xbridgetransactionsmodel.h \
    src/xbridgetransactiondescr.h \
    src/ui/xbridgetransactiondialog.h \
    src/ui/xbridgeaddressbookview.h \
    src/ui/xbridgeaddressbookmodel.h \
    src/bitcoinrpc.h \
    src/json/json_spirit.h \
    src/json/json_spirit_error_position.h \
    src/json/json_spirit_reader.h \
    src/json/json_spirit_reader_template.h \
    src/json/json_spirit_stream_reader.h \
    src/json/json_spirit_utils.h \
    src/json/json_spirit_value.h \
    src/json/json_spirit_writer.h \
    src/json/json_spirit_writer_template.h \
    src/bignum.h \
    src/uiconnector.h

#-------------------------------------------------
DISTFILES += \
    xbridgep2p.exe.conf \
    config.orig.pri

#-------------------------------------------------
withbreakpad {

SOURCES += \
    src/ExceptionHandler.cpp \
    src/sender.cpp

HEADERS += \
    src/ExceptionHandler.h

CONFIG(release, debug|release) {
LIBS += \
    -L$$PWD/lib/win/breakpad/win/release
}
else:CONFIG(debug, debug|release){
LIBS += \
    -L$$PWD/lib/win/breakpad/win/debug
}

LIBS += \
    -lcommon \
    -lexception_handler \
    -lcrash_generation_client \
    -lcrash_generation_server \
    -lcrash_report_sender

} #withbreakpad

RESOURCES += \
    resource.qrc
