#include "grain_analyzer.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>

std::map<int32_t, GrainAnalyzer::GrainStats>
GrainAnalyzer::analyze(int32_t*** voxels, int numCubes)
{
    std::map<int32_t, GrainStats> result;

    // ── Прохід 1: об'єм + поверхня ────────────────────────────────────────
    // 6 напрямків для перевірки меж зерна
    const int dx[] = { 1,-1, 0, 0, 0, 0 };
    const int dy[] = { 0, 0, 1,-1, 0, 0 };
    const int dz[] = { 0, 0, 0, 0, 1,-1 };

    for (int x = 0; x < numCubes; ++x)
        for (int y = 0; y < numCubes; ++y)
            for (int z = 0; z < numCubes; ++z)
            {
                const int32_t id = voxels[x][y][z];
                if (id <= 0) continue;

                GrainStats& s = result[id];
                s.volume++;

                // Поверхня: рахуємо грані що межують з іншим зерном або межею
                for (int d = 0; d < 6; ++d)
                {
                    const int nx = x + dx[d];
                    const int ny = y + dy[d];
                    const int nz = z + dz[d];

                    const bool boundary =
                        (nx < 0 || nx >= numCubes ||
                         ny < 0 || ny >= numCubes ||
                         nz < 0 || nz >= numCubes ||
                         voxels[nx][ny][nz] != id);

                    if (boundary) s.surface_area += 1.0;
                }
            }

    // ── Прохід 2: похідні метрики ──────────────────────────────────────────
    // Знаходимо максимальний об'єм для нормування
    double max_vol = 0.0;
    for (const auto& [id, s] : result)
        if (s.volume > max_vol) max_vol = s.volume;

    for (auto& [id, s] : result)
    {
        // ESR: еквівалентний радіус сфери з тим самим об'ємом
        s.esr = std::cbrt((3.0 * s.volume) / (4.0 * M_PI));

        // Нормований об'єм
        s.norm_volume = (max_vol > 0.0) ? s.volume / max_vol : 0.0;

        // Момент інерції однорідної кулі радіуса ESR
        s.moment_inertia = (2.0 / 5.0) * s.esr * s.esr;
    }

    return result;
}

void GrainAnalyzer::writeToCSV(
    const std::map<int32_t, GrainStats>& stats,
    const QString& filePath)
{
    if (stats.empty())
    {
        qWarning() << "GrainAnalyzer::writeToCSV: stats is empty, nothing to write";
        return;
    }

    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qWarning() << "GrainAnalyzer::writeToCSV: cannot open" << filePath;
        return;
    }

    QTextStream out(&f);
    out << "grain_id;volume;esr;norm_volume;surface_area;moment_inertia\n";

    for (const auto& [id, s] : stats)
    {
        out << id             << ";"
            << s.volume       << ";"
            << s.esr          << ";"
            << s.norm_volume  << ";"
            << s.surface_area << ";"
            << s.moment_inertia << "\n";
    }

    f.close();
    qDebug() << "GrainAnalyzer: written" << stats.size()
             << "grains to" << filePath;
}
