#include "openglwidgetqml.h"
#include "qquickwindow.h"
#include "renderopengl.h"
#include "ansyswrapper.h"

#include <QGuiApplication>
#include <QClipboard>
#include <QTimer>
#include <QImage>
#include <QThread>

RenderOpenGL* OpenGLWidgetQML::m_render = nullptr;
OpenGLWidgetQML* OpenGLWidgetQML::instance = nullptr;

OpenGLWidgetQML::OpenGLWidgetQML(QQuickItem *parent) : QQuickFramebufferObject(parent) {
    setAcceptedMouseButtons(Qt::AllButtons);
    setTextureFollowsItemSize(true);

    xRot = 0;
    yRot = 0;
    zRot = 0;
    distance = 2.0f;
    numCubes = 1;
    voxels = nullptr;
    wr = nullptr;
    timer = new QTimer(this);
    plotComponent = 0;
    delayAnimation = 0;
    bgColor.setRgbF(0.21f, 0.21f, 0.21f);
    connect(this, &QQuickItem::widthChanged, this, &OpenGLWidgetQML::handleResize, Qt::QueuedConnection);
    connect(this, &QQuickItem::heightChanged, this, &OpenGLWidgetQML::handleResize, Qt::QueuedConnection);

    // Only set instance if null (or warn if multiple instances)
    if (instance == nullptr)
    {
        instance = this;
    }
    else
    {
        qWarning() << "Multiple OpenGLWidgetQML instances detected";
    }
}

OpenGLWidgetQML* OpenGLWidgetQML::getInstance()
{
    return instance;
}

QQuickFramebufferObject::Renderer *OpenGLWidgetQML::createRenderer() const
{
    if (m_render)
    {
        delete m_render;
    }
    m_render = new RenderOpenGL();
    m_render->resizeGL(this->width(), this->height());
    m_render->setDevicePixelRatio(window() ? window()->devicePixelRatio() : 1.0f);
    return m_render;
}

OpenGLWidgetQML::~OpenGLWidgetQML()
{
    if (timer)
    {
        delete timer;
    }
    if (m_render)
    {
        //delete m_render;
    }
    instance = nullptr;
}


void OpenGLWidgetQML::qNormalizeAngle(int &angle)
{
    while (angle < -360 * 16)
        angle += 360 * 16;
    while (angle >= 360 * 16)
        angle -= 360 * 16;
}

void OpenGLWidgetQML::setXRotation(int angle)
{
    qNormalizeAngle(angle);
    if (angle != xRot) {
        xRot = angle;
        if (m_render) {
            m_render->setRotations(xRot, yRot, zRot);
        }
        update();
    }
}

void OpenGLWidgetQML::setYRotation(int angle)
{
    qNormalizeAngle(angle);
    if (angle != yRot) {
        yRot = angle;
        if (m_render) {
            m_render->setRotations(xRot, yRot, zRot);
        }
        update();
    }
}

void OpenGLWidgetQML::setZRotation(int angle)
{
    qNormalizeAngle(angle);
    if (angle != zRot) {
        zRot = angle;
        if (m_render) {
            m_render->setRotations(xRot, yRot, zRot);
        }
        update();
    }
}

void OpenGLWidgetQML::setFrontView() {
    xRot = 0;
    yRot = 0;
    zRot = 0;
    if (m_render) {
        m_render->setRotations(xRot, yRot, zRot);
    }
    update();
}

void OpenGLWidgetQML::setBackView() {
    xRot = 0;
    yRot = 180 * 16;
    zRot = 0;
    if (m_render) {
        m_render->setRotations(xRot, yRot, zRot);
    }
    update();
}

void OpenGLWidgetQML::setTopView() {
    xRot = 90 * 16;
    yRot = 0;
    zRot = 0;
    if (m_render) {
        m_render->setRotations(xRot, yRot, zRot);
    }
    update();
}

