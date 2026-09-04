#ifndef SVGEXPORTER_HPP
#define SVGEXPORTER_HPP
// svgexporter.hpp — true-vector SVG export of the voxel scene.
// Free functions, no class, no GL calls: runs on the face geometry
// the renderer has already built.
#include <QMatrix4x4>
#include <QVector4D>
#include <QPointF>
#include <QColor>
#include <QFile>
#include <QTextStream>
#include <algorithm>
#include <vector>
#include "glvertex.hpp"

namespace svgx {

struct Face { std::vector<GlVertex> v; };   // one flat face (3-4 points, single color)

struct ProjFace { std::vector<QPointF> pts; float depth; QRgb color; };


// Flat per-face shading so the three cube sides read as distinct.
inline QRgb shadeFace(const GlVertex& c)
{
    QVector3D n(c.nx / 127.0f, c.ny / 127.0f, c.nz / 127.0f);
    static const QVector3D L = QVector3D(1, 1, 1).normalized();     // key light
    const float diff = std::max(0.0f, QVector3D::dotProduct(n.normalized(), L));
    const float k = 0.55f + 0.45f * diff;                           // ambient + diffuse
    auto ch = [k](GLubyte v){ return GLubyte(std::clamp(v * k, 0.0f, 255.0f)); };
    return qRgba(ch(c.r), ch(c.g), ch(c.b), c.a);
}

// Project a world point through mvp into pixels. false = point is behind the camera.
inline bool project(const QMatrix4x4& mvp, const GlVertex& g,
                    int w, int h, QPointF& out, float& zOut)
{
    const QVector4D clip = mvp * QVector4D(g.x, g.y, g.z, 1.0f);
    if (clip.w() <= 0.0f) return false;               // behind the eye -> drop
    out.setX((clip.x() / clip.w() * 0.5f + 0.5f) * w);        // NDC -> pixels, X
    out.setY((clip.y() / clip.w() * 0.5f + 0.5f) * h);   // Quick FBO already flips Y
    zOut = clip.z() / clip.w();                        // sort key
    return true;
}

inline void writeVoxelSVG(const QString& path,
                          const std::vector<Face>& faces,
                          const QMatrix4x4& mvp,
                          int w, int h,
                          const QColor& background = Qt::white)
{
    std::vector<ProjFace> out;
    out.reserve(faces.size());
    for (const Face& f : faces) {
        ProjFace pf; pf.pts.reserve(f.v.size());
        float zsum = 0.0f; bool ok = true;
        for (const GlVertex& g : f.v) {
            QPointF s; float z;
            if (!project(mvp, g, w, h, s, z)) { ok = false; break; }
            pf.pts.push_back(s); zsum += z;
        }
        if (!ok || pf.pts.size() < 3) continue;        // clipped or degenerate
        pf.depth = zsum / pf.pts.size();
        const GlVertex& c = f.v[0];                     // flat fill = face color
        pf.color = svgx::shadeFace(f.v[0]);   // was: qRgba(c.r, c.g, c.b, c.a)
        out.push_back(std::move(pf));
    }
    // Painter's algorithm: farthest (largest z) drawn first.
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
    for (const ProjFace& pf : out) {
        QString p;
        for (const QPointF& q : pf.pts)
            p += QString("%1,%2 ").arg(q.x(), 0, 'f', 2).arg(q.y(), 0, 'f', 2);
        ts << QString("<polygon points=\"%1\" fill=\"%2\"/>\n")
                  .arg(p.trimmed(), QColor::fromRgba(pf.color).name());
    }
    ts << "</svg>\n";
}

// Build faces from the app's flat quad buffer: 4 verts per GL_TRIANGLE_FAN.
inline std::vector<Face> facesFromQuadBuffer(const std::vector<GlVertex>& buf)
{
    std::vector<Face> faces;
    faces.reserve(buf.size() / 4);
    for (size_t i = 0; i + 3 < buf.size(); i += 4)
        faces.push_back({ { buf[i], buf[i + 1], buf[i + 2], buf[i + 3] } });
    return faces;
}

} // namespace svgx
#endif