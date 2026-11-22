#include <QSurfaceFormat>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QWindow>
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
#include "hdf5projectcontroller.h"
#include "datasetbrowsercontroller.h"
#include "neuralgeneratorcontroller.h"
#include "propertyselectorcontroller.h"
#include "exportcontroller.h"
#include "tensormath_selftest.hpp"
#include <hdf5.h>
#include <vector>

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
    // Silence HDF5 library built-in error dumps to stderr.
    // Probing non-existent links/datasets is handled explicitly by return values.
    H5Eset_auto(H5E_DEFAULT, NULL, NULL);

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#ifdef Q_OS_MACOS
    // Qt >= 6.8 turns Qt Quick Controls menus into native macOS menus: the
    // in-window File / Window menu bars (custom-drawn in MainWindow.qml)
    // vanish and their blank-titled menus land in the system menu bar.
    // Keep the menus inside the window, as on Windows and Linux.
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeMenuBar);
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeMenuWindows);
#endif
#endif
#ifdef QT_DEBUG
    qputenv("QML_DISABLE_DISK_CACHE", "1");
#endif

    // Save full original command-line arguments for our own QCommandLineParser
    // and ConfigDispatcher.
    QStringList originalArgs;
    originalArgs.reserve(argc);
    for (int i = 0; i < argc; ++i) {
        originalArgs.append(QString::fromLocal8Bit(argv[i]));
    }

    // Isolate --config / -c from Qt/KConfig machinery:
    // KDE Plasma's platform theme (KConfig) inspects QCoreApplication::arguments()
    // on startup for '--config'. If present, it attempts to parse the user's config
    // file as a KConfig INI file, generating invalid-entry errors.
    // By filtering --config out of the argv passed to QApplication, KConfig never
    // sees it, while our QCommandLineParser consumes it from originalArgs.
    std::vector<char*> clean_argv;
    clean_argv.reserve(argc + 1);
    bool afterDoubleDash = false;
    for (int i = 0; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (!afterDoubleDash && arg == "--") {
            afterDoubleDash = true;
            clean_argv.push_back(argv[i]);
        } else if (!afterDoubleDash && (arg == "--config" || arg == "-c")) {
            if (i + 1 < argc) {
                ++i; // Skip config file path argument
            }
        } else if (!afterDoubleDash && (arg.startsWith("--config=") || arg.startsWith("-c="))) {
            // Skip combined flag
        } else {
            clean_argv.push_back(argv[i]);
        }
    }
    int clean_argc = static_cast<int>(clean_argv.size());
    clean_argv.push_back(nullptr);

    QApplication app(clean_argc, clean_argv.data());

    QApplication::setApplicationName("MatViz3D");
    QApplication::setApplicationVersion("3.0.0");

    // ── Parse CLI early so --help / --version exit before any UI is built,
    //    and so --nogui can be checked before loading QML at all.
    QCommandLineParser parser;
    Commandline_Parser::setupParser(parser);
    parser.process(originalArgs);   // exits here if --help / --version

    if (parser.isSet("help-json")) {
        Commandline_Parser::printJsonHelp();
        return 0;
    }

    // ── Optional: tensor math known-answer tests ──────────────────────────
    //    Opt-in via MATVIZ_TENSOR_SELFTEST=1 so it never costs anything in a
    //    normal run.  Exits non-zero on failure so it can gate a build:
    //        MATVIZ_TENSOR_SELFTEST=1 MatViz3D --nogui --autostart
    if (qEnvironmentVariable("MATVIZ_TENSOR_SELFTEST") == QLatin1String("1")) {
        if (!mvt::selfTest())
            return 1;
    }

    // ── Process configuration file and command-line options ───────────────
    if (!Commandline_Parser::processOptions(parser)) {
        return 1;
    }

    // ── Optional: skip QML entirely in headless mode ──────────────────────
    if (parser.isSet("nogui")) {
        if (!parser.isSet("autostart")) return 0;

        MainWindowAlgorithmHandler handler;
        handler.runAlgorithm(Parameters::instance()->getAlgorithm());
        if (!Parameters::csv_output.isEmpty()) {
            QString err;
            if (!ExportController::writeVoxelsCSV(Parameters::csv_output, Parameters::voxels,
                                                  Parameters::instance()->getSize(), &err)) {
                qCritical().noquote() << "CSV export failed:" << err;
                return 1;
            }
            qDebug().noquote() << "CSV saved:" << Parameters::csv_output;
        }

        // A structure that cannot be printed is not worth a stress solve: keep
        // the geometry and its report, say why, and signal the caller.
        if (Parameters::instance()->getRequirePrintable() && Parameters::printability.computed && !Parameters::printability.ok) {
            qWarning().noquote() << "Structure rejected by the printability check:"
                                 << QString::fromStdString(Parameters::printability.verdict);
            if (parser.isSet("output")) {
                QString err;
                if (!ExportController::writeGeometryHDF5(Parameters::filename, Parameters::voxels,
                                                         Parameters::instance()->getSize(), &err))
                    qCritical().noquote() << "HDF5 export failed:" << err;
            }
            return 3;
        }

        if (parser.isSet("run_stress_calc")) {
            handler.runStressCalculation();
        } else if (parser.isSet("output")) {
            // No solve requested: still persist the structure, so a
            // generation-only dataset run leaves a file behind.
            QString err;
            if (!ExportController::writeGeometryHDF5(Parameters::filename, Parameters::voxels,
                                                     Parameters::instance()->getSize(), &err)) {
                qCritical().noquote() << "HDF5 export failed:" << err;
                return 1;
            }
        }

        return 0;
    }

    // Banner is only printed when launching interactive GUI and --nologo is not set
    if (!parser.isSet("nologo")) {
        Logo::print(Logo::Style::Full);
    }

    // Install before engine
    qInstallMessageHandler(ConsoleLogger::messageHandler);
     // ── UI setup ──────────────────────────────────────────────────────────
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
#ifdef Q_OS_MACOS
    {
        // macOS: the default is a legacy 2.1 context (GLSL 1.20, no VAO).
        // The renderers need VAOs and GLSL 1.50 (glshaders.hpp), so ask for 4.1 Core Profile.
        QSurfaceFormat fmt = QSurfaceFormat::defaultFormat();
        fmt.setVersion(4, 1);
        fmt.setProfile(QSurfaceFormat::CoreProfile);
        fmt.setDepthBufferSize(24);
        fmt.setStencilBufferSize(8);
        QSurfaceFormat::setDefaultFormat(fmt);
    }