void OpenGLWidgetQML::setBottomView() {
    xRot = -90 * 16;
    yRot = 0;
    zRot = 0;
    if (m_render) {
        m_render->setRotations(xRot, yRot, zRot);
    }
    update();
}

void OpenGLWidgetQML::setLeftView() {
    xRot = 0;
    yRot = 90 * 16;
    zRot = 0;
    if (m_render) {
        m_render->setRotations(xRot, yRot, zRot);
    }
    update();
}

void OpenGLWidgetQML::setRightView() {
    xRot = 0;
    yRot = -90 * 16;
    zRot = 0;
    if (m_render) {
        m_render->setRotations(xRot, yRot, zRot);
    }
    update();
}

/**
 * Set the view to standard isometric projection.
 */
void OpenGLWidgetQML::setIsometricView()
{
    xRot =  35.26 * 16;
    yRot = -45.00 * 16;
    zRot =  0;
    if (m_render) {
        m_render->setRotations(xRot, yRot, zRot);
    }
    update();
}

/**
 * Set the view to standard dimetric projection.
 */
void OpenGLWidgetQML::setDimetricView()
{
    xRot =  26.57 * 16;
    yRot = -45.00 * 16;
    zRot =  0;
    if (m_render) {
        m_render->setRotations(xRot, yRot, zRot);
    }
    update();
}

void OpenGLWidgetQML::setNumCubes(int numCubes)
{
    distance = 2 * numCubes;
    this->numCubes = numCubes;
    this->wr = nullptr;
    if (m_render) {
        m_render->setNumCubes(numCubes);
    }
    update();
}


void OpenGLWidgetQML::setNumColors(int numColors)
{
    this->numColors = numColors;
    this->colors = generateDistinctColors();
    directionFactors.resize(numColors);

    // Initialize direction factors randomly for each color
    for (int i = 0; i < numColors; i++) {
        directionFactors[i] = ((rand() % 2) == 0) ? 1.0f : -1.0f;
    }
    this->wr = nullptr;
}

void OpenGLWidgetQML::setDelayAnimation(int delayAnimation)
{
    this->delayAnimation = delayAnimation;
}


void OpenGLWidgetQML::setDistanceFactor(int factor)
{
    if (numCubes <= 0)
    {
        return;
    }
    factor = qMax(0, factor);
    distanceFactor = log(factor/10.0 + 1) * numCubes; // 5.0 - is a sensetivity factor. log - inroduce soft sizing
    qDebug() << "distanceFactor = " << distanceFactor;
    calculateScene();
    update();
}


void OpenGLWidgetQML::setPlotWireFrame(bool status)
{
    this->plotWireFrame = status;
    if (m_render) {
        m_render->setPlotWireFrame(status);
    }
}

/**
 * Capture a screenshot of the OpenGL widget and store it in a buffer.
 * @return A QImage containing the screenshot.
 */
QImage OpenGLWidgetQML::captureScreenshot()
{
    QImage screenshot;
    if (m_render)
    {
        screenshot = m_render->captureScreenshot();
    }

    return screenshot;
}

/**
 * Capture a screenshot and copy it to the system clipboard.
 */
void OpenGLWidgetQML::captureScreenshotToClipboard()
{
    QImage screenshot = captureScreenshotWithWhiteBackground();
    QClipboard *clipboard = QGuiApplication::clipboard();
    clipboard->setImage(screenshot);
}

/**
 * Capture a screenshot of the OpenGL widget, replace the background with white, and store it in a buffer.
 * @return A QImage containing the modified screenshot.
 */
