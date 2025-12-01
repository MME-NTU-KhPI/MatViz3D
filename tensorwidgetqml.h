#ifndef TENSORWIDGETQML_H
#define TENSORWIDGETQML_H

#pragma once
#include <QQuickFramebufferObject>
#include "tensorrenderopengl.h"

class TensorWidgetQML : public QQuickFramebufferObject
{
    Q_OBJECT

public:
    TensorWidgetQML();
    Renderer* createRenderer() const override;

    Q_INVOKABLE void setVisualizationData(const QVariantList& values,
                                          const QVariantList& thetas,
                                          const QVariantList& phis,
                                          int N);

    static TensorRenderOpenGL* rendererInstance;

signals:

public slots:
    void updateTensorData(const std::vector<double>& v,
                          const std::vector<double>& t,
                          const std::vector<double>& p,
                          int N);

};


#endif // TENSORWIDGETQML_H
