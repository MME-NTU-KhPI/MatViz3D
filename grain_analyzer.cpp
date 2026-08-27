#include "grain_analyzer.h"
#include "tensormath.hpp"
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <vector>
#include <queue>
#include <array>
#include <tuple>
#include <unordered_map>

namespace {

struct GrainAccumulator {
    int64_t sum_x = 0;
    int64_t sum_y = 0;
    int64_t sum_z = 0;
    int64_t sum_xx = 0;
    int64_t sum_yy = 0;
    int64_t sum_zz = 0;
    int64_t sum_xy = 0;
    int64_t sum_xz = 0;
    int64_t sum_yz = 0;
};

} // namespace

std::map<int32_t, GrainAnalyzer::GrainStats3D>
GrainAnalyzer::analyze3D(int32_t*** voxels, int numCubes)
{
    std::map<int32_t, GrainStats3D> result;
    std::unordered_map<int32_t, GrainAccumulator> accum;

    // ── Pass 1: Volume, surface area, and spatial coordinate moments ─────
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

                auto& acc = accum[id];
                acc.sum_x += x;
                acc.sum_y += y;
                acc.sum_z += z;
                acc.sum_xx += static_cast<int64_t>(x) * x;
                acc.sum_yy += static_cast<int64_t>(y) * y;
                acc.sum_zz += static_cast<int64_t>(z) * z;
                acc.sum_xy += static_cast<int64_t>(x) * y;
                acc.sum_xz += static_cast<int64_t>(x) * z;
                acc.sum_yz += static_cast<int64_t>(y) * z;

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

    // ── Pass 2: Derived geometric & 3D inertia tensor metrics ────────────
    double max_vol = 0.0;
    for (const auto& [id, s] : result)
        if (s.volume > max_vol) max_vol = s.volume;

    for (auto& [id, s] : result)
    {
        s.esr = std::cbrt((3.0 * s.volume) / (4.0 * M_PI));
        s.norm_volume = (max_vol > 0.0) ? s.volume / max_vol : 0.0;

        const auto& acc = accum[id];
        const double V = static_cast<double>(s.volume);
        const double cx = static_cast<double>(acc.sum_x) / V;
        const double cy = static_cast<double>(acc.sum_y) / V;
        const double cz = static_cast<double>(acc.sum_z) / V;

        // Central second moments
        const double mu_xx = static_cast<double>(acc.sum_xx) - V * cx * cx;
        const double mu_yy = static_cast<double>(acc.sum_yy) - V * cy * cy;
        const double mu_zz = static_cast<double>(acc.sum_zz) - V * cz * cz;
        const double mu_xy = static_cast<double>(acc.sum_xy) - V * cx * cy;
        const double mu_xz = static_cast<double>(acc.sum_xz) - V * cx * cz;
        const double mu_yz = static_cast<double>(acc.sum_yz) - V * cy * cz;

        // Inertia tensor components (I = Tr(mu)*E - mu)
        s.Ixx = mu_yy + mu_zz;
        s.Iyy = mu_xx + mu_zz;
        s.Izz = mu_xx + mu_yy;
        s.Ixy = -mu_xy;
        s.Ixz = -mu_xz;
        s.Iyz = -mu_yz;

        // Symmetric 3x3 eigensolver from tensormath.hpp (cyclic Jacobi method)
        const mvt::Sym3 inertiaTensor{ s.Ixx, s.Iyy, s.Izz, s.Ixy, s.Iyz, s.Ixz };
        const mvt::Eig3 eig = mvt::eigenSym3(inertiaTensor);

        s.I1 = std::max(0.0, eig.lambda[0]);
        s.I2 = std::max(0.0, eig.lambda[1]);
        s.I3 = std::max(0.0, eig.lambda[2]);

        s.moment_inertia = (s.I1 + s.I2 + s.I3) / 3.0;

        // Equivalent ellipsoid semi-axes
        const double a2 = (5.0 / (2.0 * V)) * (s.I2 + s.I3 - s.I1);
        const double b2 = (5.0 / (2.0 * V)) * (s.I1 + s.I3 - s.I2);
        const double c2 = (5.0 / (2.0 * V)) * (s.I1 + s.I2 - s.I3);

        s.semi_a = std::sqrt(std::max(0.01, a2));
        s.semi_b = std::sqrt(std::max(0.01, b2));
        s.semi_c = std::sqrt(std::max(0.01, c2));

        if (s.semi_a < s.semi_b) std::swap(s.semi_a, s.semi_b);
        if (s.semi_b < s.semi_c) std::swap(s.semi_b, s.semi_c);
        if (s.semi_a < s.semi_b) std::swap(s.semi_a, s.semi_b);

        s.aspect_ratio = s.semi_a / std::max(1e-4, s.semi_c);
        s.sphericity_inertia = s.semi_c / std::max(1e-4, s.semi_a);

        const double I_mean = s.moment_inertia;
        const double num_fa = std::pow(s.I1 - I_mean, 2) + std::pow(s.I2 - I_mean, 2) + std::pow(s.I3 - I_mean, 2);
        const double den_fa = s.I1 * s.I1 + s.I2 * s.I2 + s.I3 * s.I3;
        s.fractional_anisotropy = (den_fa > 1e-12) ? std::sqrt(1.5 * num_fa / den_fa) : 0.0;
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
    out << "grain_id;volume;esr;norm_volume;surface_area;moment_inertia;Ixx;Iyy;Izz;Ixy;Ixz;Iyz;I1;I2;I3;semi_a;semi_b;semi_c;aspect_ratio;sphericity_inertia;fractional_anisotropy\n";

    for (const auto& [id, s] : stats)
    {
        out << id             << ";"
            << s.volume       << ";"
            << s.esr          << ";"
            << s.norm_volume  << ";"
            << s.surface_area << ";"
            << s.moment_inertia << ";"
            << s.Ixx          << ";"
            << s.Iyy          << ";"
            << s.Izz          << ";"
            << s.Ixy          << ";"
            << s.Ixz          << ";"
            << s.Iyz          << ";"
            << s.I1           << ";"
            << s.I2           << ";"
            << s.I3           << ";"
            << s.semi_a       << ";"
            << s.semi_b       << ";"
            << s.semi_c       << ";"
            << s.aspect_ratio << ";"
            << s.sphericity_inertia << ";"
            << s.fractional_anisotropy << "\n";
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