QImage OpenGLWidgetQML::captureScreenshotWithWhiteBackground()
{
    QImage screenshot = captureScreenshot();

    // Modify the screenshot to replace gray background with white
    QImage modifiedScreenshot(screenshot.size(), QImage::Format_RGB32);
    const float tol = 0.015f;
    for (int y = 0; y < screenshot.height(); ++y) {
        for (int x = 0; x < screenshot.width(); ++x) {
            QColor pixelColor = screenshot.pixelColor(x, y);

            // Replace gray bg color with white
            if (
                fabs(pixelColor.redF() - bgColor.redF()) <= tol  &&
                fabs(pixelColor.greenF() - bgColor.greenF()) <= tol &&
                fabs(pixelColor.blueF() - bgColor.blueF()) <=tol
                )
            {
                modifiedScreenshot.setPixelColor(x, y, Qt::white);
            }
            else
            {
                modifiedScreenshot.setPixelColor(x, y, pixelColor);
            }
        }
    }

    return modifiedScreenshot;
}

void OpenGLWidgetQML::wheelEvent(QWheelEvent *event)
{
    int numDegrees = event->angleDelta().y() / 8;
    int numSteps = numDegrees / 10;

    if (numSteps > 0) {
        // zoom in
       zoomFactor *= 1.1f;
       distance -= numSteps * numCubes * 0.1f; // adjust the distance based on the number of steps
    } else if (numSteps < 0) {
        // zoom out
        zoomFactor /= 1.1f;
        distance += -numSteps * numCubes * 0.1f; // adjust the distance based on the number of steps (use the absolute value)
    }
    if (m_render) {
        m_render->setDistZoomFactor(distance, zoomFactor);
    }
    update(); // redraw the cube with the new camera position
}

void OpenGLWidgetQML::mousePressEvent(QMouseEvent *event)
{
    lastPos = event->pos();
}

void OpenGLWidgetQML::mouseMoveEvent(QMouseEvent *event)
{
    int dx = event->position().x() - lastPos.x();
    int dy = event->position().y() - lastPos.y();

    if (event->buttons() & Qt::LeftButton) {
        setXRotation(xRot + 8 * dy);
        setYRotation(yRot + 8 * dx);
    } else if (event->buttons() & Qt::RightButton) {
        setXRotation(xRot + 8 * dy);
        setZRotation(zRot + 8 * dx);
    }

    lastPos = event->pos();
}

void OpenGLWidgetQML::handleResize()
{
    if (m_render) 
    {
        m_render->setDevicePixelRatio(window() ? window()->devicePixelRatio() : 1.0f);
        m_render->resizeGL(this->width(), this->height());
    }
    update();
}



std::vector<std::array<GLubyte, 4>> OpenGLWidgetQML::createColorMap(int numLevels)
{
    std::vector<std::array<GLubyte, 4>> colorMap(9);

    colorMap[8] = {255, 0,      0,   255};
    colorMap[7] = {255, 178,    0,   255};
    colorMap[6] = {255, 255,    0,   255};
    colorMap[5] = {178, 255,    0,   255};
    colorMap[4] = {0,   255,    0,   255};
    colorMap[3] = {0,   255,    178, 255};
    colorMap[2] = {0,   255,    255, 255};
    colorMap[1] = {0,   178,    255, 255};
    colorMap[0] = {0,   0,      255, 255};

    return colorMap;
}

QVector<QColor> OpenGLWidgetQML::getColorMap(int numLevels)
{
    std::vector<std::array<GLubyte, 4>> vcmap = OpenGLWidgetQML::createColorMap(numLevels);
    QVector<QColor> cmap(numLevels);

    for (int i = 0; i < numLevels; i++)
    {
        QColor c;
        c.setRed(vcmap[i][0]);
        c.setGreen(vcmap[i][1]);
        c.setBlue(vcmap[i][2]);
        cmap[i] = c;
    }
    return cmap;
}

std::array<GLubyte, 4> OpenGLWidgetQML::scalarToColor(float value, const std::vector<std::array<GLubyte, 4>>& colorMap)
{
    int index = static_cast<int>(value * (colorMap.size() - 1));
    return colorMap[index];
}

void OpenGLWidgetQML::setAnsysWrapper(ansysWrapper *wr)
{
    this->wr = wr;
    this->calculateScene();
}

