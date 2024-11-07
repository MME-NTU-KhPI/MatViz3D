// myglwidget.cpp

#include <QtWidgets>
#include "myglwidget.h"
#include <cmath> // include for sin and cos functions
#include <vector>
#include <cstdlib>
#include <ctime>
#include <QMatrix4x4>
#include <QColor>
#include <array>
#include <QFileDialog>
#include <QTextStream>
#include <QDebug>


MyGLWidget::MyGLWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
    xRot = 0;
    yRot = 0;
    zRot = 0;
    distance = 2.0f;
    voxels = nullptr;
    wr = nullptr;
    timer = new QTimer(this);
    plotComponent = 0;
}

MyGLWidget::~MyGLWidget()
{
    delete timer;
}

QSize MyGLWidget::minimumSizeHint() const
{
    return QSize(50, 50);
}

QSize MyGLWidget::sizeHint() const
{
    return QSize(400, 400);
}

void qNormalizeAngle(int &angle)
{
    while (angle < -360 * 16)
        angle += 360 * 16;
    while (angle >= 360 * 16)
        angle -= 360 * 16;
}

void MyGLWidget::setXRotation(int angle)
{
    qNormalizeAngle(angle);
    if (angle != xRot) {
        xRot = angle;
        emit xRotationChanged(angle);
        update();
    }
}

void MyGLWidget::setYRotation(int angle)
{
    qNormalizeAngle(angle);
    if (angle != yRot) {
        yRot = angle;
        emit yRotationChanged(angle);
        update();
    }
}

void MyGLWidget::setZRotation(int angle)
{
    qNormalizeAngle(angle);
    if (angle != zRot) {
        zRot = angle;
        emit zRotationChanged(angle);
        update();
    }
}


void MyGLWidget::setNumCubes(int numCubes)
{
    distance = 2 * numCubes;
    this->numCubes = numCubes;
    this->wr = nullptr;
    update();
}


void MyGLWidget::setNumColors(int numColors)
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

void MyGLWidget::setDelayAnimation(int delayAnimation)
{
    this->delayAnimation = delayAnimation;
}


void MyGLWidget::setDistanceFactor(int factor)
{
    distanceFactor = log(factor/10.0 + 1) * numCubes; // 5.0 - is a sensetivity factor. log - inroduce soft sizing
    calculateScene();
    update();
}


void MyGLWidget::setPlotWireFrame(bool status)
{
    this->plotWireFrame = status;
}

void inline initLights()
{
    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    f->glClearColor(0.21f, 0.21f, 0.21f, 1.0f);

    // set up light colors (ambient, diffuse, specular)
    GLfloat lightKa[] = {.2f, .2f, .2f, 1.0f};  // ambient light
    GLfloat lightKd[] = {.7f, .7f, .7f, 1.0f};  // diffuse light
    GLfloat lightKs[] = {.75f, .75f, .75f, 1.0f};  // specular light
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightKa);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightKd);
    glLightfv(GL_LIGHT0, GL_SPECULAR, lightKs);

    glLightfv(GL_LIGHT1, GL_AMBIENT, lightKa);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, lightKd);
    glLightfv(GL_LIGHT1, GL_SPECULAR, lightKs);

    glLightfv(GL_LIGHT2, GL_AMBIENT, lightKa);
    glLightfv(GL_LIGHT2, GL_DIFFUSE, lightKd);
    glLightfv(GL_LIGHT2, GL_SPECULAR, lightKs);


    // position the light
    float lightPosZ[4] = {1, 0, 1, 0}; // directional light
    float lightPosX[4] = {1, 1, 0, 0}; // directional light
    float lightPosY[4] = {1, 1, 0, 0}; // directional light

    glLightfv(GL_LIGHT0, GL_POSITION, lightPosZ);
    glLightfv(GL_LIGHT1, GL_POSITION, lightPosX);
    glLightfv(GL_LIGHT2, GL_POSITION, lightPosY);

    glEnable(GL_LIGHT0);                        // MUST enable each light source after configuration
    glEnable(GL_LIGHT1);                        // MUST enable each light source after configuration
    glEnable(GL_LIGHT2);                        // MUST enable each light source after configuration

}

