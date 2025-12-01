#include "tensorwidgetqml.h"

TensorRenderOpenGL* TensorWidgetQML::rendererInstance = nullptr;

std::vector<double> variantListToVector(const QVariantList& list) {
    std::vector<double> vec;
    vec.reserve(list.size());
    for (const QVariant& var : list) {
        vec.push_back(var.toDouble());
    }
    return vec;
}

class TensorSceneRenderer : public QQuickFramebufferObject::Renderer
{
public:
    TensorSceneRenderer()
    {
        renderer = new TensorRenderOpenGL();
        TensorWidgetQML::rendererInstance = renderer;
    }

    ~TensorSceneRenderer() override { delete renderer; }

    void render() override {
        renderer->render();
        update();
    }

    QOpenGLFramebufferObject* createFramebufferObject(const QSize& size) override {
        return renderer->createFBO(size);
    }

    TensorRenderOpenGL* renderer;
};

TensorWidgetQML::TensorWidgetQML() {}

QQuickFramebufferObject::Renderer* TensorWidgetQML::createRenderer() const {
    return new TensorSceneRenderer();
}

void TensorWidgetQML::updateTensorData(const std::vector<double>& v,
                                       const std::vector<double>& t,
                                       const std::vector<double>& p,
                                       int N)
{
    if (rendererInstance)
        rendererInstance->setTensorField(v,t,p,N);

    update();
}

void TensorWidgetQML::setVisualizationData(const QVariantList& values,
                                           const QVariantList& thetas,
                                           const QVariantList& phis,
                                           int N)
{
    // 1. Конвертація QVariantList у std::vector<double>
    std::vector<double> v = variantListToVector(values);
    std::vector<double> t = variantListToVector(thetas);
    std::vector<double> p = variantListToVector(phis);

    // 2. Виклик C++ слота для оновлення рендерера
    updateTensorData(v, t, p, N);
}
