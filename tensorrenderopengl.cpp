#include "tensorrenderopengl.h"
#include <cmath>
#include <QOpenGLFramebufferObjectFormat>

TensorRenderOpenGL::TensorRenderOpenGL()
{
    initializeOpenGLFunctions();
}

QOpenGLFramebufferObject* TensorRenderOpenGL::createFBO(const QSize &size)
{
    QOpenGLFramebufferObjectFormat fmt;
    fmt.setAttachment(QOpenGLFramebufferObject::Depth);
    fmt.setSamples(4);
    return new QOpenGLFramebufferObject(size, fmt);
}

void TensorRenderOpenGL::setTensorField(const std::vector<double> &values,
                                        const std::vector<double> &thetas,
                                        const std::vector<double> &phis,
                                        int N)
{
    m_values = values;
    m_thetas = thetas;
    m_phis = phis;
    m_N = N;
}

void TensorRenderOpenGL::render()
{
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    double minVal = 1e100;
    double maxVal = -1e100;

    for (double v : m_values) {
        if (v < minVal) minVal = v;
        if (v > maxVal) maxVal = v;
    }

    double range = maxVal - minVal;
    if (range < 1e-9) range = 1.0;

    const float TARGET_SIZE = 0.35f;
    float scale_factor = TARGET_SIZE / (float)maxVal;

    float aspect = 1.6f;

    float fovY = 45.0f;
    float f = 1.0f / tan(fovY * 0.5f * M_PI / 180.0f);

    float zNear = 0.1f;
    float zFar  = 100.0f;

    float proj[16] = {
        f/aspect, 0, 0, 0,
        0, f, 0, 0,
        0, 0, (zFar+zNear)/(zNear-zFar), -1,
        0, 0, (2*zFar*zNear)/(zNear-zFar), 0
    };

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(proj);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0, 0, -2.2f);

    glRotatef(130.0f, 1, 0, 0);
    glRotatef(135.0f, 0, 1, 0);

    if (m_N < 2) return;

    auto heatColor = [&](double v) {
        double t = (v - minVal) / range;
        float r = (float)t;
        float b = (float)(1.0 - t);
        float g = 0.25f + 0.5f * (1.0f - fabs(t - 0.5) * 2.0f);
        return std::array<float,3>{r,g,b};
    };
    auto sph = [scale_factor](double r,double t,double p){

        float scaled_r = (float)r * scale_factor;

        return std::array<float,3>{
            (float)(scaled_r*sin(t)*cos(p)),
            (float)(scaled_r*sin(t)*sin(p)),
            (float)(scaled_r*cos(t))
        };
    };

    for (int i = 0; i < m_N - 1; i++)
    {
        for (int j = 0; j < m_N - 1; j++)
        {
            int i00 = i*m_N + j;
            int i10 = (i+1)*m_N + j;
            int i11 = (i+1)*m_N + (j+1);
            int i01 = i*m_N + (j+1);

            auto v00 = sph(m_values[i00], m_thetas[i], m_phis[j]);
            auto v10 = sph(m_values[i10], m_thetas[i+1], m_phis[j]);
            auto v11 = sph(m_values[i11], m_thetas[i+1], m_phis[j+1]);
            auto v01 = sph(m_values[i01], m_thetas[i], m_phis[j+1]);

            auto c00 = heatColor(m_values[i00]);

            glBegin(GL_QUADS);
            glColor3f(c00[0], c00[1], c00[2]);
            glVertex3fv(v00.data());
            glVertex3fv(v10.data());
            glVertex3fv(v11.data());
            glVertex3fv(v01.data());
            glEnd();
        }
    }
    glLineWidth(3.0f);

    glBegin(GL_LINES);

    // X — red
    glColor3f(1, 0, 0);
    glVertex3f(0, 0, 0);
    glVertex3f(0.5f, 0, 0);

    // Y — green
    glColor3f(0, 1, 0);
    glVertex3f(0, 0, 0);
    glVertex3f(0, 0.5f, 0);

    // Z — blue
    glColor3f(0, 0, 1);
    glVertex3f(0, 0, 0);
    glVertex3f(0, 0, 0.5f);

    glEnd();
}