void MyGLWidget::initializeGL()
{

    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();

    glShadeModel(GL_SMOOTH);
    f->glEnable(GL_COLOR_MATERIAL);
    f->glEnable(GL_LIGHTING);
    f->glEnable(GL_LIGHT0);

    // enable /disable features
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    //glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_CULL_FACE);

    glEnable(GL_NORMALIZE); // normalize normals length to 1

    // track material ambient and diffuse from surface color, call it before glEnable(GL_COLOR_MATERIAL)
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_COLOR_MATERIAL);

    glClearColor(0.21f, 0.21f, 0.21f, 1.0);     // background color
    glClearStencil(0);                          // clear stencil buffer
    glClearDepth(numCubes);                         // 0 is near, 1 is far
    glDepthFunc(GL_LESS);

    glEnable(GL_LINE_SMOOTH); //smooth line drawing

    // TODO: fix transperancy
    glEnable(GL_ALPHA_TEST);
    glEnable(GL_BLEND); // enbale blending
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    initLights(); // introduce light sources

    // set the material properties
    GLfloat matAmb[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    GLfloat matDif[] = { 0.6f, 0.6f, 0.6f, 1.0f };
    GLfloat matSpc[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    GLfloat matShn[] = { 50.0f };

    glMaterialfv(GL_FRONT, GL_AMBIENT, matAmb);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, matDif);
    glMaterialfv(GL_FRONT, GL_SPECULAR, matSpc);
    glMaterialfv(GL_FRONT, GL_SHININESS, matShn);


}

std::vector<std::array<GLubyte, 4>> MyGLWidget::createColorMap(int numLevels)
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