void OpenGLWidgetQML::setComponent(int index)
{
    this->plotComponent = index;
    this->calculateScene();
}

std::vector<std::array<GLubyte, 4>> OpenGLWidgetQML::generateDistinctColors()
{
    std::vector<std::array<GLubyte, 4>> colors;
    if (numColors <= 0) {
        return std::vector<std::array<GLubyte, 4>>();
    }
    float hueIncrement = 360.0f / numColors;


    for (int i = 0; i < numColors; ++i) {
        float hue = i * hueIncrement;
        float saturation = 1.0f;
        float value = 1.0f;
        float alpha = 0.85f;

        float chroma = saturation * value;
        float huePrime = hue / 60.0f;
        float x = chroma * (1.0f - std::abs(std::fmod(huePrime, 2.0f) - 1.0f));
        float r, g, b;

        if (huePrime >= 0 && huePrime < 1)
        {
            r = chroma;
            g = x;
            b = 0;
        } else if (huePrime >= 1 && huePrime < 2)
        {
            r = x;
            g = chroma;
            b = 0;
        } else if (huePrime >= 2 && huePrime < 3)
        {
            r = 0;
            g = chroma;
            b = x;
        } else if (huePrime >= 3 && huePrime < 4)
        {
            r = 0;
            g = x;
            b = chroma;
        } else if (huePrime >= 4 && huePrime < 5)
        {
            r = x;
            g = 0;
            b = chroma;
        } else
        {
            r = chroma;
            g = 0;
            b = x;
        }

        float m = value - chroma;
        r += m;
        g += m;
        b += m;

        colors.push_back({GLubyte(r*255), GLubyte(g*255), GLubyte(b*255), GLubyte(alpha*255)});
    }

    return colors;
}

