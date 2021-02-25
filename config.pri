# Enable Qt message log context
DEFINES += QT_MESSAGELOGCONTEXT

# For maliit framework header
MALIIT_FRAMEWORK_HEADER = maliit/framework

include(defines.pri)

# Linker optimization for release build
QMAKE_LFLAGS_RELEASE+=-Wl,--as-needed
# Compiler warnings are error if the build type is debug
QMAKE_CXXFLAGS_DEBUG+=-Werror -O0
QMAKE_CFLAGS_DEBUG+=-Werror -O0

OBJECTS_DIR = .obj
MOC_DIR = .moc

isEmpty(MALIIT_VERSION) {
    MALIIT_VERSION=$$system(cat $$PWD/VERSION)
}

MALIIT_ABI_VERSION = $$MALIIT_VERSION

isEmpty(PREFIX) {
    PREFIX = /usr
}
isEmpty(BINDIR) {
    BINDIR = $$PREFIX/bin
}
isEmpty(SBINDIR) {
    SBINDIR = $$PREFIX/sbin
}
isEmpty(LIBDIR) {
    LIBDIR = $$PREFIX/lib
}
isEmpty(INCLUDEDIR) {
    INCLUDEDIR = $$PREFIX/include
}
DATADIR = $$PREFIX/share

isEmpty(MALIIT_DATA_DIR) {
    MALIIT_DATA_DIR = $$DATADIR/maliit
}

isEmpty(MALIIT_PLUGINS_DIR) {
    MALIIT_PLUGINS_DIR = $$LIBDIR/$$MALIIT_PLUGINS
}
DEFINES += MALIIT_PLUGINS_DIR=\\\"$$MALIIT_PLUGINS_DIR\\\"

isEmpty(MALIIT_PLUGINS_DATA_DIR) {
    MALIIT_PLUGINS_DATA_DIR = $$DATADIR/$$MALIIT_PLUGINS_DATA
}
DEFINES += MALIIT_PLUGINS_DATA_DIR=\\\"$$MALIIT_PLUGINS_DATA_DIR\\\"

isEmpty(MALIIT_FACTORY_PLUGINS_DIR) {
    MALIIT_FACTORY_PLUGINS_DIR = $$MALIIT_PLUGINS_DIR/factories
}
DEFINES += MALIIT_FACTORY_PLUGINS_DIR=\\\"$$MALIIT_FACTORY_PLUGINS_DIR\\\"

isEmpty(MALIIT_ENABLE_MULTITOUCH) {

    MALIIT_ENABLE_MULTITOUCH=true
}

MALIIT_EXTENSIONS_DIR = $$DATADIR/$$MALIIT_ATTRIBUTE_EXTENSIONS/
DEFINES += MALIIT_EXTENSIONS_DIR=\\\"$$MALIIT_EXTENSIONS_DIR\\\"

isEmpty(MALIIT_DEFAULT_HW_PLUGIN) {
    MALIIT_DEFAULT_HW_PLUGIN = libmaliit-keyboard-plugin.so
}

isEmpty(MALIIT_DEFAULT_PLUGIN) {
    MALIIT_DEFAULT_PLUGIN = libmaliit-keyboard-plugin.so
}

DEFINES += MALIIT_CONFIG_ROOT=\\\"$$MALIIT_CONFIG_ROOT\\\"

DEFINES += MALIIT_FRAMEWORK_USE_INTERNAL_API

# Do not define keywords signals, slots, emit and foreach because of
# conflicts with 3rd party libraries.
CONFIG += no_keywords

unix {
    MALIIT_STATIC_PREFIX=lib
    MALIIT_STATIC_SUFFIX=.a
    MALIIT_DYNAMIC_PREFIX=lib
    MALIIT_DYNAMIC_SUFFIX=.so
    MALIIT_ABI_VERSION_MAJOR=
}

defineReplace(maliitStaticLib) {
    return($${MALIIT_STATIC_PREFIX}$${1}$${MALIIT_STATIC_SUFFIX})
}

defineReplace(maliitDynamicLib) {
    return($${MALIIT_DYNAMIC_PREFIX}$${1}$${MALIIT_DYNAMIC_SUFFIX})
}

wayland {
    DEFINES += HAVE_WAYLAND
}

MALIIT_INSTALL_PRF = $$[QMAKE_MKSPECS]/features
local-install {
    MALIIT_INSTALL_PRF = $$replace(MALIIT_INSTALL_PRF, $$[QT_INSTALL_PREFIX], $$PREFIX)
}

defineTest(outputFile) {
    out = $$OUT_PWD/$$1
    in = $$PWD/$${1}.in

    !exists($$in) {
        error($$in does not exist!)
        return(false)
    }

    MALIIT_IN_DIR = $$PWD
    MALIIT_OUT_DIR = $$OUT_PWD

    variables = MALIIT_FRAMEWORK_FEATURE \
                PREFIX \
                BINDIR \
                INCLUDEDIR \
                LIBDIR \
                MALIIT_PLUGINS_DIR \
                MALIIT_DATA_DIR \
                MALIIT_PLUGINS_DATA_DIR \
                MALIIT_FACTORY_PLUGINS_DIR \
                MALIIT_VERSION \
                MALIIT_ENABLE_MULTITOUCH \
                MALIIT_DEFAULT_HW_PLUGIN \
                MALIIT_DEFAULT_PLUGIN \
                MALIIT_PLUGINS_LIB \
                MALIIT_PLUGINS_HEADER \
                MALIIT_IN_DIR \
                MALIIT_OUT_DIR \
                MALIIT_PACKAGENAME \
                MALIIT_FRAMEWORK_HEADER \
                MALIIT_SERVER_ARGUMENTS \
                MALIIT_CONNECTION_LIB \
                MALIIT_SERVER_HEADER \
                MALIIT_ABI_VERSION_MAJOR \

    command = "sed"
    for(var, variables) {
       command += "-e \"s;@$$var@;$$eval($$var);g\""
    }
    command += $$in > $$out

    system(mkdir -p $$dirname(out))
    system($$command)
    system(chmod --reference=$$in $$out)

    QMAKE_DISTCLEAN += $$1

    export(QMAKE_DISTCLEAN)

    return(true)
}

defineTest(outputFiles) {
    files = $$ARGS

    for(file, files) {
        !outputFile($$file):return(false)
    }

    return(true)
}
