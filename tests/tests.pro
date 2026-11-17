QT += core gui widgets quick testlib sql
CONFIG += console c++17 testcase
CONFIG -= app_bundle

gcc_ver = $$system($$QMAKE_CXX -dumpversion)
greaterThan(gcc_ver, 15): QMAKE_CXXFLAGS += -Wno-sfinae-incomplete

macos {
    # Apple clang has no -fopenmp driver flag; use the preprocessor form and
    # link Homebrew's libomp and HDF5 explicitly.
    QMAKE_CXXFLAGS += -Xpreprocessor -fopenmp
    INCLUDEPATH += /opt/homebrew/opt/libomp/include /opt/homebrew/opt/hdf5/include
    LIBS += -L/opt/homebrew/opt/libomp/lib -lomp -L/opt/homebrew/opt/hdf5/lib -lhdf5 -lhdf5_cpp
    # cellnet.cpp (the layers of celldiffusion.cpp and cellpredictor.cpp): the matrix products go to Accelerate's sgemm, as in the program
    DEFINES += MATVIZ_USE_ACCELERATE
    LIBS += -framework Accelerate
} else {
    QMAKE_CXXFLAGS += -fopenmp
    LIBS += -fopenmp
}

INCLUDEPATH += ..
DEPENDPATH += ..

# TestCellDiffusion and TestCellPredictor read models/cell_diffusion.bin and models/cell_predictor.bin of the source tree
# (not in git; skipped when absent)
DEFINES += "MATVIZ_SOURCE_DIR=\\\"$$clean_path($$PWD/..)\\\""

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
    ../dlca.cpp \
    ../lattice.cpp \
    ../mycelium.cpp \
    ../cellforms.cpp \
    ../printability.cpp

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
    ../lattice.h \
    ../latticefamily.h \
    ../mycelium.h \
    ../cellforms.h \
    ../printability.h \
    ../fft_homog.hpp \
    ../fft_solver_session.hpp \
    ../texturemath.hpp \
    ../svgexporter.hpp \
    ../cpuinfo.hpp \
    ../lowdiscrepancy.hpp

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
    test_commandline_parser.cpp \
    test_lattice.cpp \
    test_ffthomog.cpp \
    test_printability.cpp \
    test_datasetbrowser.cpp \
    ../datasetbrowsercontroller.cpp \
    test_seed_distribution.cpp \
    test_propertysearch.cpp \
    ../propertysearch.cpp \
    ../propertyselectorcontroller.cpp \
    test_celldecoder.cpp \
    ../celldecoder.cpp \
    test_celldiffusion.cpp \
    ../celldiffusion.cpp \
    ../cellnet.cpp \
    test_cellpredictor.cpp \
    ../cellpredictor.cpp \
    test_cellstiffness.cpp \
    ../cellstiffness.cpp \
    test_requestcheck.cpp \
    ../requestcheck.cpp \
    ../cellstiffness_gpu.cpp

HEADERS += \
    test_polycrystall_seeding.h \
    test_polycrystall_growth.h \
    test_wave_nucleation.h \
    test_texturemath.h \
    test_hillcriterion.h \
    test_svgexporter.h \
    test_commandline_parser.h \
    test_lattice.h \
    test_ffthomog.h \
    test_printability.h \
    test_datasetbrowser.h \
    ../datasetbrowsercontroller.h \
    test_seed_distribution.h \
    test_propertysearch.h \
    ../propertysearch.h \
    ../propertyselectorcontroller.h \
    test_celldecoder.h \
    ../celldecoder.h \
    test_celldiffusion.h \
    ../celldiffusion.h \
    ../cellnet.h \
    test_cellpredictor.h \
    ../cellpredictor.h \
    test_cellstiffness.h \
    ../cellstiffness.h \
    test_requestcheck.h \
    ../requestcheck.h \
    ../cellstiffness_gpu.h

# The decoder weights the program ships (:/models/cell_decoder.bin)
RESOURCES += ../models.qrc

TARGET = tests