#endif

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
    Hdf5ProjectController hdf5ProjectController;
    DatasetBrowserController datasetBrowserController;
    PropertySelectorController propertySelectorController(&datasetBrowserController);
    NeuralGeneratorController neuralGeneratorController;
    StatisticsController statisticsController;
    ExportController::registerFonts();   // Inter for every exported label
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
    engine.rootContext()->setContextProperty("hdf5ProjectController", &hdf5ProjectController);
    engine.rootContext()->setContextProperty("datasetBrowserController", &datasetBrowserController);
    engine.rootContext()->setContextProperty("propertySelectorController", &propertySelectorController);
    engine.rootContext()->setContextProperty("neuralGeneratorController", &neuralGeneratorController);
    // A decoded cell replaces whatever the HDF5 view had put into the 3D view.
    neuralGeneratorController.setAfterShow([&hdf5ProjectController]() { hdf5ProjectController.invalidate3DSync(); });
    // The browser opens a cell through the HDF5 project controller, which loads
    // it and puts the voxels into the 3D view.
    datasetBrowserController.setCellOpener([&hdf5ProjectController](const QString& path) {
        if (!hdf5ProjectController.openFile(path)) return false;
        if (!hdf5ProjectController.autoSync3D()) hdf5ProjectController.pushTo3DView();
        return true;
    });

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

    if (parser.isSet("browse")) {
        const QString target = parser.value("browse");
        QTimer::singleShot(200, &datasetBrowserController, [&datasetBrowserController, target]() {
            datasetBrowserController.openPath(target);
            datasetBrowserController.requestShow();
        });
    }
    if (parser.isSet("select")) {
        QVariantMap spec;
        for (const QString& item : parser.value("select").split(QLatin1Char(','), Qt::SkipEmptyParts)) {
            const int eq = item.indexOf(QLatin1Char('='));
            if (eq > 0) spec.insert(item.left(eq).trimmed(), item.mid(eq + 1).trimmed().toDouble());
        }
        QTimer::singleShot(400, &propertySelectorController, [&propertySelectorController, spec]() {
            propertySelectorController.requestShow(spec);
        });
    }
    if (parser.isSet("neural")) {
        const quint32 seed = parser.value("neural").toUInt();
        // --neural_compare: solve that cell as soon as it is made, for the "asked / obtained" table.
        if (parser.isSet("neural_compare")) neuralGeneratorController.setCompareNext(true);
        QTimer::singleShot(200, &neuralGeneratorController, [&neuralGeneratorController, seed]() {
            neuralGeneratorController.requestShow(seed);
        });
    }
    // MATVIZ_SNAPSHOT=<prefix>: once the windows have settled, save every visible
    // top-level window as <prefix>_<title>.png and quit (documentation shots and
    // checks of the QML windows without a screen recorder).
    if (qEnvironmentVariableIsSet("MATVIZ_SNAPSHOT")) {
        const QString prefix = qEnvironmentVariable("MATVIZ_SNAPSHOT");
        // MATVIZ_SNAPSHOT_HEIGHT=<px>: make every window that tall first, so a
        // long panel fits in one shot.
        const int tall = qEnvironmentVariableIntValue("MATVIZ_SNAPSHOT_HEIGHT");
        if (tall > 0)
            QTimer::singleShot(3000, &app, [tall]() {
                for (QWindow* w : QGuiApplication::topLevelWindows())
                    if (w->isVisible()) w->resize(w->width(), tall);
            });
        // MATVIZ_SNAPSHOT_DELAY=<ms>: when to take the shot (default 4500), for a
        // window that first has to read a dataset.
        const int delay = qEnvironmentVariableIntValue("MATVIZ_SNAPSHOT_DELAY");
        auto shoot = [prefix]() {
            for (QWindow* w : QGuiApplication::topLevelWindows()) {
                auto* qw = qobject_cast<QQuickWindow*>(w);
                if (!qw || !qw->isVisible()) continue;
                QString name = qw->title().isEmpty() ? QStringLiteral("window") : qw->title();
                name.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9]+")), QStringLiteral("_"));
                qw->grabWindow().save(prefix + QStringLiteral("_") + name + QStringLiteral(".png"));
            }
            QCoreApplication::quit();
        };
        // --neural --neural_compare without a delay: the solve takes minutes, the shot waits for its table.
        if (delay <= 0 && parser.isSet("neural") && parser.isSet("neural_compare"))
            QObject::connect(&neuralGeneratorController, &NeuralGeneratorController::compareFinished, &app,
                             [&app, shoot]() { QTimer::singleShot(1500, &app, shoot); });
        else
            QTimer::singleShot(delay > 0 ? delay : 4500, &app, shoot);
    }

    // MATVIZ_EXPORT=<prefix>: once the scene has settled (MATVIZ_EXPORT_DELAY ms,
    // default 5000), write what Save / Copy produce for the 3D view plus the raw
    // transparent capture (ExportController::debugExport) and quit. MATVIZ_EXPORT_DPI
    // sets the export DPI. The scripted check of the screenshot pipeline.
    if (qEnvironmentVariableIsSet("MATVIZ_EXPORT")) {
        const QString prefix = qEnvironmentVariable("MATVIZ_EXPORT");
        const int dpi = qEnvironmentVariableIntValue("MATVIZ_EXPORT_DPI");
        if (dpi > 0) exportController.setDpi(dpi);
        // MATVIZ_EXPORT_WIDTH_CM=<cm>: a figure of that width at the DPI.
        if (qEnvironmentVariableIsSet("MATVIZ_EXPORT_WIDTH_CM"))
            exportController.setWidthCm(qEnvironmentVariable("MATVIZ_EXPORT_WIDTH_CM").toDouble());
        const int delay = qEnvironmentVariableIntValue("MATVIZ_EXPORT_DELAY");
        QTimer::singleShot(delay > 0 ? delay : 5000, &app, [&exportController, prefix]() {
            exportController.debugExport(OpenGLWidgetQML::getInstance(), prefix,
                                         []() { QCoreApplication::quit(); });
        });
    }

    QTimer::singleShot(0, [&mainWindowWrapper, &parser, &schemaController, &stressAnalysisController]() {
        QString algo = Parameters::instance()->getAlgorithm();
        if (!algo.isEmpty())
            schemaController.onAlgorithmSelected(algo);

        OpenGLWidgetQML *ogl = OpenGLWidgetQML::getInstance();
        if (!ogl) return;

        Parameters *p = Parameters::instance();
        ogl->setNumColors(p->getPoints());   // sync grain colors
        ogl->setNumCubes(p->getSize());      // sync cube size

        // MATVIZ_SHADING=0|1, MATVIZ_AO=<0..1>, MATVIZ_DEPTHCUE=<0..1>: the
        // shading of the 3D view, for scripted before / after shots.
        if (qEnvironmentVariableIsSet("MATVIZ_SHADING"))
            ogl->setShadingModel(qEnvironmentVariableIntValue("MATVIZ_SHADING"));
        if (qEnvironmentVariableIsSet("MATVIZ_AO"))
            ogl->setAoStrength(qEnvironmentVariable("MATVIZ_AO").toDouble());
        if (qEnvironmentVariableIsSet("MATVIZ_DEPTHCUE"))
            ogl->setDepthCue(qEnvironmentVariable("MATVIZ_DEPTHCUE").toDouble());
        // MATVIZ_PALETTE=<0..6>: field colormap (matviz_cmap::Palette ordinal).
        if (qEnvironmentVariableIsSet("MATVIZ_PALETTE"))
            ogl->setColorMapPalette(qEnvironmentVariableIntValue("MATVIZ_PALETTE"));
        // MATVIZ_OUTLINES=0|1|2 (off / voxel edges / grain and pore
        // boundaries), MATVIZ_OUTLINE_WIDTH=<logical px>.
        if (qEnvironmentVariableIsSet("MATVIZ_OUTLINES")) {
            const int kind = qEnvironmentVariableIntValue("MATVIZ_OUTLINES");
            if (kind > 0) ogl->setOutlineKind(kind);
            ogl->setPlotWireFrame(kind > 0);
        }
        if (qEnvironmentVariableIsSet("MATVIZ_OUTLINE_WIDTH"))
            ogl->setOutlineWidth(qEnvironmentVariable("MATVIZ_OUTLINE_WIDTH").toDouble());
        // MATVIZ_ORIENTATIONS=1: the grains' crystal axes (thick lines).
        if (qEnvironmentVariableIntValue("MATVIZ_ORIENTATIONS") == 1)
            ogl->setShowOrientations(true);

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

