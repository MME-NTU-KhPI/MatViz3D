QT += quick quickcontrols2 sql printsupport
QT += core gui opengl widgets
QT += openglwidgets
QT += quickwidgets

LIBS += -lopengl32

QMAKE_CXXFLAGS += -fopenmp
LIBS += -fopenmp

CONFIG += qmltypes
QML_IMPORT_NAME = OpenGLUnderQML
QML_IMPORT_MAJOR_VERSION = 1

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        dbmanager.cpp \
        algorithmfactory.cpp \
        ansyswrapper.cpp \
        composite.cpp \
        dlca.cpp \
        legendview.cpp \
        main.cpp \
        mainwindowwrapper.cpp \
        materialdatabaseviewwrapper.cpp \
        moore.cpp \
        myglwidget.cpp \
        neumann.cpp \
        openglwidgetqml.cpp \
        parameters.cpp \
        parent_algorithm.cpp \
        probability_algorithm.cpp \
        probability_circle.cpp \
        probability_ellipse.cpp \
        probabilityalgorithmviewwrapper.cpp \
        radial.cpp \
        renderopengl.cpp

RESOURCES += qml.qrc

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

# Additional import path used to resolve QML modules just for Qt Quick Designer
QML_DESIGNER_IMPORT_PATH =

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

HEADERS += \
    dbmanager.h \
    algorithmfactory.h \
    ansyswrapper.h \
    composite.h \
    dlca.h \
    legendview.h \
    mainwindowwrapper.h \
    materialdatabaseviewwrapper.h \
    moore.h \
    myglwidget.h \
    neumann.h \
    openglwidgetqml.h \
    parameters.h \
    parent_algorithm.h \
    probability_algorithm.h \
    probability_circle.h \
    probability_ellipse.h \
    probabilityalgorithmviewwrapper.h \
    radial.h \
    renderopengl.h
