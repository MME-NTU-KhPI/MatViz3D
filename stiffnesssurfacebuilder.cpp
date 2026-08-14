#include "stiffnesssurfacebuilder.h"

#include <algorithm>
#include <cmath>

namespace mvsurf {

namespace {

/// Pa -> GPa for the pipeline-basis input, so every quantity the UI shows is in
/// one unit system regardless of which source the matrix came from.
constexpr double kPaToGPa = 1e-9;

/// Evaluate the selected quantity along one direction.
double evaluate(const ElasticState& st, const Params& p, const mvt::Vec3& n)
{
    switch (p.quantity) {
        case Quantity::YoungsE:
            return mvt::youngsE(st.S4s, n);

        case Quantity::ShearGmin:
        case Quantity::ShearGmax:
        case Quantity::ShearGmean: {
            double lo = 0, hi = 0, mean = 0;
            mvt::shearG(st.S4s, n, p.nAzimuth, lo, hi, mean);
            if (p.quantity == Quantity::ShearGmin)  return lo;
            if (p.quantity == Quantity::ShearGmax)  return hi;
            return mean;
        }

        case Quantity::PoissonMin:
        case Quantity::PoissonMax:
        case Quantity::PoissonMean: {
            double lo = 0, hi = 0, mean = 0;
            mvt::poissonNu(st.S4s, n, p.nAzimuth, lo, hi, mean);
            if (p.quantity == Quantity::PoissonMin) return lo;
            if (p.quantity == Quantity::PoissonMax) return hi;
            return mean;
        }

        case Quantity::LinearCompressibility:
            return mvt::linearCompressibility(st.S4s, n);

        case Quantity::C_component:
        case Quantity::S_component: {
            const mvt::C4& T = (p.quantity == Quantity::C_component) ? st.C4s : st.S4s;

            // mvt::directionalComponent carries the crystal axes to the plotted
            // direction with a twist-free rotation, in Voigt convention, so at
            // the component's pivot direction with zero twist the radius equals
            // the tabulated entry.
            //
            // Single-axis components (C11/C22/C33) reduce to the n-n-n-n
            // contraction: the twist cannot change them, so all four modes give
            // the same surface and there is nothing to resolve.
            if (mvt::componentIsSpinFree(p.ci, p.cj))
                return mvt::directionalComponent(T, n, 0.0, p.ci, p.cj);

            if (p.twist == mvsurf::TwistMode::Fixed)
                return mvt::directionalComponent(T, n, p.spinDeg * mvt::kPi / 180.0,
                                                 p.ci, p.cj);

            // Full 2*pi, not pi: components with an odd number of in-plane
            // factors (C16) change sign under a half turn, so a pi sweep would
            // miss half the range and bias the mean.
            const int kSpins = 72;
            double lo = 1e300, hi = -1e300, sum = 0.0;
            for (int k = 0; k < kSpins; ++k) {
                const double spin = 2.0 * mvt::kPi * double(k) / double(kSpins);
                const double v = mvt::directionalComponent(T, n, spin, p.ci, p.cj);
                lo = std::min(lo, v);
                hi = std::max(hi, v);
                sum += v;
            }
            if (p.twist == mvsurf::TwistMode::Min) return lo;
            if (p.twist == mvsurf::TwistMode::Max) return hi;
            return sum / double(kSpins);
        }
    }
    return 0.0;
}

} // namespace

bool buildElasticState(const double M[6][6], Basis basis, ElasticState& out)
{
    out = ElasticState{};

    double scaled[6][6];
    const double k = (basis == Basis::PipelinePa) ? kPaToGPa : 1.0;
    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j)
            scaled[i][j] = M[i][j] * k;

    out.Cm = (basis == Basis::PipelinePa) ? mvt::pipelineC_to_mandel(scaled)
                                          : mvt::voigtC_to_mandel(scaled);

    // A brand new database row is all zeros; a hand-edited one can be singular.
    if (!mvt::invertMat6(out.Cm, out.Sm))
        return false;

