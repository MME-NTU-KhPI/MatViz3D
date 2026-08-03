# ============================================================================
#  solver_compare.pro  —  console build of the ANSYS-vs-FFT comparison harness.
#
#  Reuses the existing MatViz3D translation units (so it calls the REAL
#  StressAnalysis / StressAnalysisFFT code), but replaces main.cpp with
#  solver_compare_main.cpp and builds a console executable.
#
#      qmake solver_compare.pro
#      make -j
#      ./solver_compare --help
#
#  ANSYS must be on PATH for the --solver both|ansys modes to actually run
#  (the harness degrades gracefully to FFT-only if the ANSYS process fails).
# ============================================================================

QT += core gui opengl widgets openglwidgets quick quickcontrols2 sql printsupport quickwidgets
CONFIG += console c++17
CONFIG -= app_bundle
TARGET = solver_compare

win32 { LIBS += -lopengl32 }
unix:!macos { LIBS += -lGL }
macos { LIBS += -framework OpenGL }

QMAKE_CXXFLAGS += -fopenmp
LIBS += -fopenmp

# HDF5 (same detection as MatViz3D.pro)
unix {
    HDF5_INCLUDEPATH = "/usr/include/hdf5/serial"
    HDF5_LIBPATH = "/usr/lib/x86_64-linux-gnu"
    LIBS += -L$${HDF5_LIBPATH} -lhdf5_serial
    INCLUDEPATH += $$HDF5_INCLUDEPATH
}
win32 {
    HDF5_ROOT = "C:\Program Files\HDF_Group\HDF5"
    HCMD = dir /B /AD \"$$HDF5_ROOT\" | findstr \"^[0-9]\"
    HDF5_VERSION = $$system($$HCMD)
    HDF5_LIBPATH = "$$HDF5_ROOT/$$HDF5_VERSION/lib"
    HDF5_INCLUDEPATH = "$$HDF5_ROOT/$$HDF5_VERSION/include"
    INCLUDEPATH += $$HDF5_INCLUDEPATH
    LIBS += -L$$HDF5_LIBPATH -lhdf5
}

# Same source set as MatViz3D.pro, with main.cpp -> solver_compare_main.cpp
SOURCES += \
        solver_compare_main.cpp \
        commandline_parser.cpp \
        consolelogger.cpp \
        dbmanager.cpp \
        exportcontroller.cpp \
        grain_analyzer.cpp \
        hdf5wrapper.cpp \
        hillcriterion.cpp \
        algorithmfactory.cpp \
        ansyswrapper.cpp \
        composite.cpp \
        dlca.cpp \
        legendview.cpp \
        loadstepmanager.cpp \
        logo_printer.cpp \
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
        stressanalysis.cpp \
        stressanalysis_fft.cpp \
        stressanalysiscontroller.cpp

HEADERS += \
    commandline_parser.h consolelogger.h cpuinfo.hpp dbmanager.h exportcontroller.h \
    fft_homog.hpp fft_solver_session.hpp grain_analyzer.h hdf5wrapper.h hillcriterion.h \
    algorithmfactory.h ansyswrapper.h composite.h dlca.h legendview.h logo_printer.h \
    loadstepmanager.h mainwindowalgorithmhandler.h mainwindowwrapper.h \
    materialdatabaseviewwrapper.h matviz_homog.hpp moore.h neumann.h openglwidgetqml.h \
    parameters.h paramfield.h parent_algorithm.h probability_algorithm.h \
    probability_circle.h probability_ellipse.h radial.h renderopengl.h schemacontroller.h \
    statisticscontroller.h stressanalysis.h stressanalysis_fft.h stressanalysiscontroller.h \
    stressresult.h

# qml.qrc is intentionally omitted: this is a console tool, no QML engine is
# started. If your build complains about missing QML type registrations, add
#   RESOURCES += qml.qrc
# back in.
