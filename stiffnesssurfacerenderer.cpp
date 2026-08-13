#include "stiffnesssurfacerenderer.h"
#include "stiffnesssurfaceitem.h"
#include "glshaders.hpp"

#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>
#include <QOpenGLExtraFunctions>
#include <QQuickWindow>
#include <QDebug>

#include <cstddef>

StiffnessSurfaceRenderer::StiffnessSurfaceRenderer() = default;

StiffnessSurfaceRenderer::~StiffnessSurfaceRenderer()
{
    // The GL context is current during renderer destruction, so these are safe.
    QOpenGLExtraFunctions* ef = QOpenGLContext::currentContext()
                                    ? QOpenGLContext::currentContext()->extraFunctions()
                                    : nullptr;
    if (ef) {
        if (m_vbo)     ef->glDeleteBuffers(1, &m_vbo);
        if (m_ebo)     ef->glDeleteBuffers(1, &m_ebo);
        if (m_vao)     ef->glDeleteVertexArrays(1, &m_vao);
        if (m_axisVbo) ef->glDeleteBuffers(1, &m_axisVbo);
        if (m_axisVao) ef->glDeleteVertexArrays(1, &m_axisVao);
    }
    delete m_litProgram;
    delete m_axisProgram;
}

QOpenGLFramebufferObject* StiffnessSurfaceRenderer::createFramebufferObject(const QSize& size)
{
    QOpenGLFramebufferObjectFormat fmt;
    fmt.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    fmt.setSamples(4);                       // the surface is smooth; MSAA is cheap here
    return new QOpenGLFramebufferObject(size, fmt);
}

void StiffnessSurfaceRenderer::synchronize(QQuickFramebufferObject* item)
{
    // Runs on the render thread with the GUI thread blocked: the one safe place
    // to copy item state across.
    auto* s = qobject_cast<StiffnessSurfaceItem*>(item);
    if (!s) return;

    if (s->takeMeshDirty()) {
        m_verts   = s->mesh().verts;
        m_indices = s->mesh().indices;
        m_meshDirty = true;
    }

    m_xRot = s->xRot();
    m_yRot = s->yRot();
    m_zRot = s->zRot();
    m_distance  = s->distance();
    m_wireframe = s->wireframe();
    m_showAxes  = s->showAxes();
    m_viewSize  = QSize(int(s->width()), int(s->height()));
    m_dpr = s->window() ? float(s->window()->devicePixelRatio()) : 1.0f;
}

