QT       += core gui opengl printsupport openglwidgets charts

unix: LIBS += -lGL
win32: LIBS += -lopengl32

QMAKE_CXXFLAGS += -fopenmp
LIBS += -fopenmp

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets


CONFIG += c++17 console

unix: LIBS += -L"/usr/lib/x86_64-linux-gnu" -lhdf5_serial
win32:LIBS += -lhdf5


#INCLUDEPATH += $$PWD/libs/hdf5_1.14.3/include
#DEPENDPATH += $$PWD/libs/hdf5_1.14.3/include

unix:INCLUDEPATH += /usr/include/hdf5/serial
#unix:DEPENDPATH += /usr/lib/x86_64-linux-gnu


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
