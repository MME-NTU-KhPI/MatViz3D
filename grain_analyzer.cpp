#include "grain_analyzer.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <vector>
#include <queue>
#include <array>
#include <tuple>
#include <unordered_map>

std::map<int32_t, GrainAnalyzer::GrainStats3D>
GrainAnalyzer::analyze3D(int32_t*** voxels, int numCubes)
{
    std::map<int32_t, GrainStats3D> result;

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

                GrainStats3D& s = result[id];
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

// ═══════════════════════════════════════════════════════════════════════════
// 2D layer analysis
// ═══════════════════════════════════════════════════════════════════════════

namespace {

struct Point2D { int x, y; };

// Перевірка: піксель (i,j) лежить на межі зерна → дає внесок 1 у периметр
int compute_perimeter(const std::vector<std::vector<int>>& img, int i, int j)
{
    const int rows  = img.size();
    const int cols  = img[0].size();
    const int color = img[i][j];

    const std::array<Point2D, 4> nb = {{ {i-1,j},{i+1,j},{i,j-1},{i,j+1} }};
    for (const auto& p : nb)
        if (p.x < 0 || p.x >= rows || p.y < 0 || p.y >= cols
            || img[p.x][p.y] != color)
            return 1;
    return 0;
}

// BFS-розмітка зв'язних областей одного шару
std::vector<GrainAnalyzer::GrainStats2D>
label_connected_regions(const std::vector<std::vector<int>>& image)
{
    const int rows        = image.size();
    const int cols        = image[0].size();
    const double totalArea = static_cast<double>(rows) * cols;
    constexpr double pi   = 3.14159265358979;

    std::vector<std::vector<bool>> visited(rows, std::vector<bool>(cols, false));
    std::vector<GrainAnalyzer::GrainStats2D> objects;

    for (int i = 0; i < rows; ++i)
        for (int j = 0; j < cols; ++j)
        {
            if (!image[i][j] || visited[i][j]) continue;

            int size = 0, perimeter = 0;
            const int color = image[i][j];
            std::queue<Point2D> q;
            q.push({i, j});
            visited[i][j] = true;

            while (!q.empty())
            {
                auto [ci, cj] = q.front(); q.pop();
                size++;
                perimeter += compute_perimeter(image, ci, cj);

                for (const auto& nb : std::array<Point2D,4>
                     {{ {ci-1,cj},{ci+1,cj},{ci,cj-1},{ci,cj+1} }})
                {
                    if (nb.x >= 0 && nb.x < rows &&
                        nb.y >= 0 && nb.y < cols &&
                        image[nb.x][nb.y] == color && !visited[nb.x][nb.y])
                    {
                        visited[nb.x][nb.y] = true;
                        q.push(nb);
                    }
                }
            }

            GrainAnalyzer::GrainStats2D o;
            o.label        = color;
            o.size         = size;
            o.perimeter    = perimeter;
            o.norm_area    = static_cast<double>(size) / totalArea;
            o.ecr          = std::sqrt(o.norm_area / pi);
            o.shape_factor = (perimeter > 0)
                                 ? 4.0 * pi * size / (static_cast<double>(perimeter) * perimeter)
                                 : 0.0;
            objects.push_back(o);
        }
    return objects;
}

} // anonymous namespace

std::vector<GrainAnalyzer::GrainStats2D>
GrainAnalyzer::analyze2D(int32_t*** voxels, int numCubes)
{
    std::vector<GrainStats2D> all;

    for (int z = 0; z < numCubes; ++z)
    {
        std::vector<std::vector<int>> layer(numCubes,
                                            std::vector<int>(numCubes));
        for (int y = 0; y < numCubes; ++y)
            for (int x = 0; x < numCubes; ++x)
                layer[y][x] = voxels[x][y][z];

        auto objects = label_connected_regions(layer);
        all.insert(all.end(), objects.begin(), objects.end());
    }
    return all;
}

void GrainAnalyzer::writeToCSV3D(
    const std::map<int32_t, GrainStats3D>& stats,
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

void GrainAnalyzer::writeToCSV2D(
    const std::vector<GrainStats2D>& stats,
    const QString& filePath)
{
    if (stats.empty())
    {
        qWarning() << "GrainAnalyzer::writeToCSV2D: stats is empty, nothing to write";
        return;
    }

    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qWarning() << "GrainAnalyzer::writeToCSV2D: cannot open" << filePath;
        return;
    }

    QTextStream out(&f);
    out << "label;size;perimeter;norm_area;ecr;shape_factor\n";

    for (const auto& s : stats)
    {
        out << s.label        << ";"
            << s.size         << ";"
            << s.perimeter    << ";"
            << s.norm_area    << ";"
            << s.ecr          << ";"
            << s.shape_factor << "\n";
    }

    f.close();
    qDebug() << "GrainAnalyzer: written" << stats.size()
             << "2D regions to" << filePath;
}