    out.C4s = mvt::mandel_to_C4(out.Cm);
    out.S4s = mvt::mandel_to_C4(out.Sm);

    out.cubic = mvt::detectCubic(out.Cm, out.c11, out.c12, out.c44);
    out.zener = out.cubic ? mvt::zener(out.c11, out.c12, out.c44) : 1.0;
    out.bulkVRH = mvt::vrhBulkModulus(out.Cm, out.Sm);

    out.valid = true;
    return true;
}

Mesh buildSurface(const ElasticState& state, const Params& params)
{
    Mesh mesh;
    if (!state.valid) return mesh;

    const int nT = std::max(4, params.nTheta);
    const int nP = std::max(6, params.nPhi);

    // The phi seam vertex is duplicated (j == 0 and j == nP describe the same
    // direction) so colours and normals stay correct across the wrap.
    const int rows = nT + 1, cols = nP + 1;

    std::vector<double> value(size_t(rows) * size_t(cols), 0.0);
    std::vector<mvt::Vec3> dir(size_t(rows) * size_t(cols));

    double vMin = 1e300, vMax = -1e300, absMax = 0.0;

    for (int i = 0; i < rows; ++i) {
        const double theta = mvt::kPi * double(i) / double(nT);
        for (int j = 0; j < cols; ++j) {
            const double phi = 2.0 * mvt::kPi * double(j % nP) / double(nP);
            const mvt::Vec3 n = mvt::sphereDir(theta, phi);
            const double v = evaluate(state, params, n);
            const size_t idx = size_t(i) * size_t(cols) + size_t(j);
            dir[idx] = n;
            value[idx] = v;
            vMin = std::min(vMin, v);
            vMax = std::max(vMax, v);
            absMax = std::max(absMax, std::abs(v));
        }
    }

    if (absMax < 1e-300) return mesh;   // identically zero: nothing to draw

    mesh.minValue = vMin;
    mesh.maxValue = vMax;
    mesh.unit = quantityUnit(params.quantity);

    double absMin = 1e300;
    for (double v : value) absMin = std::min(absMin, std::abs(v));
    mesh.anisotropyRatio = (absMin > 1e-300) ? absMax / absMin : 0.0;

    // Colour normalization: symmetric about zero when the quantity changes sign
    // (Poisson's ratio and rotated components do), plain min..max otherwise.
    const bool signed_ = (vMin < 0.0 && vMax > 0.0);
    const auto cmap = matviz_cmap::createColorMap(params.colorLevels, params.palette);

    auto colorFor = [&](double v) {
        float t;
        if (signed_)               t = float(0.5 * (1.0 + v / absMax));
        else if (vMax - vMin > 0)  t = float((v - vMin) / (vMax - vMin));
        else                       t = 0.5f;
        return matviz_cmap::scalarToColor(t, cmap);
    };

    // Positions.
    std::vector<std::array<double, 3>> pos(size_t(rows) * size_t(cols));
    for (size_t idx = 0; idx < pos.size(); ++idx) {
        const double r = std::abs(value[idx]) / absMax * params.worldRadius;
        pos[idx] = { dir[idx][0] * r, dir[idx][1] * r, dir[idx][2] * r };
    }

    // Normals: accumulate face normals into the corner vertices, then
    // normalize. The surface is a general radial function (a rotated stiffness
    // component has no closed-form normal), so finite geometry is the honest
    // way to get them; on a 48x96 grid it is smooth enough for the lit shader.
    std::vector<std::array<double, 3>> nrm(pos.size(), { 0.0, 0.0, 0.0 });

    auto accumulate = [&](size_t a, size_t b, size_t c) {
        const double e1[3] = { pos[b][0]-pos[a][0], pos[b][1]-pos[a][1], pos[b][2]-pos[a][2] };
        const double e2[3] = { pos[c][0]-pos[a][0], pos[c][1]-pos[a][1], pos[c][2]-pos[a][2] };
        const double f[3] = { e1[1]*e2[2] - e1[2]*e2[1],
                              e1[2]*e2[0] - e1[0]*e2[2],
                              e1[0]*e2[1] - e1[1]*e2[0] };
        for (int k = 0; k < 3; ++k) {
            nrm[a][k] += f[k]; nrm[b][k] += f[k]; nrm[c][k] += f[k];
        }
    };

    mesh.indices.reserve(size_t(nT) * size_t(nP) * 6);
    for (int i = 0; i < nT; ++i) {
        for (int j = 0; j < nP; ++j) {
            const uint32_t a = uint32_t(i * cols + j);
            const uint32_t b = uint32_t((i + 1) * cols + j);
            const uint32_t c = uint32_t((i + 1) * cols + (j + 1));
            const uint32_t d = uint32_t(i * cols + (j + 1));

            // Winding: +theta then +phi gives cross(dTheta, dPhi) pointing
            // outward, i.e. counter-clockwise seen from outside the surface.
            mesh.indices.push_back(a); mesh.indices.push_back(b); mesh.indices.push_back(c);
            mesh.indices.push_back(a); mesh.indices.push_back(c); mesh.indices.push_back(d);
            accumulate(a, b, c);
            accumulate(a, c, d);
        }
    }

    // Stitch the seam: vertices at j == 0 and j == nP are the same point, so
    // average their accumulated normals to avoid a visible crease.
    for (int i = 0; i < rows; ++i) {
        const size_t a = size_t(i) * size_t(cols);
        const size_t b = a + size_t(nP);
        for (int k = 0; k < 3; ++k) {
            const double s = nrm[a][k] + nrm[b][k];
            nrm[a][k] = s; nrm[b][k] = s;
        }
    }

    mesh.verts.resize(pos.size());
    for (size_t idx = 0; idx < pos.size(); ++idx) {
        GlVertex& v = mesh.verts[idx];
        v.x = float(pos[idx][0]);
        v.y = float(pos[idx][1]);
        v.z = float(pos[idx][2]);

        const auto col = colorFor(value[idx]);
        v.r = col[0]; v.g = col[1]; v.b = col[2]; v.a = 255;

        double nx = nrm[idx][0], ny = nrm[idx][1], nz = nrm[idx][2];
        const double len = std::sqrt(nx*nx + ny*ny + nz*nz);
        if (len > 1e-300) { nx /= len; ny /= len; nz /= len; }
        else { nx = dir[idx][0]; ny = dir[idx][1]; nz = dir[idx][2]; }   // degenerate: fall back radial
        glVertexSetNormal(v, float(nx), float(ny), float(nz));
    }

    mesh.valid = true;
    return mesh;
}

