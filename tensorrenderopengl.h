#ifndef TENSORRENDEROPENGL_H
#define TENSORRENDEROPENGL_H

#pragma once
#include <QOpenGLFunctions>
#include <QOpenGLFramebufferObject>
#include <vector>

class TensorRenderOpenGL : public QObject, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    TensorRenderOpenGL();
    ~TensorRenderOpenGL() override = default;

    void render();
    QOpenGLFramebufferObject* createFBO(const QSize &size);

    // Дані тензора
    void setTensorField(const std::vector<double> &values,
                        const std::vector<double> &thetas,
                        const std::vector<double> &phis,
                        int N);

private:
    std::vector<double> m_values;
    std::vector<double> m_thetas;
    std::vector<double> m_phis;
    int m_N = 0;

    float distance = 3.5f;
};


#endif // TENSORRENDEROPENGL_H
