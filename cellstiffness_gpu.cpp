#include "cellstiffness_gpu.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QProcess>
#include <QStandardPaths>
#include <QStringList>
#include <QTemporaryFile>

namespace {

QString findScript()
{
    const QString named = qEnvironmentVariable("MATVIZ_GPU_SCRIPT");
    if (!named.isEmpty()) return QFileInfo::exists(named) ? named : QString();
    const QString file = QStringLiteral("tools/neural/gpu_stiffness.py");
    const QDir app(QCoreApplication::applicationDirPath());
    QStringList list = {
        app.filePath(file),                                     // next to the executable
        app.filePath(QStringLiteral("../Resources/") + file),   // inside a macOS bundle
        app.filePath(QStringLiteral("../../../") + file),       // next to the bundle
    };
#ifdef MATVIZ_SOURCE_DIR
    list << QStringLiteral(MATVIZ_SOURCE_DIR "/") + file;       // the source tree the program was built from
#endif
    for (const QString& candidate : std::as_const(list))
        if (QFileInfo::exists(candidate)) return QDir::cleanPath(candidate);
    return QString();
}

// A program started from the Finder has a bare PATH, so the usual places of a Python with PyTorch are named.
QStringList interpreters()
{
    const QString named = qEnvironmentVariable("MATVIZ_GPU_PYTHON");
    if (!named.isEmpty()) return { named };
    const QString home = QDir::homePath();
    QStringList list = {
        QStringLiteral("/opt/anaconda3/bin/python3"), home + QStringLiteral("/anaconda3/bin/python3"),
        home + QStringLiteral("/miniconda3/bin/python3"), home + QStringLiteral("/miniforge3/bin/python3"),
        QStringLiteral("/opt/homebrew/bin/python3"), QStringLiteral("/usr/local/bin/python3"),
    };
    const QString onPath = QStandardPaths::findExecutable(QStringLiteral("python3"));
    if (!onPath.isEmpty() && !list.contains(onPath)) list << onPath;
    return list;
}

bool readSolution(const QJsonObject& o, cellstiff::Solution& s)
{
    const QJsonArray rows = o.value(QStringLiteral("C")).toArray();
    if (rows.size() != 6) return false;
    for (int i = 0; i < 6; ++i) {
        const QJsonArray row = rows.at(i).toArray();
        if (row.size() != 6) return false;
        for (int j = 0; j < 6; ++j) s.C[size_t(i)][size_t(j)] = row.at(j).toDouble();
    }
    s.voidEta = o.value(QStringLiteral("void_eta")).toDouble();
    s.share = o.value(QStringLiteral("share")).toDouble();
    s.shareFirst = o.value(QStringLiteral("share_first")).toDouble(s.share);
    s.iterations = o.value(QStringLiteral("iterations")).toInt();
    s.passes = o.value(QStringLiteral("passes")).toInt(1);
    s.converged = o.value(QStringLiteral("converged")).toBool(true);
    s.ok = true;
    return true;
}

} // namespace