void OpenGLWidgetQML::calculateScene()
{

    static const float node_coordinates[8][3] =
        {
            {0, 0, 0}, // 1
            {0, 0, 1}, // 5
            {0, 1, 1}, // 8
            {0, 1, 0}, // 4
            {1, 0, 0}, // 2
            {1, 0, 1}, // 6
            {1, 1, 1}, // 7
            {1, 1, 0}, // 3
        };

    voxelScene.clear();
    float cubeSize = 1.0; // numCubes;
    for (int i = 0; i < numCubes; i++) { // y
        for (int j = 0; j < numCubes; j++) { // z
            for (int k = 0; k < numCubes; k++) { // x

                assert(voxels[k][i][j] >= 0);

                if(voxels[k][i][j] == 0)
                {
                    continue;
                }

                const bool isExploded = (distanceFactor > 0.0f);

                bool neighbors[6] = {false, false, false, false, false, false};

                auto vx = voxels[k][i][j];
                // In exploded view, emit all non-enclosed faces
                if (isExploded)
                {
                    // Exploded view: cull face if neighbor is same color (==vx)
                    neighbors[0] = (k > 0)            && (voxels[k-1][i][j] == vx);  // -x
                    neighbors[2] = (k < numCubes - 1) && (voxels[k+1][i][j] == vx);  // +x
                    neighbors[1] = (i < numCubes - 1) && (voxels[k][i+1][j] == vx);  // +y
                    neighbors[3] = (i > 0)            && (voxels[k][i-1][j] == vx);  // -y
                    neighbors[4] = (j < numCubes - 1) && (voxels[k][i][j+1] == vx);  // +z
                    neighbors[5] = (j > 0)            && (voxels[k][i][j-1] == vx);  // -z
                }
                else
                {
                    // Solid view: cull face if neighbor is any occupied voxel (!=0)
                    neighbors[0] = (k > 0)            && (voxels[k-1][i][j] != 0);  // -x
                    neighbors[2] = (k < numCubes - 1) && (voxels[k+1][i][j] != 0);  // +x
                    neighbors[1] = (i < numCubes - 1) && (voxels[k][i+1][j] != 0);  // +y
                    neighbors[3] = (i > 0)            && (voxels[k][i-1][j] != 0);  // -y
                    neighbors[4] = (j < numCubes - 1) && (voxels[k][i][j+1] != 0);  // +z
                    neighbors[5] = (j > 0)            && (voxels[k][i][j-1] != 0);  // -z

                }
                // Skip fully enclosed voxels in both modes
                if (neighbors[0] && neighbors[1] && neighbors[2] &&
                    neighbors[3] && neighbors[4] && neighbors[5])
                    continue;


 /*               bool neighbors[6] = {false, false, false, false, false, false};


                auto vx = distanceFactor > 0 ? voxels[k][i][j] : 0; // if exploded view enabled draw all faces, otherwise => outer faces
                // Cull shared faces between same-type voxels only
                neighbors[0] = (k > 0)            && (voxels[k-1][i][j] == vx);
                neighbors[2] = (k < numCubes - 1) && (voxels[k+1][i][j] == vx);
                neighbors[1] = (i < numCubes - 1) && (voxels[k][i+1][j] == vx);
                neighbors[3] = (i > 0)            && (voxels[k][i-1][j] == vx);
                neighbors[4] = (j < numCubes - 1) && (voxels[k][i][j+1] == vx);
                neighbors[5] = (j > 0)            && (voxels[k][i][j-1] == vx);

                // Skip fully enclosed voxels entirely
                if (neighbors[0] && neighbors[1] && neighbors[2] &&
                    neighbors[3] && neighbors[4] && neighbors[5])
                    continue;
*/

 /*               bool neighbors[6] = {false};  // check for the same type voxels
                {
                    auto vx = voxels[k][i][j];
                    neighbors[0] = (k > 0) ? (voxels[k - 1][i][j] == vx) : false; // -x
                    neighbors[2] = (k < numCubes - 1) ? (voxels[k + 1][i][j] == vx) : false; // +x

                    neighbors[1] = (i < numCubes - 1) ? (voxels[k][i + 1][j] == vx): false; // +y
                    neighbors[3] = (i > 0) ? (voxels[k][i - 1][j] == vx): false; // -y


                    neighbors[4] = (j < numCubes - 1) ? (voxels[k][i][j + 1] == vx) : false; // +z
                    neighbors[5] = (j > 0) ? (voxels[k][i][j - 1] == vx): false; // -z

                    bool c1 = neighbors[0] && neighbors[1];
                    bool c2 = neighbors[2] && neighbors[3];
                    bool c3 = neighbors[4] && neighbors[5];
                    if (c1 && c2 && c3)
                        continue;
                }
*/
                size_t index = voxels[k][i][j] - 1;

                if (index >= colors.size() || colors.size() == 0)
                {
                    qCritical() << "Invalid voxel color index:" << index << "size:" << colors.size();
                    continue;
                }
                auto color = colors[index].data();


                GLfloat refColor[] = {1.0f, 1.0f, 1.0f};
                GLfloat diff[] = {  GLfloat(color[0]/255.0 - refColor[0]),
                    GLfloat(color[1]/255.0 - refColor[1]),
                    GLfloat(color[2]/255.0 - refColor[2])
                };

                // Define a direction factor, which can be negative or positive
                float directionFactor = directionFactors[index];

                GLfloat offset[] = {directionFactor * diff[0] * distanceFactor,
                                    directionFactor * diff[1] * distanceFactor,
                                    directionFactor * diff[2] * distanceFactor};

                RenderOpenGL::Voxel v;
                v.x = -(numCubes/2 - k) * cubeSize + offset[0];
                v.y = -(numCubes/2 - i) * cubeSize + offset[1];
                v.z = -(numCubes/2 - j) * cubeSize + offset[2];

                v.r = color[0];
                v.g = color[1];
                v.b = color[2];
                v.a = color[3];

                std::vector<std::array<GLubyte, 4>> node_colors(8);
                if (wr)
                {
                    auto cmap = this->createColorMap(0);
                    for (int l = 0; l < 8; l++)
                    {
                        n3d::node3d key;
                        key.data[0] = node_coordinates[l][0] + k;
                        key.data[1] = node_coordinates[l][1] + i;
                        key.data[2] = node_coordinates[l][2] + j;

                        float val = wr->getValByCoord(key, plotComponent);
                        float val01 = wr->scaleValue01(val, plotComponent);
                        node_colors[l] = this->scalarToColor(val01, cmap);
                    }
                }
                else
                {
                    std::fill(node_colors.begin(), node_colors.end(), colors[index]);
                }

                drawCube(cubeSize, v, neighbors, node_colors);
            }
        }
    }
    buildOrientationGlyphs();
    isVBOupdateRequired = true;
}

