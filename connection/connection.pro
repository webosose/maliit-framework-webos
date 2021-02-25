include(../config.pri)

TOP_DIR = ..

VERSION = $$MALIIT_ABI_VERSION
TEMPLATE = lib
TARGET = $$TOP_DIR/lib/$$MALIIT_CONNECTION_LIB

include($$TOP_DIR/common/libmaliit-common.pri)

CONFIG += staticlib

# Interface classes
PUBLIC_HEADERS += \
    connectionfactory.h \
    minputcontextconnection.h \

PUBLIC_SOURCES += \
    connectionfactory.cpp \
    minputcontextconnection.cpp \

wayland {
    QT += gui-private
    PUBLIC_SOURCES += \
        minputcontextwestonimprotocolconnection.cpp
    PUBLIC_HEADERS += \
        minputcontextwestonimprotocolconnection.h
}

enable-libim:contains(WEBOS_TARGET_MACHINE_IMPL, hardware) {
    DEFINES += HAS_LIBIM
    CONFIG += link_pkgconfig
    PKGCONFIG += libim
}

HEADERS += \
    $$PUBLIC_HEADERS \
    $$PRIVATE_HEADERS \

SOURCES += \
    $$PUBLIC_SOURCES \
    $$PRIVATE_SOURCES \

OTHER_FILES += libmaliit-connection.pri
