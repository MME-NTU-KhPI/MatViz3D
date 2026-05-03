#include <QGuiApplication>
#include <QApplication>
#include <QQmlApplicationEngine>
#include <QtQuickControls2>
#include <QIcon>
#include "openglwidgetqml.h"
#include "dbmanager.h"
#include "parameters.h"
#include "mainwindowwrapper.h"
#include "materialdatabaseviewwrapper.h"
#include "probabilityalgorithmviewwrapper.h"
#include "consolelogger.h"
#include "commandline_parser.h"

#ifdef _WIN32
    #include <windows.h>
#endif

void enable_virtual_term()
{
    #ifdef _WIN32
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h != INVALID_HANDLE_VALUE)
    {
        DWORD mode = 0;
        if (GetConsoleMode(h, &mode))
        {
            mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(h, mode);
        }
    }
    #endif
    return;
}

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
    QApplication app(argc, argv);

    QApplication::setApplicationName("MatViz3D");
    QApplication::setApplicationVersion("3.01");

    // ── Parse CLI early so --help / --version exit before any UI is built,
    //    and so --nogui can be checked before loading QML at all.
    QCommandLineParser parser;
    Commandline_Parser::setupParser(parser);
    parser.process(app);   // exits here if --help / --version

    // ── Optional: skip QML entirely in headless mode ──────────────────────
    if (parser.isSet("nogui")) {
        Commandline_Parser::processOptions(parser);
        return 0;
    }

    // Install before engine
    qInstallMessageHandler(ConsoleLogger::messageHandler);
     // ── UI setup ──────────────────────────────────────────────────────────
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    QIcon icon(":/img/Plugin icon - 1icon.ico");
    app.setWindowIcon(icon);

    QQuickStyle::setStyle("Material");

    QQmlApplicationEngine engine;

    // Register to QML
    engine.rootContext()->setContextProperty("ConsoleLogger", ConsoleLogger::instance());

    qmlRegisterSingletonInstance<Parameters>("parameters", 1, 0, "Parameters", Parameters::instance());

    DBManager dbManager;
    engine.rootContext()->setContextProperty("dbManager", &dbManager);
    engine.rootContext()->setContextProperty("materialModel", dbManager.getModel());

    // //SQL-запит для отримання данних.
    // QString sql = "SELECT Material, c11, c44 FROM material_properties WHERE Type = 'bcc'";
    // //Отримаемо вектор-список результатів
    // QVariantList results = dbManager.executeSelectQuery(sql);

    // // Тепер ви можете обробити результати. Приклад.
    // for (const QVariant& item : results)
    // {
    //     QVariantMap row = item.toMap();
    //     qDebug() << "Матеріал:" << row["Material"].toString()
    //              << ", C11:" << row["c11"].toDouble()
    //              << ", C44:" << row["c44"].toDouble();
    // }

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

    QTimer::singleShot(0, [&mainWindowWrapper, &parser]() {
        Commandline_Parser::processOptions(parser);

        OpenGLWidgetQML *ogl = OpenGLWidgetQML::getInstance();
        if (!ogl) return;

        Parameters *p = Parameters::instance();
        ogl->setNumColors(p->getPoints());   // sync grain colors
        ogl->setNumCubes(p->getSize());      // sync cube size

        if (parser.isSet("autostart"))           // or however you track this flag
            mainWindowWrapper.onStartButton();
    });


    return app.exec();
}