void OpenGLWidgetQML::setVoxels(int32_t*** voxels, short int numCubes)
{
    this->voxels = voxels;
    this->numCubes = numCubes;
    voxelScene.clear();
    calculateScene();
    if (m_render)
    {
        this->setNumCubes(numCubes);
        m_render->setDevicePixelRatio(window() ? window()->devicePixelRatio() : 1.0f);
        m_render->setDistZoomFactor(distance, zoomFactor);
        m_render->setNumCubes(numCubes);
        m_render->updateVoxelData(voxelScene);
        m_render->updateOrientationData(orientationVerts, orientationColors);
        m_render->resizeGL(this->width(), this->height());
    }
}

void OpenGLWidgetQML::updateVoxelColor(RenderOpenGL::Voxel &v1)
{
    if (wr)
    {
        float val = wr->getValByCoord(v1.x, v1.y, v1.z, SX);
        float val01 = wr->scaleValue01(val, SX);
        auto cmap = this->createColorMap(8);
        auto color = this->scalarToColor(val01, cmap);
        v1.r = color[0];
        v1.g = color[1];
        v1.b = color[2];
    }
}

void OpenGLWidgetQML::drawCube(short cubeSize, RenderOpenGL::Voxel vox, bool* neighbors, std::vector<std::array<GLubyte, 4>> &node_colors)
{

/*    static const GLfloat n[6][3] =
        {
            {-1.0, 0.0, 0.0}, // -x
            {0.0, 1.0, 0.0},  // y
            {1.0, 0.0, 0.0},  // x
            {0.0, -1.0, 0.0}, // -y
            {0.0, 0.0, 1.0},  // z
            {0.0, 0.0, -1.0}  // -z
        };
  */
    static const GLbyte n[6][3] = {
        {-127,    0,    0},  // -X
        {   0,  127,    0},  // +Y
        { 127,    0,    0},  // +X
        {   0, -127,    0},  // -Y
        {   0,    0,  127},  // +Z
        {   0,    0, -127}   // -Z
    };

    static const GLint faces[6][4] =
        {
            {0, 1, 2, 3},
            {3, 2, 6, 7},
            {7, 6, 5, 4},
            {4, 5, 1, 0},
            {5, 6, 2, 1},
            {7, 4, 0, 3}
        };
    float v[8][3];

    float offset = 0.0; // offset to help z-buffer distinc same quads

    v[0][0] = v[1][0] = v[2][0] = v[3][0] = vox.x + offset;
    v[4][0] = v[5][0] = v[6][0] = v[7][0] = vox.x + cubeSize - offset;
    v[0][1] = v[1][1] = v[4][1] = v[5][1] = vox.y + offset;
    v[2][1] = v[3][1] = v[6][1] = v[7][1] = vox.y + cubeSize - offset;
    v[0][2] = v[3][2] = v[4][2] = v[7][2] = vox.z + offset;
    v[1][2] = v[2][2] = v[5][2] = v[6][2] = vox.z + cubeSize - offset;

    RenderOpenGL::Voxel v1;
    for (int i = 0; i < 6; i++) // for each of 6 face of cube
    {
        if (neighbors[i])
            continue;

        v1.nx = n[i][0];
        v1.ny = n[i][1];
        v1.nz = n[i][2];

        if  (neighbors[i] == true) // plot hidden faces with gray (50.50.50) color for debug
        {
            v1.r = 50;
            v1.g = 50;
            v1.b = 50;
            v1.a = vox.a;
        }

        for (int j = 0; j < 4; j++) // for each node
        {
            auto fij = faces[i][j];
            v1.x = v[fij][0];
            v1.y = v[fij][1];
            v1.z = v[fij][2];

            v1.a = vox.a;

            v1.r = node_colors[fij][0];
            v1.g = node_colors[fij][1];
            v1.b = node_colors[fij][2];

            //qDebug() << i << j << fij << v1.r << v1.g << v1.b << v1.x <<v1.y << v1.z;

            voxelScene.push_back(v1);
        }

    }
}