void StiffnessSurfaceRenderer::initializeGL()
{
    initializeOpenGLFunctions();
    QOpenGLExtraFunctions* ef = QOpenGLContext::currentContext()->extraFunctions();

    m_litProgram = new QOpenGLShaderProgram();
    if (!m_litProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, matviz_gl::kLitVertexShader) ||
        !m_litProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, matviz_gl::kLitFragmentShader)) {
        qWarning() << "[StiffnessSurfaceRenderer] lit shader compile failed:" << m_litProgram->log();
        delete m_litProgram; m_litProgram = nullptr;
    } else {
        m_litProgram->bindAttributeLocation("aPosition", 0);
        m_litProgram->bindAttributeLocation("aColor", 1);
        m_litProgram->bindAttributeLocation("aNormal", 2);
        if (!m_litProgram->link()) {
            qWarning() << "[StiffnessSurfaceRenderer] lit shader link failed:" << m_litProgram->log();
            delete m_litProgram; m_litProgram = nullptr;
        }
    }

    m_axisProgram = new QOpenGLShaderProgram();
    if (!m_axisProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, matviz_gl::kAxisVertexShader) ||
        !m_axisProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, matviz_gl::kAxisFragmentShader)) {
        qWarning() << "[StiffnessSurfaceRenderer] axis shader compile failed:" << m_axisProgram->log();
        delete m_axisProgram; m_axisProgram = nullptr;
    } else {
        m_axisProgram->bindAttributeLocation("aPosition", 0);
        m_axisProgram->bindAttributeLocation("aColor", 1);
        if (!m_axisProgram->link()) {
            qWarning() << "[StiffnessSurfaceRenderer] axis shader link failed:" << m_axisProgram->log();
            delete m_axisProgram; m_axisProgram = nullptr;
        }
    }

    // Surface mesh VAO. NOTE the ordering: the element buffer binding is part
    // of VAO state, so the EBO must be bound while the VAO is bound, and the
    // VAO must be unbound BEFORE the EBO. Getting this backwards silently
    // renders nothing at all.
    ef->glGenVertexArrays(1, &m_vao);
    ef->glGenBuffers(1, &m_vbo);
    ef->glGenBuffers(1, &m_ebo);

    ef->glBindVertexArray(m_vao);
    ef->glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    ef->glEnableVertexAttribArray(0);
    ef->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GlVertex),
                              reinterpret_cast<void*>(offsetof(GlVertex, x)));
    ef->glEnableVertexAttribArray(1);
    ef->glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(GlVertex),
                              reinterpret_cast<void*>(offsetof(GlVertex, r)));
    ef->glEnableVertexAttribArray(2);
    ef->glVertexAttribPointer(2, 3, GL_BYTE, GL_TRUE, sizeof(GlVertex),
                              reinterpret_cast<void*>(offsetof(GlVertex, nx)));
    ef->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    ef->glBindVertexArray(0);
    ef->glBindBuffer(GL_ARRAY_BUFFER, 0);
    ef->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    // Axis triad: 3 lines, position + colour, static.
    const GLfloat axisData[] = {
        //  x     y     z     r    g    b
        0, 0, 0,  1, 0, 0,   1.35f, 0, 0,  1, 0, 0,
        0, 0, 0,  0, 1, 0,   0, 1.35f, 0,  0, 1, 0,
        0, 0, 0,  0, 0, 1,   0, 0, 1.35f,  0, 0, 1,
    };
    ef->glGenVertexArrays(1, &m_axisVao);
    ef->glGenBuffers(1, &m_axisVbo);
    ef->glBindVertexArray(m_axisVao);
    ef->glBindBuffer(GL_ARRAY_BUFFER, m_axisVbo);
    ef->glBufferData(GL_ARRAY_BUFFER, sizeof(axisData), axisData, GL_STATIC_DRAW);
    ef->glEnableVertexAttribArray(0);
    ef->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat),
                              reinterpret_cast<void*>(0));
    ef->glEnableVertexAttribArray(1);
    ef->glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat),
                              reinterpret_cast<void*>(3 * sizeof(GLfloat)));
    ef->glBindVertexArray(0);
    ef->glBindBuffer(GL_ARRAY_BUFFER, 0);

    m_initialized = true;
}

void StiffnessSurfaceRenderer::uploadMesh()
{
    if (!m_meshDirty) return;
    m_meshDirty = false;

    QOpenGLExtraFunctions* ef = QOpenGLContext::currentContext()->extraFunctions();
    ef->glBindVertexArray(m_vao);

    ef->glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    ef->glBufferData(GL_ARRAY_BUFFER,
                     GLsizeiptr(m_verts.size() * sizeof(GlVertex)),
                     m_verts.empty() ? nullptr : m_verts.data(),
                     GL_STATIC_DRAW);

    ef->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    // GL_UNSIGNED_INT is required, not a luxury: a 48x96 surface is ~4.7k
    // vertices but glyph meshes sharing this format run past 65535.
    ef->glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     GLsizeiptr(m_indices.size() * sizeof(uint32_t)),
                     m_indices.empty() ? nullptr : m_indices.data(),
                     GL_STATIC_DRAW);

    m_indexCount = GLsizei(m_indices.size());
    ef->glBindVertexArray(0);
}

