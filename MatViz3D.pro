QT       += core gui opengl printsupport openglwidgets charts

#include opengl libs
linux: LIBS += -lGL
win32: LIBS += -lopengl32

#enable openmp
QMAKE_CXXFLAGS += -fopenmp
LIBS += -fopenmp
RC_FILE = app_icon.rc

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets


CONFIG += c++17 console

# define include paths for hdf5 library
linux {
    HDF5_INCLUDEPATH = "/usr/include/hdf5/serial"
    HDF5_LIBPATH = "/usr/lib/x86_64-linux-gnu"
    LIBS += -L$${HDF5_LIBPATH} -lhdf5_serial
    INCLUDEPATH += $$HDF5_INCLUDEPATH
}

macx {
    # OpenMP — Apple Clang does not include it; use Homebrew libomp
    QMAKE_CXXFLAGS -= -fopenmp
    LIBS           -= -fopenmp
    LIBOMP_PREFIX  = $$(LIBOMP_PREFIX)
    isEmpty(LIBOMP_PREFIX): LIBOMP_PREFIX = $$system(brew --prefix libomp 2>/dev/null)
    QMAKE_CXXFLAGS += -Xpreprocessor -fopenmp -I$$LIBOMP_PREFIX/include
    LIBS           += -L$$LIBOMP_PREFIX/lib -lomp

    # HDF5 via Homebrew
    HDF5_PREFIX    = $$(HDF5_PREFIX)
    isEmpty(HDF5_PREFIX): HDF5_PREFIX = $$system(brew --prefix hdf5 2>/dev/null)
    INCLUDEPATH    += $$HDF5_PREFIX/include
    LIBS           += -L$$HDF5_PREFIX/lib -lhdf5
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

include(3rdparty/qtgifimage/src/gifimage/qtgifimage.pri)


# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    about.cpp \
    algorithmfactory.cpp \
    ansyswrapper.cpp \
    composite.cpp \
    console.cpp \
    export.cpp \
    dlca.cpp \
    hdf5wrapper.cpp \
    legendview.cpp \
    loadstepmanager.cpp \
    main.cpp \
    mainwindow.cpp \
    messagehandler.cpp \
    moore.cpp \
    myglwidget.cpp \
    neumann.cpp \
    parameters.cpp \
    parent_algorithm.cpp \
    probability_algorithm.cpp \
    probability_circle.cpp \
    probability_ellipse.cpp \
    radial.cpp \
    statistics.cpp \
    stressanalysis.cpp

HEADERS += \
    about.h \
    algorithmfactory.h \
    ansyswrapper.h \
    composite.h \
    console.h \
    cpuinfo.hpp \
    export.h \
    dlca.h \
    hdf5wrapper.h \
    legendview.h \
    loadstepmanager.h \
    mainwindow.h \
    messagehandler.h \
    moore.h \
    myglwidget.h \
    neumann.h \
    parameters.h \
    parent_algorithm.h \
    probability_algorithm.h \
    probability_circle.h \
    probability_ellipse.h \
    radial.h \
    statistics.h \
    stressanalysis.h

FORMS += \
    about.ui \
    mainwindow.ui \
    probability_algorithm.ui \
    statistics.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    resourses.qrc
