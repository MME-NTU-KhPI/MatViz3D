QT       += core gui opengl printsupport
QT       += core gui charts

LIBS += -lopengl32

QMAKE_CXXFLAGS += -fopenmp
LIBS += -fopenmp

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
QT += openglwidgets

CONFIG += c++17 console

LIBS += -L$$PWD/libs/hdf5_1.14.3/lib/

LIBS        += -lhdf5
#LIBS        += -lhdf5_cpp
#LIBS        += -lhdf5_hl
#LIBS        += -lhdf5_hl_cpp
#LIBS        += -lhdf5_tools
#LIBS        += -lzlib

#LIBS        += -llibhdf5
#LIBS        += -llibhdf5_cpp
#LIBS        += -llibhdf5_hl
#LIBS        += -llibhdf5_hl_cpp
#LIBS        += -llibhdf5_tools
#LIBS        += -llibzlib

#-lhdf5_cpp -lhdf5_hl_cpp -lhdf5 -lhdf5_hl

INCLUDEPATH += $$PWD/libs/hdf5_1.14.3/include
DEPENDPATH += $$PWD/libs/hdf5_1.14.3/include


#-lszaec -laec -lzlib -lhdf5 -lhdf5_cpp   #for static linking
# -lhdf5 -lhdf5_cpp                       #for dynamic linking



include(3rdparty/qtgifimage/src/gifimage/qtgifimage.pri)

# Підключення HDF5 бібліотек (відносно кореня проекту)
LIBS += -L$$PWD/libs/hdf5-1.14.4-2-win-vs2022_cl/hdf5/HDF5-1.14.4-win64/lib/

# Підключення конкретних HDF5 бібліотек
LIBS += -lhdf5 -lhdf5_cpp -lhdf5_hl -lhdf5_hl_cpp

# Шляхи до заголовків HDF5
INCLUDEPATH += $$PWD/libs/hdf5-1.14.4-2-win-vs2022_cl/hdf5/HDF5-1.14.4-win64/include/

# Шляхи для пошуку заголовочних файлів
DEPENDPATH += $$PWD/libs/hdf5-1.14.4-2-win-vs2022_cl/hdf5/HDF5-1.14.4-win64/include/



# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    about.cpp \
    animation.cpp \
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
    animation.h \
    ansyswrapper.h \
    composite.h \
    console.h \
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