void StiffnessSurfaceRenderer::drawAxisTriad(const QMatrix4x4& mvp)
{
    if (!m_axisProgram || m_axisVao == 0) return;
    QOpenGLExtraFunctions* ef = QOpenGLContext::currentContext()->extraFunctions();

    m_axisProgram->bind();
    m_axisProgram->setUniformValue("uMVP", mvp);
    ef->glBindVertexArray(m_axisVao);
    glLineWidth(1.5f);
    ef->glDrawArrays(GL_LINES, 0, 6);
    ef->glBindVertexArray(0);
    m_axisProgram->release();
}

void StiffnessSurfaceRenderer::render()
{
    if (!m_initialized) initializeGL();

    QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
    QOpenGLExtraFunctions* ef = QOpenGLContext::currentContext()->extraFunctions();

    uploadMesh();

    const int w = std::max(1, int(m_viewSize.width()  * m_dpr));
    const int h = std::max(1, int(m_viewSize.height() * m_dpr));
    f->glViewport(0, 0, w, h);

    f->glClearColor(0.157f, 0.157f, 0.157f, 1.0f);   // #282828, the DB window's ground
    f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    f->glEnable(GL_DEPTH_TEST);
    f->glDepthFunc(GL_LEQUAL);
    // Face culling stays OFF: the radial function can pass close to zero, and a
    // sign-changing quantity produces thin geometry that reads wrong when back
    // faces are dropped.
    f->glDisable(GL_CULL_FACE);

    // Aspect from the real framebuffer, never a constant.
    QMatrix4x4 projection;
    projection.perspective(45.0f, float(w) / float(std::max(1, h)), 0.05f, 100.0f);

    QMatrix4x4 view;
    view.translate(0.0f, 0.0f, -m_distance);
    view.rotate(m_xRot, 1.0f, 0.0f, 0.0f);
    view.rotate(m_yRot, 0.0f, 1.0f, 0.0f);
    view.rotate(m_zRot, 0.0f, 0.0f, 1.0f);

    QMatrix4x4 model;
    const QMatrix4x4 mvp = projection * view * model;

    if (m_showAxes) drawAxisTriad(mvp);

    if (!m_litProgram || m_vao == 0 || m_indexCount == 0) return;

    m_litProgram->bind();
    m_litProgram->setUniformValue("uMVP", mvp);
    m_litProgram->setUniformValue("uModel", model);
    m_litProgram->setUniformValue("uView", view);
    m_litProgram->setUniformValue("uProjection", projection);

    // Same six-light rig as the main view, so the surface reads as part of the
    // same application rather than a differently-shaded panel.
    QVector3D lightDirs[6] = {
        QVector3D( 1.0f,  1.0f,  1.0f).normalized(),
        QVector3D(-1.0f,  1.0f,  0.5f).normalized(),
        QVector3D( 0.0f,  1.0f, -1.0f).normalized(),
        QVector3D( 0.0f, -1.0f,  0.5f).normalized(),
        QVector3D(-1.0f, -0.5f, -1.0f).normalized(),
        QVector3D( 1.0f, -0.5f, -1.0f).normalized(),
    };
    const GLfloat lightWeights[6] = { 0.40f, 0.25f, 0.15f, 0.10f, 0.05f, 0.05f };
    for (int i = 0; i < 6; ++i) {
        m_litProgram->setUniformValueArray(
            QString("uLightDirections[%1]").arg(i).toUtf8().constData(), &lightDirs[i], 1);
    }
    m_litProgram->setUniformValueArray("uLightWeights", lightWeights, 6, 1);

    const QVector3D viewPos = view.inverted().column(3).toVector3D();
    m_litProgram->setUniformValue("uViewPos", viewPos);
    m_litProgram->setUniformValue("uDebugMode", 0);
    m_litProgram->setUniformValue("uWireframe", 0);

    ef->glBindVertexArray(m_vao);
    if (m_wireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        // One indexed draw call for the whole surface -- deliberately not the
        // per-quad glDrawArrays loop the main voxel renderer still uses.
        ef->glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    } else {
        ef->glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);
    }
    ef->glBindVertexArray(0);
    m_litProgram->release();
}
