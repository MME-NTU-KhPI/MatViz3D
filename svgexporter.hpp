#ifndef SVGEXPORTER_HPP
#define SVGEXPORTER_HPP
// svgexporter.hpp — true-vector SVG export of the voxel scene.
// Free functions, no class, no GL calls.
#include <QMatrix4x4>
#include <QVector4D>
#include <QVector3D>
#include <QPointF>
#include <QColor>
#include <QFile>
#include <QTextStream>
#include <algorithm>
#include <cmath>
#include <vector>
#include "glvertex.hpp"

namespace svgx {

struct Face { std::vector<GlVertex> v; };          // one flat face (3-4 pts, single color)
struct ProjFace { std::vector<QPointF> pts; float depth; QRgb color; };

// Project a world point through mvp into pixels. false = behind the camera.
inline bool project(const QMatrix4x4& mvp, const GlVertex& g,
                    int w, int h, QPointF& out, float& zOut)
{
    const QVector4D clip = mvp * QVector4D(g.x, g.y, g.z, 1.0f);
    if (clip.w() <= 0.0f) return false;                       // behind eye -> drop
    out.setX((clip.x() / clip.w() * 0.5f + 0.5f) * w);        // NDC -> px, X
    out.setY((clip.y() / clip.w() * 0.5f + 0.5f) * h);        // Quick FBO already flips Y
    zOut = clip.z() / clip.w();
    return true;
}

// Flat per-face shading so the three cube sides read as distinct.
inline QRgb shadeFace(const GlVertex& c)
{
    QVector3D n(c.nx / 127.0f, c.ny / 127.0f, c.nz / 127.0f);
    static const QVector3D L = QVector3D(1, 1, 1).normalized();
    const float diff = std::max(0.0f, QVector3D::dotProduct(n.normalized(), L));
    const float k = 0.55f + 0.45f * diff;
    auto ch = [k](GLubyte v){ return GLubyte(std::clamp(v * k, 0.0f, 255.0f)); };
    return qRgba(ch(c.r), ch(c.g), ch(c.b), c.a);
}

// Build faces from the flat quad buffer: 4 verts per GL_TRIANGLE_FAN.
inline std::vector<Face> facesFromQuadBuffer(const std::vector<GlVertex>& buf)
{
    std::vector<Face> faces;
    faces.reserve(buf.size() / 4);
    for (size_t i = 0; i + 3 < buf.size(); i += 4)
        faces.push_back({ { buf[i], buf[i + 1], buf[i + 2], buf[i + 3] } });
    return faces;
}

// X/Y/Z axes (line + 2D arrowhead + label), matching drawAxisWithMVP().
inline void appendAxes(QTextStream& ts, const QMatrix4x4& mvp,
                       int numCubes, int w, int h)
{
    const float half = numCubes / 2.0f;
    const float tip  = 1.15f * numCubes;
    const GlVertex origin{ -half, -half, -half };
    struct Axis { GlVertex end; const char* color; const char* label; };
    const Axis axes[3] = {
                           {{  tip, -half, -half }, "#ff0000", "X"},
                           {{ -half,  tip, -half }, "#00ff00", "Y"},
                           {{ -half, -half,  tip }, "#0080ff", "Z"},
                           };

    QPointF o; float z;
    if (!project(mvp, origin, w, h, o, z)) return;             // shared axis root

    for (const Axis& a : axes) {
        QPointF e; float ze;
        if (!project(mvp, a.end, w, h, e, ze)) continue;
        QPointF d = e - o;
        const double len = std::hypot(d.x(), d.y());
        if (len < 1.0) continue;
        d /= len;                                              // 2D axis direction
        const QPointF perp(-d.y(), d.x());
        const double s = 10.0;                                 // arrowhead size, px
        const QPointF base = e - d * s;                        // shaft stops before head
        const QPointF p1 = base + perp * (s * 0.5);
        const QPointF p2 = base - perp * (s * 0.5);
        const QPointF lbl = e + d * 12.0;                      // label past the tip

        ts << QString("<line x1=\"%1\" y1=\"%2\" x2=\"%3\" y2=\"%4\" "
                      "stroke=\"%5\" stroke-width=\"2\"/>\n")
                  .arg(o.x(),0,'f',2).arg(o.y(),0,'f',2)
                  .arg(base.x(),0,'f',2).arg(base.y(),0,'f',2).arg(a.color);
        ts << QString("<polygon points=\"%1,%2 %3,%4 %5,%6\" fill=\"%7\"/>\n")
                  .arg(e.x(),0,'f',2).arg(e.y(),0,'f',2)
                  .arg(p1.x(),0,'f',2).arg(p1.y(),0,'f',2)
                  .arg(p2.x(),0,'f',2).arg(p2.y(),0,'f',2).arg(a.color);
        ts << QString("<text x=\"%1\" y=\"%2\" fill=\"%3\" font-family=\"sans-serif\" "
                      "font-size=\"16\" text-anchor=\"middle\" "
                      "dominant-baseline=\"central\">%4</text>\n")
                  .arg(lbl.x(),0,'f',2).arg(lbl.y(),0,'f',2).arg(a.color).arg(a.label);
    }
}

inline void writeVoxelSVG(const QString& path,
                          const std::vector<Face>& faces,
                          const QMatrix4x4& mvp,
                          int w, int h,
                          const QColor& background = Qt::white,
                          bool drawAxes = false, int numCubes = 0)
{
    std::vector<ProjFace> out;
    out.reserve(faces.size());
    for (const Face& f : faces) {
        ProjFace pf; pf.pts.reserve(f.v.size());
        float zsum = 0.0f; bool ok = true;
        for (const GlVertex& g : f.v) {
            QPointF p; float z;
            if (!project(mvp, g, w, h, p, z)) { ok = false; break; }
            pf.pts.push_back(p); zsum += z;
        }
        if (!ok || pf.pts.size() < 3) continue;
        pf.depth = zsum / pf.pts.size();
        pf.color = shadeFace(f.v[0]);
        out.push_back(std::move(pf));
    }
    std::sort(out.begin(), out.end(),
              [](const ProjFace& a, const ProjFace& b){ return a.depth > b.depth; });

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream ts(&file);
    ts << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
       << QString("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%1\" height=\"%2\" "
                  "viewBox=\"0 0 %1 %2\">\n").arg(w).arg(h)
       << QString("<rect width=\"%1\" height=\"%2\" fill=\"%3\"/>\n")
              .arg(w).arg(h).arg(background.name());
    if (drawAxes && numCubes > 0)
        appendAxes(ts, mvp, numCubes, w, h);                   // behind the voxels
    for (const ProjFace& pf : out) {
        QString p;
        for (const QPointF& q : pf.pts)
            p += QString("%1,%2 ").arg(q.x(),0,'f',2).arg(q.y(),0,'f',2);
        ts << QString("<polygon points=\"%1\" fill=\"%2\"/>\n")
                  .arg(p.trimmed(), QColor::fromRgba(pf.color).name());
    }
    ts << "</svg>\n";
}

} // namespace svgx
#endif