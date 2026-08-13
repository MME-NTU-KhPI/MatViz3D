// ============================================================================
//  stiffnesssurfacerenderer.h  -  Renderer for the elastic directional surface.
//
//  A second QQuickFramebufferObject::Renderer, needed because OpenGLWidgetQML
//  holds its renderer in a STATIC pointer and deletes/recreates it in
//  createRenderer(); instantiating a second one would tear down the main 3D
//  view's renderer.  See openglwidgetqml.h.
//
//  It shares glshaders.hpp, glvertex.hpp and colormap.hpp with the main
//  renderer, so the surface is lit and coloured identically.  Three things it
//  deliberately does NOT copy from RenderOpenGL:
//
//    1. It implements synchronize(), which is the correct place to move item
//       state across the GUI/render thread boundary.  RenderOpenGL instead
//       pushes directly from the GUI thread.
//    2. Its renderer pointer is not static -- Qt owns the instance.
//    3. Its aspect ratio comes from the framebuffer size, never a constant.
// ============================================================================
#pragma once

#include <QQuickFramebufferObject>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QMatrix4x4>

#include <cstdint>
#include <vector>

#include "glvertex.hpp"

class StiffnessSurfaceRenderer : public QQuickFramebufferObject::Renderer,
                                 protected QOpenGLFunctions
{
public:
    StiffnessSurfaceRenderer();
    ~StiffnessSurfaceRenderer() override;

    void render() override;
    void synchronize(QQuickFramebufferObject* item) override;
    QOpenGLFramebufferObject* createFramebufferObject(const QSize& size) override;

private:
    void initializeGL();
    void uploadMesh();
    void drawAxisTriad(const QMatrix4x4& mvp);

    bool m_initialized = false;

    QOpenGLShaderProgram* m_litProgram  = nullptr;
    QOpenGLShaderProgram* m_axisProgram = nullptr;

    GLuint m_vao = 0, m_vbo = 0, m_ebo = 0;
    GLuint m_axisVao = 0, m_axisVbo = 0;

    // --- state mirrored from the item in synchronize() ---
    std::vector<GlVertex> m_verts;
    std::vector<uint32_t> m_indices;
    bool  m_meshDirty = false;
    GLsizei m_indexCount = 0;

    float m_xRot = 0.0f, m_yRot = 0.0f, m_zRot = 0.0f;   // degrees
    float m_distance = 3.2f;
    bool  m_wireframe = false;
    bool  m_showAxes = true;
    QSize m_viewSize{ 1, 1 };
    float m_dpr = 1.0f;
};
