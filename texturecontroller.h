#ifndef TEXTURECONTROLLER_H
#define TEXTURECONTROLLER_H

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <vector>
#include "texturelibrary.h"

class TextureController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QVariantList components READ components NOTIFY componentsChanged)

    Q_PROPERTY(QVariantList eulerPoints READ eulerPoints NOTIFY previewChanged)

    Q_PROPERTY(QVariantList componentLabels READ componentLabels CONSTANT)

    Q_PROPERTY(int process READ process WRITE setProcess NOTIFY processChanged)
    Q_PROPERTY(QVariantList processNames READ processNames CONSTANT)

    Q_PROPERTY(double sectionPhi2 READ sectionPhi2 WRITE setSectionPhi2 NOTIFY sectionChanged)

    Q_PROPERTY(int    grainCount READ grainCount WRITE setGrainCount NOTIFY paramsChanged)
    Q_PROPERTY(double scatterDeg READ scatterDeg WRITE setScatterDeg NOTIFY paramsChanged)

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

    Q_INVOKABLE void setProcess(int processIndex);

    Q_INVOKABLE void regenerate();

    Q_INVOKABLE void convertAngles(double phi1, double Phi, double phi2);

    Q_INVOKABLE void applyToStress();

    QVariantList components()   const;
    QVariantList eulerPoints()  const { return m_eulerPoints; }
    QVariantList componentLabels() const;
    int          process()      const { return m_process; }
    QVariantList processNames() const;
    double       sectionPhi2()  const { return m_sectionPhi2; }
    void         setSectionPhi2(double v);
    int          grainCount()   const { return m_grainCount; }
    double       scatterDeg()   const { return m_scatterDeg; }
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

    void textureReady(const std::vector<TextureLibrary::Component>& comps);

private:
    void rebuildPolePoints();

    TextureLibrary                          m_lib;
    std::vector<TextureLibrary::Component>  m_components;
    QVariantList                            m_eulerPoints;

    int    m_process = -1;   // метод изготовления материала (TextureLibrary::Process)
    int    m_grainCount = 1000;
    double m_scatterDeg = 11.0;
    double m_sectionPhi2 = 45.0;
    QString m_thxy = "—", m_thyz = "—", m_thzx = "—";
};

#endif // TEXTURECONTROLLER_H
