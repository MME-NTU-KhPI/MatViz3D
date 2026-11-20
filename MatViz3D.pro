QT += quick quickcontrols2 sql printsupport
QT += core gui opengl widgets
QT += openglwidgets
QT += quickwidgets
QT += concurrent


win32 {
    LIBS += -lopengl32
}

unix:!macos {
    CONFIG += link_pkgconfig
    PKGCONFIG += gl
}

macos {
    LIBS += -framework OpenGL
}

QML_LIBEXEC = $$(QT_QML_LIBEXEC)
!isEmpty(QML_LIBEXEC) {
    QT_TOOL.qmltyperegistrar.binary = $$QML_LIBEXEC/qmltyperegistrar
    QT_TOOL.qmlcachegen.binary      = $$QML_LIBEXEC/qmlcachegen
    QT_TOOL.qmlimportscanner.binary = $$QML_LIBEXEC/qmlimportscanner
}

# -Wsfinae-incomplete exists only in GCC 16+; guard so older toolchains don't choke
gcc_ver = $$system($$QMAKE_CXX -dumpversion)
greaterThan(gcc_ver, 15): QMAKE_CXXFLAGS += -Wno-sfinae-incomplete

# macOS: the folder (with include/ and lib/) of a package that does not come with the system.
# $$1 = Homebrew formula, $$2 = a header that proves the package is there, $$3 = the folder
# given in the environment (used as it is when set). Homebrew's prefix differs between Macs
# (/opt/homebrew on Apple silicon, /usr/local on Intel, anything for a custom install), so it
# is asked from brew first; the two standard prefixes are tried after it because a build
# started from Qt Creator may not have brew on its PATH. Empty when nothing is found.
defineReplace(macPackageDir) {
    given = $$3
    !isEmpty(given): return($$given)
    candidates = $$system(brew --prefix $$1 2>/dev/null) /opt/homebrew/opt/$$1 /usr/local/opt/$$1
    for(dir, candidates) {
        exists($$dir/include/$$2): return($$dir)
    }
    return()
}

macos {
    # Apple clang has no OpenMP runtime of its own and macOS has no HDF5: both are installed
    # separately (brew install libomp hdf5). LIBOMP_DIR and HDF5_DIR in the environment name
    # another location, as HDF5_DIR does on Windows.
    LIBOMP_DIR = $$macPackageDir(libomp, omp.h, $$(LIBOMP_DIR))
    HDF5_DIR   = $$macPackageDir(hdf5, H5Cpp.h, $$(HDF5_DIR))
    isEmpty(LIBOMP_DIR): error("libomp not found: run 'brew install libomp' or set LIBOMP_DIR to the folder that holds include/omp.h and lib/")
    isEmpty(HDF5_DIR):   error("HDF5 not found: run 'brew install hdf5' or set HDF5_DIR to the folder that holds include/H5Cpp.h and lib/")
    message(LIBOMP_DIR = $$LIBOMP_DIR)
    message(HDF5_DIR = $$HDF5_DIR)
    QMAKE_CXXFLAGS += -Xpreprocessor -fopenmp
    INCLUDEPATH += $$LIBOMP_DIR/include $$HDF5_DIR/include
    LIBS += -L$$LIBOMP_DIR/lib -lomp -L$$HDF5_DIR/lib -lhdf5 -lhdf5_cpp
    # The matrix products of the networks of the Neural tab (cellnet.cpp) go to Accelerate's sgemm
    DEFINES += MATVIZ_USE_ACCELERATE
    LIBS += -framework Accelerate
} else {
    QMAKE_CXXFLAGS += -fopenmp
    LIBS += -fopenmp
}

# Where the sources are: models/cell_diffusion.bin (94 MB) and models/cell_predictor.bin (12 MB), not embedded, are looked for there too
DEFINES += "MATVIZ_SOURCE_DIR=\\\"$$PWD\\\""

CONFIG += qmltypes
QML_IMPORT_NAME = OpenGLUnderQML
QML_IMPORT_MAJOR_VERSION = 1

QMAKE_QMLTYPEREGISTRAR = qmltyperegistrar
QMAKE_QMLCACHEGEN = qmlcachegen
QMAKE_QMLDEBUGGER = qmldebugger

CONFIG += console

CONFIG(debug, debug|release) {
    QMAKE_QMLCACHEGEN =
}

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

# define include paths for hdf5 library
unix:!macos {
    #(NixOS, Arch, Fedora)
    CONFIG += link_pkgconfig
    PKGCONFIG += hdf5

    # Ubuntu/Debian,
    # where hdf5 can be hdf5_serial
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
        message(HDF5_ROOT = $$HDF5_ROOT)
        HCMD = dir /B /AD \"$$HDF5_ROOT\" | findstr \"^[0-9]\"
        message(HCMD = $$HCMD)
        HDF5_VERSION = $$system($$HCMD)
        message(HDF5_VERSION = $$HDF5_VERSION)
        HDF5_DIR = "$$HDF5_ROOT/$$HDF5_VERSION"
    }
    HDF5_LIBPATH = "$$HDF5_DIR/lib"
    message(HDF5_LIBPATH = $$HDF5_LIBPATH)
    HDF5_INCLUDEPATH = "$$HDF5_DIR/include"
    message(HDF5_INCLUDEPATH = $$HDF5_INCLUDEPATH)
    INCLUDEPATH += $$HDF5_INCLUDEPATH
    LIBS += -L$$HDF5_LIBPATH -lhdf5
}

