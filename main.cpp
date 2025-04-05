#include <QGuiApplication>
#include <QApplication>
#include <QQmlApplicationEngine>
#include <QtQuickControls2>
#include <QIcon>
#include "dbmanager.h"
#include "parameters.h"
#include "mainwindowwrapper.h"
#include "materialdatabaseviewwrapper.h"
#include "probabilityalgorithmviewwrapper.h"


int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
    QApplication app(argc, argv);

    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    QIcon icon(":/img/Plugin icon - 1icon.ico");
    app.setWindowIcon(icon);

    QQuickStyle::setStyle("Material");

    QQmlApplicationEngine engine;

    qmlRegisterSingletonInstance<Parameters>("parameters", 1, 0, "Parameters", Parameters::instance());

    DBManager dbManager;
    engine.rootContext()->setContextProperty("dbManager", &dbManager);
    engine.rootContext()->setContextProperty("materialModel", dbManager.getModel());

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

    return app.exec();
}