void OpenGLWidgetQML::DelayFrameUpdate()
{
    QThread::msleep(delayAnimation);
}

int32_t*** OpenGLWidgetQML::getVoxels()
{
    return voxels;
}

void OpenGLWidgetQML::toggleDebugMode()
{
    qDebug() << "OpenGLWidgetQML::toggleDebugMode() - Called from QML";
    if (m_render) {
        qDebug() << "OpenGLWidgetQML::toggleDebugMode() - Calling renderer method";
        m_render->toggleDebugMode();
        update();
    } else {
        qWarning() << "OpenGLWidgetQML::toggleDebugMode() - ERROR: m_render is null!";
    }
}

void OpenGLWidgetQML::toggleFaceCulling()
{
    qDebug() << "OpenGLWidgetQML::toggleFaceCulling() - Called from QML";
    if (m_render) {
        m_render->toggleFaceCulling();
        update();
    } else {
        qWarning() << "OpenGLWidgetQML::toggleFaceCulling() - ERROR: m_render is null!";
    }
}

void OpenGLWidgetQML::toggleDepthTest()
{
    qDebug() << "OpenGLWidgetQML::toggleDepthTest() - Called from QML";
    if (m_render) {
        m_render->toggleDepthTest();
        update();
    } else {
        qWarning() << "OpenGLWidgetQML::toggleDepthTest() - ERROR: m_render is null!";
    }
}


void OpenGLWidgetQML::explodedValueChanged(double value)
{
    if (qFuzzyCompare(distanceFactor, (float)value))
        return;
    value = value < 1 ? 0: value;

    setDistanceFactor(value);

    if (m_render) {
        m_render->updateVoxelData(voxelScene);
       // m_render->updateVBO();
        qDebug() << "Exploede View Value Changed";

    } else {
        qWarning() << "OpenGLWidgetQML::explodedValueChanged() - ERROR: m_render is null!";
    }

}


// ── Orientation helpers ──────────────────────────────────────────────

void OpenGLWidgetQML::setGrainOrientations(
    const std::vector<std::array<float,3>>& orientations)
{
    grainOrientations = orientations;
    if (voxels)
        calculateScene();   // rebuild glyphs with new angles
}

void OpenGLWidgetQML::setShowOrientations(bool show)
{
    qDebug() << "setShowOrientations(); show = " << show;
    showOrientations = show;
    if (m_render) {
        m_render->setShowOrientations(show);
        update();
    }
}

void OpenGLWidgetQML::setOrientationGlyphScale(float scale)
{
    orientationGlyphScale = scale;
    if (voxels)
        calculateScene();
}

