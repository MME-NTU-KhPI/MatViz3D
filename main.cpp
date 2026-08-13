#include <QGuiApplication>
#include <QApplication>
#include <QQmlApplicationEngine>
#include <QtQuickControls2>
#include <QIcon>
#include "algorithmfactory.h"
#include "openglwidgetqml.h"
#include "dbmanager.h"
#include "parameters.h"
#include "mainwindowwrapper.h"
#include "materialdatabaseviewwrapper.h"
#include "consolelogger.h"
#include "commandline_parser.h"
#include "logo_printer.h"
#include "schemacontroller.h"
#include "statisticscontroller.h"
#include "exportcontroller.h"
#include "stressanalysiscontroller.h"
#include "texturecontroller.h"

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


    Logo::print(Logo::Style::Full);

    // ── Parse CLI early so --help / --version exit before any UI is built,
    //    and so --nogui can be checked before loading QML at all.
    QCommandLineParser parser;
    Commandline_Parser::setupParser(parser);
    parser.process(app);   // exits here if --help / --version

    // ── Optional: skip QML entirely in headless mode ──────────────────────
    if (parser.isSet("nogui")) {
        Commandline_Parser::processOptions(parser);
        if (!parser.isSet("autostart")) return 0;

        MainWindowAlgorithmHandler handler;
        handler.runAlgorithm(Parameters::instance()->getAlgorithm(), false);

        if (parser.isSet("run_stress_calc"))
            handler.runStressCalculation();

        return 0;
    }

    // Install before engine
    qInstallMessageHandler(ConsoleLogger::messageHandler);
     // ── UI setup ──────────────────────────────────────────────────────────
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    QIcon icon(":/img/Plugin icon - 1icon.ico");
    app.setWindowIcon(icon);

    QQuickStyle::setStyle("Material");

    // NOTE: all QML-exposed controllers below must outlive the QML engine.
    // QQmlApplicationEngine is declared last (and therefore destroyed
    // *first*, before these controllers) so that no QML binding can observe
    // a context property go null mid-teardown when the app closes.
    DBManager dbManager;

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
    SchemaController schemaController;
    StatisticsController statisticsController;
    ExportController exportController;
    StressAnalysisController stressAnalysisController;
    TextureController textureController;
    // Both before the QML engine: the algorithm combo box reads the registry,
    // so every algorithm has to be in it by the time the panel is built.
    registerAlgorithms();
    registerSchemas();

    QQmlApplicationEngine engine;

    // Register to QML
    engine.rootContext()->setContextProperty("ConsoleLogger", ConsoleLogger::instance());

    qmlRegisterSingletonInstance<Parameters>("parameters", 1, 0, "Parameters", Parameters::instance());

    engine.rootContext()->setContextProperty("dbManager", &dbManager);
    engine.rootContext()->setContextProperty("materialModel", dbManager.getModel());

    engine.rootContext()->setContextProperty("mainWindowWrapper", &mainWindowWrapper);
    engine.rootContext()->setContextProperty("materialDatabaseViewWrapper", &materialDatabaseViewWrapper);
    engine.rootContext()->setContextProperty("schemaController", &schemaController);
    engine.rootContext()->setContextProperty("statisticsController", &statisticsController);
    engine.rootContext()->setContextProperty("exportController", &exportController);
    engine.rootContext()->setContextProperty("stressAnalysisController", &stressAnalysisController);
    engine.rootContext()->setContextProperty("textureController", &textureController);

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

    QObject::connect(&textureController, &TextureController::textureReady,
                     &textureController,
                     [](const std::vector<TextureLibrary::Component>& comps) {
                         Parameters::textureComponents = comps;
                         // Hand-edited from now on: stop the preset dropdown
                         // from rebuilding over the user's components.
                         Parameters::instance()->markTextureCustom();
                     });

    QTimer::singleShot(0, [&mainWindowWrapper, &parser, &schemaController, &stressAnalysisController]() {
        Commandline_Parser::processOptions(parser);

        QString algo = Parameters::instance()->getAlgorithm();
        if (!algo.isEmpty())
            schemaController.onAlgorithmSelected(algo);

        OpenGLWidgetQML *ogl = OpenGLWidgetQML::getInstance();
        if (!ogl) return;

        Parameters *p = Parameters::instance();
        ogl->setNumColors(p->getPoints());   // sync grain colors
        ogl->setNumCubes(p->getSize());      // sync cube size

        if (parser.isSet("autostart"))           // or however you track this flag
            mainWindowWrapper.onStartButton();

        if (parser.isSet("run_stress_calc")) {
            Parameters* p = Parameters::instance();
            const QString mode = p->getStressMode();
            if (mode.compare("single", Qt::CaseInsensitive) == 0) {
                const double* e = p->getStressEps();
                QVariantList eps;
                eps.reserve(6);
                for (int i = 0; i < 6; ++i) eps.append(e[i]);
                stressAnalysisController.runSingleShot(p->getStressSolver(), eps);
            } else if (mode.compare("stiffness", Qt::CaseInsensitive) == 0) {
                // Without this branch --stress_mode stiffness fell through to
                // runDataset() and silently ran the full ~450-solve pipeline.
                // The controller saves the result once the run finishes.
                QObject::connect(&stressAnalysisController,
                                 &StressAnalysisController::stiffnessChanged,
                                 &stressAnalysisController,
                                 [&stressAnalysisController]() {
                                     stressAnalysisController.saveStiffnessResult();
                                 },
                                 static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::SingleShotConnection));
                stressAnalysisController.runStiffnessMatrix(p->getStressSolver());
            } else {
                stressAnalysisController.runDataset(p->getStressSolver());
            }
        }
    });


    return app.exec();
}

