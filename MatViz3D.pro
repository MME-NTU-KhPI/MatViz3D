QT       += core gui opengl

LIBS += -lopengl32

QMAKE_CXXFLAGS += -fopenmp
LIBS += -fopenmp

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
QT += openglwidgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    moore.cpp \
    myglwidget.cpp \
    neumann.cpp \
    parent_algorithm.cpp \
    probability_circle.cpp \
    probability_ellipse.cpp

HEADERS += \
    mainwindow.h \
    moore.h \
    myglwidget.h \
    neumann.h \
    parent_algorithm.h \
    probability_circle.h \
    probability_ellipse.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    resourses.qrc