const char* quantityName(Quantity q)
{
    switch (q) {
        case Quantity::YoungsE:               return "Young's modulus E";
        case Quantity::ShearGmin:             return "Shear modulus G (min)";
        case Quantity::ShearGmax:             return "Shear modulus G (max)";
        case Quantity::ShearGmean:            return "Shear modulus G (mean)";
        case Quantity::PoissonMin:            return "Poisson's ratio (min)";
        case Quantity::PoissonMax:            return "Poisson's ratio (max)";
        case Quantity::PoissonMean:           return "Poisson's ratio (mean)";
        case Quantity::LinearCompressibility: return "Linear compressibility";
        case Quantity::C_component:           return "C component";
        case Quantity::S_component:           return "S component";
    }
    return "";
}

const char* quantityUnit(Quantity q)
{
    switch (q) {
        case Quantity::YoungsE:
        case Quantity::ShearGmin:
        case Quantity::ShearGmax:
        case Quantity::ShearGmean:
        case Quantity::C_component:           return "GPa";
        case Quantity::LinearCompressibility:
        case Quantity::S_component:           return "1/GPa";
        case Quantity::PoissonMin:
        case Quantity::PoissonMax:
        case Quantity::PoissonMean:           return "-";
    }
    return "";
}

} // namespace mvsurf
