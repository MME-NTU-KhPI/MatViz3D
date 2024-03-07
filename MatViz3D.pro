QT       += core gui opengl printsupport
QT       += core gui charts

LIBS += -lopengl32

QMAKE_CXXFLAGS += -fopenmp
LIBS += -fopenmp

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
QT += openglwidgets

CONFIG += c++17

include(3rdparty/qtgifimage/src/gifimage/qtgifimage.pri)

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    about.cpp \
    animation.cpp \
    console.cpp \
    export.cpp \
    dlca.cpp \
    main.cpp \
    mainwindow.cpp \
    messagehandler.cpp \
    moore.cpp \
    myglwidget.cpp \
    neumann.cpp \
    parameters.cpp \
    parent_algorithm.cpp \
    probability_circle.cpp \
    probability_ellipse.cpp \
    radial.cpp \
    statistics.cpp

HEADERS += \
    about.h \
    animation.h \
    console.h \
    export.h \
    dlca.h \
    mainwindow.h \
    messagehandler.h \
    moore.h \
    myglwidget.h \
    neumann.h \
    parameters.h \
    parent_algorithm.h \
    probability_circle.h \
    probability_ellipse.h \
    radial.h \
    statistics.h

FORMS += \
    about.ui \
    mainwindow.ui \
    statistics.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    resourses.qrc