namespace cellstiff {

GpuSolver gpuSolver()
{
    static QMutex mutex;
    static bool looked = false;
    static GpuSolver found;
    QMutexLocker lock(&mutex);
    if (looked) return found;
    looked = true;
    if (qEnvironmentVariable("MATVIZ_GPU_SOLVER") == QLatin1String("0")) {
        found.reason = QStringLiteral("turned off by MATVIZ_GPU_SOLVER=0");
        return found;
    }
    const QString script = findScript();
    if (script.isEmpty()) {
        found.reason = QStringLiteral("tools/neural/gpu_stiffness.py was not found");
        return found;
    }
    QStringList tried;
    const QStringList list = interpreters();
    for (const QString& python : list) {
        if (!QFileInfo::exists(python)) continue;
        QProcess probe;
        probe.start(python, { script, QStringLiteral("--probe") });
        if (!probe.waitForStarted(5000) || !probe.waitForFinished(60000)) {
            probe.kill();
            probe.waitForFinished(2000);
            tried << python + QStringLiteral(": no answer");
            continue;
        }
        const QString out = QString::fromUtf8(probe.readAllStandardOutput()).trimmed();
        if (out.startsWith(QLatin1String("OK "))) {
            found.python = python;
            found.script = script;
            found.device = out.mid(3).trimmed();
            return found;
        }
        tried << python + QStringLiteral(": ") + (out.isEmpty() ? QStringLiteral("no PyTorch") : out);
    }
    found.reason = tried.isEmpty() ? QStringLiteral("no Python interpreter was found")
                                   : QStringLiteral("no interpreter with PyTorch and a GPU (%1)").arg(tried.join(QStringLiteral("; ")));
    return found;
}

Solution solveOnGpu(const GpuSolver& gpu, const std::vector<uint8_t>& solid, int n, const propsearch::Law& material,
                    const Settings& settings, const Progress& progress, const FirstPass& firstPass, const std::atomic<bool>* cancel)
{
    Solution s;
    s.backend = gpu.device.toStdString();
    QElapsedTimer clock;
    clock.start();
    const size_t count = size_t(n) * size_t(n) * size_t(n);
    if (n < 2 || solid.size() != count) {
        s.error = "the cell is not an n x n x n field";
        return s;
    }
    size_t filled = 0;
    for (uint8_t v : solid) filled += v ? 1 : 0;
    s.density = double(filled) / double(count);
    if (!gpu.available()) {
        s.error = gpu.reason.toStdString();
        return s;
    }

    QTemporaryFile file(QDir::temp().filePath(QStringLiteral("matviz_cell_XXXXXX.u8")));
    if (!file.open() || file.write(reinterpret_cast<const char*>(solid.data()), qint64(count)) != qint64(count)) {
        s.error = "the cell could not be written for the GPU solver";
        return s;
    }
    file.close();   // the file stays until `file` goes out of scope

    const auto number = [](double v) { return QString::number(v, 'g', 17); };
    QProcess process;
    process.start(gpu.python, { gpu.script, QStringLiteral("--cell"), file.fileName(), QStringLiteral("--n"), QString::number(n),
                                QStringLiteral("--e_gpa"), number(material.eGpa), QStringLiteral("--nu"), number(material.nu),
                                QStringLiteral("--aniso"), number(material.aniso), QStringLiteral("--tol"), number(settings.tol),
                                QStringLiteral("--max_iter"), QString::number(settings.maxIter), QStringLiteral("--void"), number(settings.voidEta),
                                QStringLiteral("--void_fallback"), number(settings.voidEtaFallback), QStringLiteral("--min_share"), number(settings.minSolidShare) });
    if (!process.waitForStarted(10000)) {
        s.error = "the GPU solver did not start: " + process.errorString().toStdString();
        return s;
    }

    Solution first;
    bool haveFirst = false, answered = false;
    QByteArray buffer;
    for (;;) {
        if (cancel && cancel->load()) {
            process.kill();
            process.waitForFinished(3000);
            if (haveFirst) {            // the second solve was stopped: the first one's stiffness stands
                first.cancelled = true;
                first.seconds = clock.elapsed() / 1000.0;
                return first;
            }
            s.cancelled = true;
            s.seconds = clock.elapsed() / 1000.0;
            return s;
        }
        const bool running = process.state() != QProcess::NotRunning;
        if (running) process.waitForReadyRead(100);
        buffer += process.readAllStandardOutput();
        qsizetype end;
        while ((end = buffer.indexOf('\n')) >= 0) {
            const QByteArray line = buffer.left(end).trimmed();
            buffer.remove(0, end + 1);
            if (line.startsWith("P ")) {
                const QList<QByteArray> part = line.split(' ');
                if (progress && part.size() >= 5)
                    progress(part.at(1).toInt(), -(1 + part.at(4).toInt()), part.at(2).toInt(), part.at(3).toDouble());
            } else if (line.startsWith("F ") || line.startsWith("R ")) {
                Solution t;
                t.backend = s.backend;
                t.density = s.density;
                if (!readSolution(QJsonDocument::fromJson(line.mid(2)).object(), t)) {
                    s.error = "the GPU solver's answer could not be read";
                    continue;
                }
                if (line.startsWith("F ")) {
                    first = t;
                    first.passes = 1;
                    first.seconds = clock.elapsed() / 1000.0;
                    haveFirst = true;
                    if (firstPass) firstPass(first);
                } else {
                    s = t;
                    answered = true;
                }
            } else if (line.startsWith("E ")) {
                s.error = line.mid(2).toStdString();
            }
        }
        if (!running && process.bytesAvailable() == 0) break;
    }
    process.waitForFinished(3000);
    s.seconds = clock.elapsed() / 1000.0;
    if (!answered) {
        s.ok = false;
        if (s.error.empty()) {
            const QString tail = QString::fromUtf8(process.readAllStandardError()).trimmed().right(300);
            s.error = "the GPU solver ended without an answer" + (tail.isEmpty() ? std::string() : ": " + tail.toStdString());
        }
    }
    return s;
}

} // namespace cellstiff
