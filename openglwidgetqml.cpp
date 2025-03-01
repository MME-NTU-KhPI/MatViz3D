#include "openglwidgetqml.h"
#include "renderopengl.h"
#include <QTimer>

OpenGLWidgetQML::OpenGLWidgetQML(QQuickItem *parent) : QQuickFramebufferObject(parent) {
    setAcceptedMouseButtons(Qt::AllButtons);
    setTextureFollowsItemSize(true);

    auto timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this]() {
        m_t += 0.01;
        if (m_t > 1.0) m_t = 0.0;
        update();
    });
    timer->start(16);
}

QQuickFramebufferObject::Renderer *OpenGLWidgetQML::createRenderer() const {
    return new RenderOpenGL();
}

void OpenGLWidgetQML::setT(qreal t) {
    if (m_t == t) return;
    m_t = t;
    emit tChanged();
    update();
}