QVector<QColor> MyGLWidget::getColorMap(int numLevels)
{
    std::vector<std::array<GLubyte, 4>> vcmap = MyGLWidget::createColorMap(numLevels);
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

std::array<GLubyte, 4> MyGLWidget::scalarToColor(float value, const std::vector<std::array<GLubyte, 4>>& colorMap)
{
    int index = static_cast<int>(value * (colorMap.size() - 1));
    return colorMap[index];
}

void MyGLWidget::setAnsysWrapper(ansysWrapper *wr)
{
    this->wr = wr;
    this->calculateScene();
}

void MyGLWidget::setComponent(int index)
{
    this->plotComponent = index;
    this->calculateScene();
}

std::vector<std::array<GLubyte, 4>> MyGLWidget::generateDistinctColors()
{
    std::vector<std::array<GLubyte, 4>> colors;
    float hueIncrement = 360.0f / numColors;

    for (int i = 0; i < numColors; ++i) {
        float hue = i * hueIncrement;
        float saturation = 1.0f;
        float value = 1.0f;
        float alpha = 0.85f;

        // Convert HSV to RGB
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

void MyGLWidget::calculateScene()
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

}

void MyGLWidget::setVoxels(int32_t*** voxels, short int numCubes)
{
    this->voxels = voxels;
    this->numCubes = numCubes;
    calculateScene();
}

void MyGLWidget::updateVoxelColor(Voxel &v1)
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

void MyGLWidget::drawCube(short cubeSize, Voxel vox, bool* neighbors, std::vector<std::array<GLubyte, 4>> &node_colors)
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

void MyGLWidget::drawCube(float cubeSize, GLenum type)
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

void MyGLWidget::update_function()
{
    update();
}

void MyGLWidget::paintGL()
{

    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    // clear buffer
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // save the initial ModelView matrix before modifying ModelView matrix
    glPushMatrix();

    glLoadIdentity();
    glTranslatef(0.0, 0.0, -distance);
    glRotatef(xRot / 16.0, 1.0, 0.0, 0.0);
    glRotatef(yRot / 16.0, 0.0, 1.0, 0.0);
    glRotatef(zRot / 16.0, 0.0, 0.0, 1.0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    glEnable(GL_NORMALIZE);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);

    // Draw X-axis (Red)
    glColor3f(1.0, 0.0, 0.0);
    glBegin(GL_LINES);
    glVertex3f(-numCubes/2, -numCubes/2, -numCubes/2);
    glVertex3f(1.15*numCubes, -numCubes/2, -numCubes/2);
    glEnd();

    // Draw Y-axis (Green)
    glColor3f(0.0, 1.0, 0.0);
    glBegin(GL_LINES);
    glVertex3f(-numCubes/2, -numCubes/2, -numCubes/2);
    glVertex3f(-numCubes/2, 1.15*numCubes, -numCubes/2);
    glEnd();

    // Draw Z-axis (Blue)
    glColor3f(0.0, 0.0, 1.0);
    glBegin(GL_LINES);
    glVertex3f(-numCubes/2, -numCubes/2, -numCubes/2);
    glVertex3f(-numCubes/2, -numCubes/2, 1.15*numCubes);
    glEnd();


    // Create buffers for vertex, color, and normal data
    GLuint vboIds[3];
    f->glGenBuffers(3, vboIds);

    // Bind the vertex buffer
    f->glBindBuffer(GL_ARRAY_BUFFER, vboIds[0]);
    f->glBufferData(GL_ARRAY_BUFFER, voxelScene.size() * sizeof(Voxel), &voxelScene[0], GL_STATIC_DRAW);
    glVertexPointer(3, GL_FLOAT, sizeof(Voxel), 0);
    \

    if (plotWireFrame == true)
    {

        glEnable(GL_POLYGON_OFFSET_LINE); // Enable polygon offset for lines
        glPolygonOffset(-1.0f, -1.0f); // Set the polygon offset factor and units
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glLineWidth(0.1);

        for (size_t i = 0; i < voxelScene.size() ; i += 4)
        {
            glColor4f(0.5f, 0.5f, 0.5f, 0.5f);
            glDrawArrays(GL_POLYGON, i, 4); // plot 4 lines for each face
        }
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    // Bind the color buffer
    f->glBindBuffer(GL_ARRAY_BUFFER, vboIds[1]);
    f->glBufferData(GL_ARRAY_BUFFER, voxelScene.size() * sizeof(Voxel), &voxelScene[0].r, GL_STATIC_DRAW);
    glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(Voxel), 0);

    // Bind the normal buffer
    f->glBindBuffer(GL_ARRAY_BUFFER, vboIds[2]);
    f->glBufferData(GL_ARRAY_BUFFER, voxelScene.size() * sizeof(Voxel), &voxelScene[0].nx, GL_STATIC_DRAW);
    glNormalPointer(GL_BYTE, sizeof(Voxel), 0);

    // Draw the voxel scene using the VBOs
    glDrawArrays(GL_QUADS, 0, voxelScene.size()); // 24 vertices per voxel (6 faces * 4 vertices)
    if (plotWireFrame == true)
    {
        glColor3f(0.5f, 0.5f, 0.5f);
        for (size_t i = 0; i < voxelScene.size(); i += 4)
        {
            glDrawArrays(GL_LINE_LOOP, i, 4); // plot 4 lines for each face
        }
    }

    // Unbind the buffers and disable client-side capabilities
    f->glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);

    // Delete the VBOs
    f->glDeleteBuffers(3, vboIds);

    glPopMatrix();
}


void MyGLWidget::resizeGL(int width, int height)
{
    //   int side = qMin(width, height);
    glViewport(0, 0, width, height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    // Set up perspective projection using glFrustum
    GLfloat aspect = (GLfloat)width / (GLfloat)height;
    GLfloat fov = 45.0f;
    GLfloat nearVal = 1.0f;
    GLfloat farVal = 1000.0f;
    GLfloat top = nearVal * std::tan(fov * 0.5f * 3.14159f / 180.0f);
    GLfloat bottom = -top;
    GLfloat left = bottom * aspect;
    GLfloat right = top * aspect;

#ifdef QT_OPENGL_ES
    glFrustum(left, right, bottom, top, nearVal, farVal);
#else
    glFrustum(left, right, bottom, top, nearVal, farVal);
#endif
    glMatrixMode(GL_MODELVIEW);
}

void MyGLWidget::wheelEvent(QWheelEvent *event)
{
    int numDegrees = event->angleDelta().y() / 8;
    int numSteps = numDegrees / 10;

    if (numSteps > 0) {
        // zoom in
        distance -= numSteps * numCubes * 0.1f; // adjust the distance based on the number of steps
    } else if (numSteps < 0) {
        // zoom out
        distance += -numSteps * numCubes * 0.1f; // adjust the distance based on the number of steps (use the absolute value)
    }

    update(); // redraw the cube with the new camera position
}

void MyGLWidget::mousePressEvent(QMouseEvent *event)
{
    lastPos = event->pos();
}

void MyGLWidget::mouseMoveEvent(QMouseEvent *event)
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

void MyGLWidget::updateGLWidget(int32_t*** voxels, short int numCubes)
{
    setVoxels(voxels, numCubes);
    QThread::msleep(delayAnimation);
    repaint();
}


// Функція для підрахунку кількості вокселей кожного кольору
QVector<int> MyGLWidget::countVoxelColors()
{
    QVector<int> colorCounts(numColors, 0);

    for (int k = 0; k < numCubes; k++) {
        for (int i = 0; i < numCubes; i++) {
            for (int j = 0; j < numCubes; j++) {
                int color = voxels[k][i][j];
                if (color > 0 && color <= numColors) {
                    colorCounts[color - 1]++;
                }
            }
        }
    }
    return colorCounts;
}

void MyGLWidget::calculateSurfaceArea()
{
    qDebug() << "Total Surface Area for Color";
    QVector<int> colorCounts = countVoxelColors();

    for (int colorIndex = 0; colorIndex < numColors; colorIndex++) {
        int colorCount = colorCounts[colorIndex];

        if (colorCount > 0) {
            //qDebug() << "Color" << colorIndex + 1 << "Surface Area:";

            int totalSurfaceArea = 0;

            for (int k = 0; k < numCubes; k++) {
                for (int i = 0; i < numCubes; i++) {
                    for (int j = 0; j < numCubes; j++) {
                        int currentColor = voxels[k][i][j];

                        if (currentColor == colorIndex + 1) {
                            int surfaceArea = 0;

                            // Check the six faces of the voxel
                            if (i == 0 || voxels[k][i - 1][j] != colorIndex + 1) {
                                surfaceArea++; // left face
                            }
                            if (i == numCubes - 1 || voxels[k][i + 1][j] != colorIndex + 1) {
                                surfaceArea++; // right face
                            }
                            if (j == 0 || voxels[k][i][j - 1] != colorIndex + 1) {
                                surfaceArea++; // front face
                            }
                            if (j == numCubes - 1 || voxels[k][i][j + 1] != colorIndex + 1) {
                                surfaceArea++; // back face
                            }
                            if (k == 0 || voxels[k - 1][i][j] != colorIndex + 1) {
                                surfaceArea++; // bottom face
                            }
                            if (k == numCubes - 1 || voxels[k + 1][i][j] != colorIndex + 1) {
                                surfaceArea++; // top face
                            }

                            //qDebug() << "Voxel at (" << i << ", " << j << ", " << k << ") - Surface Area: " << surfaceArea;
                            totalSurfaceArea += surfaceArea;
                        }
                    }
                }
            }

            qDebug() << "Color" << colorIndex + 1 << ": " << totalSurfaceArea;
            //qDebug() << totalSurfaceArea;
        }
    }
}


//void MyGLWidget::exportVRML(const QString& filename, const std::vector<std::array<GLubyte, 4>>& colors) {
//    QFile file(filename);
//    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
//        qDebug() << "Cannot open file!";
//        return;
//    }

//    QTextStream out(&file);
//    out << "#VRML V2.0 utf8\n\n";

//    for (int k = 0; k < numCubes; k++) {
//        for (int i = 0; i < numCubes; i++) {
//            for (int j = 0; j < numCubes; j++) {
//                out << "Transform {\n";
//                out << "  translation " << k << " " << i << " " << j << "\n";
//                out << "  children Shape {\n";
//                out << "    appearance Appearance {\n";
//                out << "      material Material {\n";
//                out << "        diffuseColor " << (colors[voxels[k][i][j] - 1][0] / 255.0) << " "
//                    << (colors[voxels[k][i][j] - 1][1] / 255.0) << " "
//                    << (colors[voxels[k][i][j] - 1][2] / 255.0) << "\n";
//                out << "      }\n";
//                out << "    }\n";
//                out << "    geometry Box {\n";
//                out << "      size 1.0 1.0 1.0\n";
//                out << "    }\n";
//                out << "  }\n";
//                out << "}\n";
//            }
//        }
//    }
//    file.close();
//}

int32_t*** MyGLWidget::getVoxels()
{
    return voxels;
}


