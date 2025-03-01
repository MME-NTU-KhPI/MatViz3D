#pragma once

#include <QQuickFramebufferObject>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>

class RenderOpenGL : public QQuickFramebufferObject::Renderer, protected QOpenGLFunctions {
public:
    RenderOpenGL();
    void render() override;
    QSize size() const { return QSize(300, 300); }

private:
    QOpenGLShaderProgram m_program;
    qreal m_t = 0.0;
};