// ── Bunge ZXZ rotation matrix ─────────────────────────────────────────
// Returns column-major 3×3 stored as R[col][row] so that
//   R[0] = crystal-X axis in sample frame
//   R[1] = crystal-Y axis in sample frame
//   R[2] = crystal-Z axis in sample frame
static void bungeZXZ(float phi1, float Phi, float phi2,
                     float R[3][3])
{
    const float c1 = cosf(phi1), s1 = sinf(phi1);
    const float c  = cosf(Phi),  s  = sinf(Phi);
    const float c2 = cosf(phi2), s2 = sinf(phi2);

    // Standard Bunge ZXZ: R = Rz(phi1)·Rx(Phi)·Rz(phi2)
    // Row-major storage; R[i][j] is row i, col j
    R[0][0] =  c1*c2 - s1*s2*c;
    R[0][1] = -c1*s2 - s1*c2*c;
    R[0][2] =  s1*s;

    R[1][0] =  s1*c2 + c1*s2*c;
    R[1][1] = -s1*s2 + c1*c2*c;
    R[1][2] = -c1*s;

    R[2][0] =  s2*s;
    R[2][1] =  c2*s;
    R[2][2] =  c;
}

void OpenGLWidgetQML::buildOrientationGlyphs()
{
    orientationVerts.clear();
    orientationColors.clear();

    if (!showOrientations || grainOrientations.empty() || !voxels)
        return;

    // Accumulate centroid per grain ID
    // key = grain ID (1-based); value = {sum_x, sum_y, sum_z, count}
    std::unordered_map<int32_t, std::array<double,4>> acc;
    acc.reserve(static_cast<size_t>(numColors) + 1);

    for (int x = 0; x < numCubes; ++x)
        for (int y = 0; y < numCubes; ++y)
            for (int z = 0; z < numCubes; ++z) {
                int32_t id = voxels[x][y][z];
                if (id <= 0) continue;
                auto& a = acc[id];
                a[0] += x;  a[1] += y;  a[2] += z;  a[3] += 1.0;
            }

    // Colours for the three local axes
    static const GLubyte axisRGB[3][3] = {
        {230,  50,  50},   // crystal-X  ≈ red
        { 50, 200,  50},   // crystal-Y  ≈ green
        { 60, 130, 255},   // crystal-Z  ≈ blue
    };

    const float L   = orientationGlyphScale;
    const float half = static_cast<float>(numCubes) / 2.0f;

    auto pushVertex = [&](float x, float y, float z,
                          GLubyte r, GLubyte g, GLubyte b)
    {
        orientationVerts.push_back(x);
        orientationVerts.push_back(y);
        orientationVerts.push_back(z);
        orientationColors.push_back(r / 255.0f);
        orientationColors.push_back(g / 255.0f);
        orientationColors.push_back(b / 255.0f);
    };

    for (auto& [id, a] : acc) {
        const int idx = id - 1;   // 0-based
        if (idx < 0 || idx >= static_cast<int>(grainOrientations.size()))
            continue;

        // World-space centroid (same coordinate mapping as calculateScene)
        const float cx = -(half - static_cast<float>(a[0] / a[3]));
        const float cy = -(half - static_cast<float>(a[1] / a[3]));
        const float cz = -(half - static_cast<float>(a[2] / a[3]));

        const auto& eu = grainOrientations[static_cast<size_t>(idx)];
        const float phi1 = eu[0] * static_cast<float>(M_PI) / 180.0f;
        const float Phi  = eu[1] * static_cast<float>(M_PI) / 180.0f;
        const float phi2 = eu[2] * static_cast<float>(M_PI) / 180.0f;

        float R[3][3];
        bungeZXZ(phi1, Phi, phi2, R);

        // Three axes: column j of R gives unit vector of crystal-axis j
        for (int axis = 0; axis < 3; ++axis) {
            // R[row][col], col = axis
            const float dx = L * R[0][axis];
            const float dy = L * R[1][axis];
            const float dz = L * R[2][axis];

            const GLubyte r = axisRGB[axis][0];
            const GLubyte g = axisRGB[axis][1];
            const GLubyte b = axisRGB[axis][2];

            pushVertex(cx,      cy,      cz,      r, g, b);  // tail
            pushVertex(cx + dx, cy + dy, cz + dz, r, g, b);  // tip
        }
    }
}