SOURCES += \
        commandline_parser.cpp \
        config_source.cpp \
        consolelogger.cpp \
        dbmanager.cpp \
        exportcontroller.cpp \
        grain_analyzer.cpp \
        hdf5wrapper.cpp \
        hillcriterion.cpp \
        algorithmfactory.cpp \
        algorithmplugin.cpp \
        ansyswrapper.cpp \
        composite.cpp \
        deformed_state_analyzer.cpp \
        dlca.cpp \
        lattice.cpp \
        mycelium.cpp \
        cellforms.cpp \
        printability.cpp \
        legendview.cpp \
        loadstepmanager.cpp \
        logo_printer.cpp \
        main.cpp \
        mainwindowalgorithmhandler.cpp \
        mainwindowwrapper.cpp \
        materialdatabaseviewwrapper.cpp \
        openglwidgetqml.cpp \
        parameters.cpp \
        parent_algorithm.cpp \
        polycrystall.cpp \
        probability_algorithm.cpp \
        renderopengl.cpp \
        schemacontroller.cpp \
        statisticscontroller.cpp \
        stiffnesssurfacebuilder.cpp \
        stiffnesssurfaceitem.cpp \
        stiffnesssurfacerenderer.cpp \
        stressanalysis.cpp \
        stressanalysis_fft.cpp \
        stressanalysiscontroller.cpp \
        hdf5projectcontroller.cpp \
        datasetbrowsercontroller.cpp \
        propertysearch.cpp \
        propertyselectorcontroller.cpp \
        celldecoder.cpp \
        celldiffusion.cpp \
        cellnet.cpp \
        cellpredictor.cpp \
        cellstiffness.cpp \
        requestcheck.cpp \
        cellstiffness_gpu.cpp \
        neuralgeneratorcontroller.cpp \
        tensorfieldsnapshot.cpp \
        tensorglyphbuilder.cpp \
        tensorstreamlinebuilder.cpp \
        texturelibrary.cpp \
        texturecontroller.cpp \
        voronoi.cpp

RESOURCES += qml.qrc
# Weights of the cell decoder (the Neural tab), written by tools/neural/export_cell_decoder.py
RESOURCES += models.qrc

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

# Additional import path used to resolve QML modules just for Qt Quick Designer
QML_DESIGNER_IMPORT_PATH =

MATVIZ_FORCE_ORIENT_DEG="30,0,0"

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

HEADERS += \
    colormap.hpp \
    cornergizmo.hpp \
    commandline_parser.h \
    config_source.h \
    consolelogger.h \
    cpuinfo.hpp \
    dbmanager.h \
    deformed_state_analyzer.h \
    exportcontroller.h \
    fft_homog.hpp \
    fft_solver_session.hpp \
    glshaders.hpp \
    glvertex.hpp \
    grain_analyzer.h \
    hdf5projectcontroller.h \
    datasetbrowsercontroller.h \
    propertysearch.h \
    propertyselectorcontroller.h \
    celldecoder.h \
    celldiffusion.h \
    cellnet.h \
    cellpredictor.h \
    cellstiffness.h \
    requestcheck.h \
    cellstiffness_gpu.h \
    neuralgeneratorcontroller.h \
    hdf5wrapper.h \
    hillcriterion.h \
    algorithmfactory.h \
    algorithmplugin.h \
    ansyswrapper.h \
    composite.h \
    dlca.h \
    lattice.h \
    latticefamily.h \
    mycelium.h \
    cellforms.h \
    printability.h \
    periodic_wrap.hpp \
    legendview.h \
    logo_printer.h \
    loadstepmanager.h \
    mainwindowalgorithmhandler.h \
    mainwindowwrapper.h \
    materialdatabaseviewwrapper.h \
    matviz_homog.hpp \
    openglwidgetqml.h \
    parameters.h \
    paramfield.h \
    parent_algorithm.h \
    phasematerial.h \
    polycrystall.h \
    probability_algorithm.h \
    renderopengl.h \
    schemacontroller.h \
    statisticscontroller.h \
    stiffnesssurfacebuilder.h \
    stiffnesssurfaceitem.h \
    stiffnesssurfacerenderer.h \
    stressanalysis.h \
    stressanalysis_fft.h \
    stressanalysiscontroller.h \
    stressresult.h \
    svgexporter.hpp \
    tensorfieldsnapshot.h \
    tensorglyphbuilder.h \
    tensormath.hpp \
    tensormath_selftest.hpp \
    texturelibrary.h \
    texturemath.hpp \
    texturecontroller.h \
    voronoi.h
