#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtQuickControls2>
#include <QIcon>

#include "myglwidget.h"
#include "mainwindowwrapper.h"
#include "materialdatabaseviewwrapper.h"
#include "probabilityalgorithmviewwrapper.h"


int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
    QGuiApplication app(argc, argv);

    QIcon icon(":/img/Plugin icon - 1icon.ico");
    app.setWindowIcon(icon);

    QQuickStyle::setStyle("Material");

    QQmlApplicationEngine engine;

    MainWindowWrapper mainWindowWrapper;
    MaterialDatabaseViewWrapper materialDatabaseViewWrapper;
    ProbabilityAlgorithmViewWrapper probabilityAlgorithmViewWrapper;

    engine.rootContext()->setContextProperty("mainWindowWrapper", &mainWindowWrapper);
    engine.rootContext()->setContextProperty("materialDatabaseViewWrapper", &materialDatabaseViewWrapper);
    engine.rootContext()->setContextProperty("probabilityAlgorithmViewWrapper", &probabilityAlgorithmViewWrapper);

    const QUrl url(QStringLiteral("qrc:/main.qml"));
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreated,
        &app,
        [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);
    engine.load(url);

    QObject *rootObject = nullptr;
    QList<QObject*> rootObjects = engine.rootObjects();
    if (!rootObjects.isEmpty()) {
        rootObject = rootObjects.first();
    }

    QQuickItem *sceneItem = rootObject ? rootObject->findChild<QQuickItem*>("sceneOpenGL") : nullptr;

    if (sceneItem) {
        MyGLWidget *glWidget = new MyGLWidget();
        glWidget->setSceneParent(sceneItem);
    }

    return app.exec();
}



// #include <QApplication>
// #include <QQuickView>
// #include <QQmlContext>
// #include <QWidget>
// #include <QVBoxLayout>
// #include <QQuickWidget>
// #include <QOpenGLWidget>

// #include "myglwidget.h"

// int main(int argc, char *argv[]) {
//     QApplication app(argc, argv);

//     // Завантажуємо QML у QQuickView
//     QQuickView *view = new QQuickView();
//     view->setSource(QUrl(QStringLiteral("qrc:/main.qml")));
//     view->setResizeMode(QQuickView::SizeRootObjectToView);

//     // Створюємо контейнер для QQuickView
//     QWidget *qmlContainer = QWidget::createWindowContainer(view);
//     qmlContainer->setMinimumSize(800, 600);
//     qmlContainer->setFocusPolicy(Qt::StrongFocus);

//     // Створюємо головне вікно з layout
//     QWidget mainWindow;
//     QVBoxLayout *layout = new QVBoxLayout(&mainWindow);

//     // Додаємо QML у головне вікно
//     layout->addWidget(qmlContainer);

//     // Додаємо OpenGL-віджет
//     MyGLWidget *glWidget = new MyGLWidget();
//     layout->addWidget(static_cast<QWidget*>(glWidget));

//     mainWindow.show();
//     return app.exec();
// }



// #include <QGuiApplication>
// #include <QQmlApplicationEngine>
// #include <QtQuickControls2>
// #include <QIcon>
// #include <QWidget>

// #include "myglwidget.h"
// #include "mainwindowwrapper.h"
// #include "materialdatabaseviewwrapper.h"
// #include "probabilityalgorithmviewwrapper.h"

// int main(int argc, char *argv[])
// {
// #if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
//     QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
// #endif
//     QGuiApplication app(argc, argv);

//     QIcon icon(":/img/Plugin icon - 1icon.ico");
//     app.setWindowIcon(icon);

//     QQuickStyle::setStyle("Material");

//     QQmlApplicationEngine engine;

//     MainWindowWrapper mainWindowWrapper;
//     MaterialDatabaseViewWrapper materialDatabaseViewWrapper;
//     ProbabilityAlgorithmViewWrapper probabilityAlgorithmViewWrapper;

//     engine.rootContext()->setContextProperty("mainWindowWrapper", &mainWindowWrapper);
//     engine.rootContext()->setContextProperty("materialDatabaseViewWrapper", &materialDatabaseViewWrapper);
//     engine.rootContext()->setContextProperty("probabilityAlgorithmViewWrapper", &probabilityAlgorithmViewWrapper);

//     const QUrl url(QStringLiteral("qrc:/main.qml"));
//     QObject::connect(
//         &engine,
//         &QQmlApplicationEngine::objectCreated,
//         &app,
//         [url](QObject *obj, const QUrl &objUrl) {
//             if (!obj && url == objUrl)
//                 QCoreApplication::exit(-1);
//         },
//         Qt::QueuedConnection);
//     engine.load(url);

//     QObject *rootObject = nullptr;
//     QList<QObject*> rootObjects = engine.rootObjects();
//     if (!rootObjects.isEmpty()) {
//         rootObject = rootObjects.first();
//     }

//     QQuickItem *sceneItem = rootObject ? rootObject->findChild<QQuickItem*>("sceneOpenGL") : nullptr;

//     if (sceneItem) {
//         MyGLWidget *glWidget = new MyGLWidget();

//         // Створення контейнера для OpenGL віджету
//         QWidget *widgetContainer = QWidget::createWindowContainer(glWidget);

//         widgetContainer->setFocusPolicy(Qt::StrongFocus);

//         QObject::connect(sceneItem, &QQuickItem::widthChanged, glWidget, [glWidget, sceneItem]() {
//             glWidget->resize(sceneItem->width(), sceneItem->height());
//         });
//         QObject::connect(sceneItem, &QQuickItem::heightChanged, glWidget, [glWidget, sceneItem]() {
//             glWidget->resize(sceneItem->width(), sceneItem->height());
//         });

//         // Додавання контейнера в основне вікно
//         QWidget *mainWidget = qobject_cast<QWidget*>(rootObject);
//         if (mainWidget) {
//             widgetContainer->setParent(mainWidget);
//         }
//     }

//     return app.exec();
// }
