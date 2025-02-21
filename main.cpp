#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtQuickControls2>
#include <QIcon>
#include "dbmanager.h"


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

    qmlRegisterType<DBManager>("DBModule", 1, 0, "DataBase");

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
