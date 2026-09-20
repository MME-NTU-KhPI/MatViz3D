QT += core gui widgets quick testlib sql
CONFIG += console c++17 testcase
CONFIG -= app_bundle

gcc_ver = $$system($$QMAKE_CXX -dumpversion)
greaterThan(gcc_ver, 15): QMAKE_CXXFLAGS += -Wno-sfinae-incomplete

QMAKE_CXXFLAGS += -fopenmp
LIBS += -fopenmp

INCLUDEPATH += ..
DEPENDPATH += ..

# HDF5
unix:!macos {
    CONFIG += link_pkgconfig
    PKGCONFIG += hdf5
    exists(/usr/include/hdf5/serial) {
        HDF5_INCLUDEPATH = "/usr/include/hdf5/serial"
        HDF5_LIBPATH = "/usr/lib/x86_64-linux-gnu"
        LIBS += -L$${HDF5_LIBPATH} -lhdf5_serial
        INCLUDEPATH += $$HDF5_INCLUDEPATH
    }
}
win32 {
    HDF5_DIR = $$(HDF5_DIR)
    isEmpty(HDF5_DIR) {
        HDF5_ROOT = "C:\Program Files\HDF_Group\HDF5"
        HCMD = dir /B /AD \"$$HDF5_ROOT\" | findstr \"^[0-9]\"
        HDF5_VERSION = $$system($$HCMD)
        HDF5_DIR = "$$HDF5_ROOT/$$HDF5_VERSION"
    }
    HDF5_LIBPATH = "$$HDF5_DIR/lib"
    HDF5_INCLUDEPATH = "$$HDF5_DIR/include"
    INCLUDEPATH += $$HDF5_INCLUDEPATH
    LIBS += -L$$HDF5_LIBPATH -lhdf5
}

# Production algorithm and math sources without GUI/QML entry point
SOURCES += \
    ../parameters.cpp \
    ../parent_algorithm.cpp \
    ../polycrystall.cpp \
    ../probability_algorithm.cpp \
    ../algorithmplugin.cpp \
    ../algorithmfactory.cpp \
    ../grain_analyzer.cpp \
    ../loadstepmanager.cpp \
    ../texturelibrary.cpp \
    ../commandline_parser.cpp \
    ../config_source.cpp \
    ../dbmanager.cpp \
    ../hillcriterion.cpp \
    ../hdf5wrapper.cpp \
    ../ansyswrapper.cpp \
    ../consolelogger.cpp \
    ../voronoi.cpp \
    ../composite.cpp \
    ../dlca.cpp

HEADERS += \
    ../parameters.h \
    ../parent_algorithm.h \
    ../polycrystall.h \
    ../probability_algorithm.h \
    ../algorithmplugin.h \
    ../algorithmfactory.h \
    ../grain_analyzer.h \
    ../loadstepmanager.h \
    ../texturelibrary.h \
    ../commandline_parser.h \
    ../config_source.h \
    ../dbmanager.h \
    ../hillcriterion.h \
    ../hdf5wrapper.h \
    ../ansyswrapper.h \
    ../consolelogger.h \
    ../voronoi.h \
    ../composite.h \
    ../dlca.h \
    ../texturemath.hpp \
    ../svgexporter.hpp \
    ../cpuinfo.hpp

# Test sources
SOURCES += \
    main.cpp \
    headless_gl_stub.cpp \
    test_polycrystall_seeding.cpp \
    test_polycrystall_growth.cpp \
    test_wave_nucleation.cpp \
    test_texturemath.cpp \
    test_hillcriterion.cpp \
    test_svgexporter.cpp \
    test_commandline_parser.cpp

HEADERS += \
    test_polycrystall_seeding.h \
    test_polycrystall_growth.h \
    test_wave_nucleation.h \
    test_texturemath.h \
    test_hillcriterion.h \
    test_svgexporter.h \
    test_commandline_parser.h

TARGET = tests
