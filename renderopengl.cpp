#include "renderopengl.h"

RenderOpenGL::RenderOpenGL() {
    initializeOpenGLFunctions();

    QOpenGLContext *glContext = QOpenGLContext::currentContext();
    if (!glContext) {
        qDebug() << "OpenGL context is NULL!";
    } else {
        qDebug() << "OpenGL initialized. Context valid:" << glContext->isValid();
    }

    m_program.addShaderFromSourceCode(QOpenGLShader::Vertex,
                                      "attribute vec4 vertices;"
                                      "varying vec2 coords;"
                                      "void main() {"
                                      "    gl_Position = vertices;"
                                      "    coords = vertices.xy;"
                                      "}");
    m_program.addShaderFromSourceCode(QOpenGLShader::Fragment,
                                      "uniform float t;"
                                      "varying vec2 coords;"
                                      "void main() {"
                                      "    float r = length(coords);"
                                      "    float alpha = sin(t) * 0.5 + 0.5;"
                                      "    gl_FragColor = vec4(abs(sin(t * 3.14)) * r, r, 0.5, 1.0);"
                                      "}");
    m_program.link();
}

void RenderOpenGL::render() {

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    m_program.bind();
    m_program.setUniformValue("t", static_cast<float>(m_t));

    GLfloat vertices[] = {
        -1, -1,  1, -1,  -1, 1,
        1, -1,  1, 1,   -1, 1
    };

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glEnableVertexAttribArray(0);

    glDrawArrays(GL_TRIANGLES, 0, 6);
    m_program.release();

    m_t += 0.01;
    update();
}
