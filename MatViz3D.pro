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

QMAKE_CXXFLAGS += -fopenmp
LIBS += -fopenmp

CONFIG += qmltypes
QML_IMPORT_NAME = OpenGLUnderQML
QML_IMPORT_MAJOR_VERSION = 1

QMAKE_QMLTYPEREGISTRAR = qmltyperegistrar
QMAKE_QMLCACHEGEN = qmlcachegen
QMAKE_QMLDEBUGGER = qmldebugger

CONFIG += console

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
        dlca.cpp \
        legendview.cpp \
        loadstepmanager.cpp \
        logo_printer.cpp \
        main.cpp \
        mainwindowalgorithmhandler.cpp \
        mainwindowwrapper.cpp \
        materialdatabaseviewwrapper.cpp \
        moore.cpp \
        neumann.cpp \
        openglwidgetqml.cpp \
        parameters.cpp \
        parent_algorithm.cpp \
        probability_algorithm.cpp \
        probability_circle.cpp \
        probability_ellipse.cpp \
        radial.cpp \
        renderopengl.cpp \
        schemacontroller.cpp \
        statisticscontroller.cpp \
        stiffnesssurfacebuilder.cpp \
        stiffnesssurfaceitem.cpp \
        stiffnesssurfacerenderer.cpp \
        stressanalysis.cpp \
        stressanalysis_fft.cpp \
        stressanalysiscontroller.cpp \
        tensorfieldsnapshot.cpp \
        tensorglyphbuilder.cpp \
        texturelibrary.cpp \
        texturecontroller.cpp \
        voronoi.cpp

RESOURCES += qml.qrc

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
    commandline_parser.h \
    consolelogger.h \
    cpuinfo.hpp \
    dbmanager.h \
    exportcontroller.h \
    fft_homog.hpp \
    fft_solver_session.hpp \
    glshaders.hpp \
    glvertex.hpp \
    grain_analyzer.h \
    hdf5wrapper.h \
    hillcriterion.h \
    algorithmfactory.h \
    algorithmplugin.h \
    ansyswrapper.h \
    composite.h \
    dlca.h \
    legendview.h \
    logo_printer.h \
    loadstepmanager.h \
    mainwindowalgorithmhandler.h \
    mainwindowwrapper.h \
    materialdatabaseviewwrapper.h \
    matviz_homog.hpp \
    moore.h \
    neumann.h \
    openglwidgetqml.h \
    parameters.h \
    paramfield.h \
    parent_algorithm.h \
    probability_algorithm.h \
    probability_circle.h \
    probability_ellipse.h \
    radial.h \
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
    tensorfieldsnapshot.h \
    tensorglyphbuilder.h \
    tensormath.hpp \
    tensormath_selftest.hpp \
    texturelibrary.h \
    texturemath.hpp \
    texturecontroller.h \
    voronoi.h
