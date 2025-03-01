#pragma once

#include <QQuickFramebufferObject>
#include <QOpenGLFunctions>

class RenderOpenGL;

class OpenGLWidgetQML : public QQuickFramebufferObject {
    Q_OBJECT
    Q_PROPERTY(qreal t READ t WRITE setT NOTIFY tChanged)

    QML_ELEMENT

public:
    explicit OpenGLWidgetQML(QQuickItem *parent = nullptr);
    Renderer *createRenderer() const override;

    qreal t() const { return m_t; }
    void setT(qreal t);

signals:
    void tChanged();

private:
    qreal m_t = 0.0;
};
