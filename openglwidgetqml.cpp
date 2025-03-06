#include "openglwidgetqml.h"
#include "renderopengl.h"
#include "ansyswrapper.h"

#include <QTimer>
#include <QImage>
#include <QThread>

RenderOpenGL* OpenGLWidgetQML::m_render = nullptr;

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

}

QQuickFramebufferObject::Renderer *OpenGLWidgetQML::createRenderer() const {
    m_render = new RenderOpenGL();
    m_render->resizeGL(this->width(), this->height());
    return m_render;
}

OpenGLWidgetQML::~OpenGLWidgetQML()
{
    delete timer;
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
        m_render->setRotations(xRot, yRot, zRot);
        update();
    }
}

void OpenGLWidgetQML::setYRotation(int angle)
{
    qNormalizeAngle(angle);
    if (angle != yRot) {
        yRot = angle;
        m_render->setRotations(xRot, yRot, zRot);
        update();
    }
}

void OpenGLWidgetQML::setZRotation(int angle)
{
    qNormalizeAngle(angle);
    if (angle != zRot) {
        zRot = angle;
        m_render->setRotations(xRot, yRot, zRot);
        update();
    }
}

void OpenGLWidgetQML::setFrontView() {
    xRot = 0;
    yRot = 0;
    zRot = 0;
    m_render->setRotations(xRot, yRot, zRot);
    update();
}

void OpenGLWidgetQML::setBackView() {
    xRot = 0;
    yRot = 180 * 16;
    zRot = 0;
    m_render->setRotations(xRot, yRot, zRot);
    update();
}

void OpenGLWidgetQML::setTopView() {
    xRot = -90 * 16;
    yRot = 0;
    zRot = 0;
    m_render->setRotations(xRot, yRot, zRot);
    update();
}

void OpenGLWidgetQML::setBottomView() {
    xRot = 90 * 16;
    yRot = 0;
    zRot = 0;
    m_render->setRotations(xRot, yRot, zRot);
    update();
}

void OpenGLWidgetQML::setLeftView() {
    xRot = 0;
    yRot = 90 * 16;
    zRot = 0;
    m_render->setRotations(xRot, yRot, zRot);
    update();
}

void OpenGLWidgetQML::setRightView() {
    xRot = 0;
    yRot = -90 * 16;
    zRot = 0;
    m_render->setRotations(xRot, yRot, zRot);
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
    m_render->setRotations(xRot, yRot, zRot);
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
    m_render->setRotations(xRot, yRot, zRot);
    update();
}

void OpenGLWidgetQML::setNumCubes(int numCubes)
{
    distance = 2 * numCubes;
    this->numCubes = numCubes;
    this->wr = nullptr;
    m_render->setNumCubes(numCubes);
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
    distanceFactor = log(factor/10.0 + 1) * numCubes; // 5.0 - is a sensetivity factor. log - inroduce soft sizing
    calculateScene();
    update();
}


void OpenGLWidgetQML::setPlotWireFrame(bool status)
{
    this->plotWireFrame = status;
    m_render->setPlotWireFrame(status);
}

/**
 * Capture a screenshot of the OpenGL widget and store it in a buffer.
 * @return A QImage containing the screenshot.
 */
QImage OpenGLWidgetQML::captureScreenshot()
{

    // Get the dimensions of the widget
    int width = this->width();
    int height = this->height();

    // Create a buffer to store pixel data
    QImage screenshot(width, height, QImage::Format_RGBA8888);

    return screenshot;
}

/**
 * Capture a screenshot and copy it to the system clipboard.
 */
void OpenGLWidgetQML::captureScreenshotToClipboard()
{
 //   QImage screenshot = captureScreenshotWithWhiteBackground();
 //   QClipboard *clipboard = QGuiApplication::clipboard();
 //   clipboard->setImage(screenshot);
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

void OpenGLWidgetQML::resizeEvent(QResizeEvent *event)
{
    qDebug() << event->size();
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
    m_render->setDistZoomFactor(distance, zoomFactor);
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

                bool neighbors[6] = {false};  // check for the same type voxels
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

                int index = voxels[k][i][j] - 1;
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

                Voxel v;
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
    isVBOupdateRequired = true;
}

void OpenGLWidgetQML::setVoxels(int32_t*** voxels, short int numCubes)
{
    this->voxels = voxels;
    this->numCubes = numCubes;
    calculateScene();
}

void OpenGLWidgetQML::updateVoxelColor(Voxel &v1)
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

void OpenGLWidgetQML::drawCube(short cubeSize, Voxel vox, bool* neighbors, std::vector<std::array<GLubyte, 4>> &node_colors)
{

    static const GLfloat n[6][3] =
        {
            {-1.0, 0.0, 0.0}, // -x
            {0.0, 1.0, 0.0},  // y
            {1.0, 0.0, 0.0},  // x
            {0.0, -1.0, 0.0}, // -y
            {0.0, 0.0, 1.0},  // z
            {0.0, 0.0, -1.0}  // -z
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

    float offset = 1e-6f; // offset to help z-buffer distinc same quads

    v[0][0] = v[1][0] = v[2][0] = v[3][0] = vox.x + offset;
    v[4][0] = v[5][0] = v[6][0] = v[7][0] = vox.x + cubeSize - offset;
    v[0][1] = v[1][1] = v[4][1] = v[5][1] = vox.y + offset;
    v[2][1] = v[3][1] = v[6][1] = v[7][1] = vox.y + cubeSize - offset;
    v[0][2] = v[3][2] = v[4][2] = v[7][2] = vox.z + offset;
    v[1][2] = v[2][2] = v[5][2] = v[6][2] = vox.z + cubeSize - offset;

    Voxel v1;
    for (int i = 0; i < 6; i++) // for each of 6 face of cube
    {
        if (neighbors[i] )
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

void OpenGLWidgetQML::drawCube(float cubeSize, GLenum type)
{
    float cb2 = cubeSize / 2.0;

    static const GLfloat n[6][3] =
        {
            {-1.0, 0.0, 0.0},
            {0.0, 1.0, 0.0},
            {1.0, 0.0, 0.0},
            {0.0, -1.0, 0.0},
            {0.0, 0.0, 1.0},
            {0.0, 0.0, -1.0}
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
    GLfloat v[8][3];
    GLint i;

    v[0][0] = v[1][0] = v[2][0] = v[3][0] = -cb2;
    v[4][0] = v[5][0] = v[6][0] = v[7][0] =  cb2 ;
    v[0][1] = v[1][1] = v[4][1] = v[5][1] = -cb2;
    v[2][1] = v[3][1] = v[6][1] = v[7][1] =  cb2;
    v[0][2] = v[3][2] = v[4][2] = v[7][2] = -cb2;
    v[1][2] = v[2][2] = v[5][2] = v[6][2] =  cb2;

    for (i = 5; i >= 0; i--) {
        glBegin(type);

        glNormal3fv(&n[i][0]);
        glVertex3fv(&v[faces[i][0]][0]);
        glVertex3fv(&v[faces[i][1]][0]);
        glVertex3fv(&v[faces[i][2]][0]);
        glVertex3fv(&v[faces[i][3]][0]);
        glEnd();
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
