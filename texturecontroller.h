#ifndef TEXTURECONTROLLER_H
#define TEXTURECONTROLLER_H

#include <QObject>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <array>
#include <vector>
#include "texturelibrary.h"

class TextureController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QVariantList presets READ presets CONSTANT)

    Q_PROPERTY(QVariantList components READ components NOTIFY componentsChanged)

    Q_PROPERTY(QVariantList eulerPoints READ eulerPoints NOTIFY previewChanged)

    // Pole figure: flat [x0,y0, x1,y1, ...] in [-1,1] disc coordinates
    // (RD up, TD right, ND at the centre).
    Q_PROPERTY(QVariantList polePoints READ polePoints NOTIFY previewChanged)
    Q_PROPERTY(int poleFamily READ poleFamily WRITE setPoleFamily NOTIFY poleFamilyChanged)
    Q_PROPERTY(QString poleFamilyName READ poleFamilyName NOTIFY poleFamilyChanged)

    // ODF: one entry per φ2 section, each { phi2, max, contours: [ {level, segs} ] }.
    Q_PROPERTY(QVariantList odfSections READ odfSections NOTIFY previewChanged)
    Q_PROPERTY(double odfMax READ odfMax NOTIFY previewChanged)
    Q_PROPERTY(int odfBins READ odfBins NOTIFY previewChanged)

    Q_PROPERTY(QVariantList componentLabels READ componentLabels CONSTANT)

    Q_PROPERTY(double sectionPhi2 READ sectionPhi2 WRITE setSectionPhi2 NOTIFY sectionChanged)

    // Half-thickness of the φ2 slab the Euler view draws. Owned here so the
    // QML plot and the SVG export cannot disagree about it.
    Q_PROPERTY(double sectionTol READ sectionTol CONSTANT)

    Q_PROPERTY(int process READ process WRITE setProcess NOTIFY processChanged)
    Q_PROPERTY(int lattice READ lattice WRITE setLattice NOTIFY processChanged)

    Q_PROPERTY(int    grainCount READ grainCount WRITE setGrainCount NOTIFY paramsChanged)
    Q_PROPERTY(double scatterDeg READ scatterDeg WRITE setScatterDeg NOTIFY paramsChanged)

    // Read-only mirror of Parameters::seed -- the preview is a deterministic
    // function of it, so showing it makes a "Generate" result reproducible.
    Q_PROPERTY(int seed READ seed NOTIFY paramsChanged)

    Q_PROPERTY(QString ansysThxy READ ansysThxy NOTIFY converterChanged)
    Q_PROPERTY(QString ansysThyz READ ansysThyz NOTIFY converterChanged)
    Q_PROPERTY(QString ansysThzx READ ansysThzx NOTIFY converterChanged)

public:
    explicit TextureController(QObject* parent = nullptr);
    Q_INVOKABLE void addCustom(int h,int k,int l, int u,int v,int w);
    Q_INVOKABLE void removeComponent(int index);
    Q_INVOKABLE void setWeight(int index, double weight);
    Q_INVOKABLE void setComponentScatter(int index, double scatter);
    Q_INVOKABLE void clearComponents();

    Q_INVOKABLE void regenerate();

    // Draws a new global Parameters::seed and regenerates -- this is what the
    // "Generate" button calls, so every press yields a new realization (and one
    // the structure/stress pipelines will reproduce, since they read the same seed).
    Q_INVOKABLE void reseed();

    Q_INVOKABLE void convertAngles(double phi1, double Phi, double phi2);

    Q_INVOKABLE void applyToStress();

    // Vector export of the current plot. view: 0 = pole figure, 1 = ODF
    // sections, 2 = Euler section. Writes real SVG geometry (circles, lines,
    // text) built from the same arrays QML draws, so the output is scalable
    // rather than a screenshot. Returns false and logs on failure.
    Q_INVOKABLE bool exportSvg(int view, const QUrl& fileUrl, bool dark);

    // QUrl (what FileDialog hands back) -> native path, for Item.grabToImage's
    // saveToFile(), which wants a plain filesystem path.
    Q_INVOKABLE QString toLocalFile(const QUrl& fileUrl) const;

    QVariantList presets()     const;
    QVariantList components()   const;
    QVariantList eulerPoints()  const { return m_eulerPoints; }
    QVariantList polePoints()   const { return m_polePoints; }
    QVariantList odfSections()  const { return m_odfSections; }
    double       odfMax()       const { return m_odfMax; }
    int          odfBins()      const { return m_odfBins; }
    int          poleFamily()   const { return m_poleFamily; }
    QString      poleFamilyName() const;
    void         setPoleFamily(int f);
    QVariantList componentLabels() const;
    double       sectionPhi2()  const { return m_sectionPhi2; }
    double       sectionTol()   const { return kSectionTolDeg; }
    int          process()      const { return m_process; }
    int          lattice()      const { return m_lattice; }
    void         setProcess(int p);
    void         setLattice(int l);
    void         setSectionPhi2(double v);
    int          grainCount()   const { return m_grainCount; }
    double       scatterDeg()   const { return m_scatterDeg; }
    int          seed()         const;
    QString      ansysThxy()    const { return m_thxy; }
    QString      ansysThyz()    const { return m_thyz; }
    QString      ansysThzx()    const { return m_thzx; }

    void setGrainCount(int n);
    void setScatterDeg(double s);

signals:
    void componentsChanged();
    void previewChanged();
    void paramsChanged();
    void converterChanged();
    void sectionChanged();
    void processChanged();
    void poleFamilyChanged();

    void textureReady(const std::vector<TextureLibrary::Component>& comps);

private:
    // regenerate() samples the grains once (sampleOrientations) and every view
    // is then derived from that same realization, so the Euler section, the
    // pole figure and the ODF always show the identical set of grains.
    void sampleOrientations();
    void rebuildPolePoints();      // Euler φ1–Φ scatter
    void rebuildPoleFigure();      // stereographic pole figure
    void rebuildOdf();             // ODF sections + contours
    void rebuildFromProcess();

    TextureLibrary                          m_lib;
    std::vector<TextureLibrary::Component>  m_components;

    std::vector<std::array<double,3>>       m_orientations;  // raw Bunge ZXZ, deg
    std::vector<std::array<double,3>>       m_fzVariants;    // symmetry-reduced

    QVariantList                            m_eulerPoints;
    QVariantList                            m_polePoints;
    QVariantList                            m_odfSections;
    double                                  m_odfMax  = 0.0;
    int                                     m_odfBins = 0;
    int                                     m_poleFamily = 2;   // {111}

    // SVG writers for the three views (see exportSvg).
    QString svgPoleFigure(bool dark) const;
    QString svgOdfSections(bool dark) const;
    QString svgEulerSection(bool dark) const;

    static constexpr double kSectionTolDeg = 8.0;

    int    m_grainCount = 1000;
    double m_scatterDeg = 11.0;
    double m_sectionPhi2 = 45.0;
    int    m_process = 2;   // Rolling
    int    m_lattice = 0;   // FCC
    QString m_thxy = "—", m_thyz = "—", m_thzx = "—";
};

#endif // TEXTURECONTROLLER_H
