
// myglwidget.cpp

#include <QtWidgets>
#include <QtOpenGLWidgets/QtOpenGLWidgets>
#include <QtOpenGLWidgets/QOpenGLWidget>
#include "myglwidget.h"
#include <cmath> // include for sin and cos functions
#include <vector>
#include <cstdlib>
#include <ctime>
#include <QMatrix4x4>
#include <QColor>
#include <array>





MyGLWidget::MyGLWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
    xRot = 0;
    yRot = 0;
    zRot = 0;
    distance = 2.0f;
    voxels = nullptr;
}

MyGLWidget::~MyGLWidget()
{
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
    numCubes_ = numCubes;
    update();
}

void MyGLWidget::setNumColors(int numColors)
{
    numColors_ = numColors;
    update();
}

void MyGLWidget::setDistanceFactor(float factor)
{
    distanceFactor = factor;
    update();
}



void MyGLWidget::initializeGL()
{
    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    f->glClearColor(0.21f, 0.21f, 0.21f, 1.0f);;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_COLOR_MATERIAL);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
//    glEnable(GL_LIGHT1);
//    glEnable(GL_LIGHT2);
//    glEnable(GL_LIGHT3);
//    glEnable(GL_LIGHT4);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    //glEnable(GL_LIGHT4);
    //glEnable(GL_CULL_FACE);

    // Specify the back face to be culled
    //glCullFace(GL_FRONT);

    GLfloat lightPos[] = { 0.0f, 0.0f, 0.0f, 0.0f };
    GLfloat lightAmb[] = { 0.7f, 0.7f, 0.7f, 1.0f };
//    GLfloat lightDif[] = { 0.5f, 0.5f, 0.5f, 1.0f };
//    GLfloat lightSpc[] = { 0.1f, 0.1f, 0.1f, 1.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmb);
//    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDif);
//    glLightfv(GL_LIGHT0, GL_SPECULAR, lightSpc);



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

std::vector<std::array<GLfloat, 4>> generateDistinctColors(int numColors) {
    std::vector<std::array<GLfloat, 4>> colors;
    float hueIncrement = 360.0f / numColors;

    for (int i = 0; i < numColors; ++i) {
        float hue = i * hueIncrement;
        float saturation = 1.0f;
        float value = 1.0f;
        float alpha = 0.6f;

        // Convert HSV to RGB
        float chroma = saturation * value;
        float huePrime = hue / 60.0f;
        float x = chroma * (1.0f - std::abs(std::fmod(huePrime, 2.0f) - 1.0f));
        float r, g, b;

        if (huePrime >= 0 && huePrime < 1) {
            r = chroma;
            g = x;
            b = 0;
        } else if (huePrime >= 1 && huePrime < 2) {
            r = x;
            g = chroma;
            b = 0;
        } else if (huePrime >= 2 && huePrime < 3) {
            r = 0;
            g = chroma;
            b = x;
        } else if (huePrime >= 3 && huePrime < 4) {
            r = 0;
            g = x;
            b = chroma;
        } else if (huePrime >= 4 && huePrime < 5) {
            r = x;
            g = 0;
            b = chroma;
        } else {
            r = chroma;
            g = 0;
            b = x;
        }

        float m = value - chroma;
        r += m;
        g += m;
        b += m;

        colors.push_back({r, g, b, alpha});
    }

    return colors;
}

void MyGLWidget::setVoxels(int16_t*** voxels, short int numCubes)
{
    this->voxels=voxels;
    this->numCubes=numCubes;
}

void MyGLWidget::paintGL()
{
    if (voxels == nullptr)
    {
        return;
    }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDepthFunc(GL_LESS);
    glLoadIdentity();
    glTranslatef(0.0, 0.0, -distance);
    glRotatef(xRot / 16.0, 1.0, 0.0, 0.0);
    glRotatef(yRot / 16.0, 0.0, 1.0, 0.0);
    glRotatef(zRot / 16.0, 0.0, 0.0, 1.0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    //srand(time(NULL));
    srand(1);

    // Define an array of 5 colors
//    GLfloat colors[7][4] = {{1.0, 0.0, 0.0, 0.8f},    // red
//                            {0.0, 1.0, 0.0, 0.8f},    // green
//                            {0.0, 0.0, 1.0, 0.8f},    // blue
//                            {1.0, 1.0, 0.0, 0.8f},    // yellow
//                            {0.0, 1.0, 1.0, 0.8f},    // magenta
//                            {1.0, 0.0, 1.0, 0.8f},    //
//                            {1.0, 1.0, 1.0, 0.8f}};   // white


    // Draw the small cubes
    //int numCubes = 5; // Number of small cubes in each dimension

    std::vector<std::array<GLfloat, 4>> colors = generateDistinctColors(numColors_);

    float cubeSize = 1.0 / numCubes_; // Size of each small cube

    GLfloat refColor[] = {1.0f, 1.0f, 1.0f, 1.0f};

    // Declare an array to store direction factors for each color
    float directionFactors[numColors_];

    // Initialize direction factors randomly for each color
    for (int i = 0; i < numColors_; i++) {
        directionFactors[i] = ((rand() % 2) == 0) ? 1.0f : -1.0f;
    }

    const int CubeSizeLimit = 30;
    for (int i = 0; i < numCubes_; i++) {
        for (int j = 0; j < numCubes_; j++) {
            for (int k = 0; k < numCubes_; k++) {

                if (numCubes_ > CubeSizeLimit && distanceFactor <= 0.1) // Drop invisible cubes
                {
                    int alpha = 1; // How many elemet will be shown
                    int lb = alpha;
                    int ub = numCubes_ - alpha;
                    bool i_cond = i > lb && i < ub;
                    bool j_cond = j > lb && j < ub;
                    bool k_cond = k > lb && k < ub;
                    if (i_cond && j_cond && k_cond)
                        continue;
                }

                if (voxels[k][i][j] > 0) {
                    //int numColors1_ = rand() % numColors_;
                    //int index = voxels[k][i][j] % numColors1_;
                    //int index = voxels[k][i][j]  % (rand() % numColors_); // Use the value in the array as an index for color selection

                    int index = voxels[k][i][j] % numColors_ % (rand()+1); // +1 to prevent devision by zero


                    //int index = voxels[k][i][j]  % 7;

                    //int index = rand() % numColors_;

                    GLfloat* color = colors[index].data();

                    GLfloat diff[] = {color[0] - refColor[0], color[1] - refColor[1], color[2] - refColor[2], color[3] - refColor[3]};

                    // Define a direction factor, which can be negative or positive
                    float directionFactor = directionFactors[index];

                    GLfloat offset[] = {directionFactor * diff[0] * distanceFactor,
                                        directionFactor * diff[1] * distanceFactor,
                                        directionFactor * diff[2] * distanceFactor};

                    glColor4fv(colors[index].data());
                    glPushMatrix();
                    glTranslatef((i + 0.5) * cubeSize - 0.5 + offset[0],
                                 (j + 0.5) * cubeSize - 0.5 + offset[1],
                                 (k + 0.5) * cubeSize - 0.5 + offset[2]);



                // Choose a random color from the array
                //int index = rand() % 7;




//                glColor4fv(colors[index]);
//                glPushMatrix();
//                glTranslatef((i + 0.5) * cubeSize - 0.5 + distanceFactor*i,
//                             (j + 0.5) * cubeSize - 0.5 + distanceFactor*j,
//                             (k + 0.5) * cubeSize - 0.5 + distanceFactor*k);




                //glColor3ub(rand() % 256, rand() % 256, rand() % 256); // Set a random RGB color

                // Draw the small cube
                glBegin(GL_QUADS);
                glNormal3b(-1.0f, 0.0f, 0.0f);
                glVertex3f(-cubeSize / 2.0, -cubeSize / 2.0, -cubeSize / 2.0);
                glVertex3f(-cubeSize / 2.0, -cubeSize / 2.0, cubeSize / 2.0);
                glVertex3f(-cubeSize / 2.0, cubeSize / 2.0, cubeSize / 2.0);
                glVertex3f(-cubeSize / 2.0, cubeSize / 2.0, -cubeSize / 2.0);
                glEnd();

                glBegin(GL_QUADS);
                glNormal3b(0.0f, 1.0f, 0.0f);
                glVertex3f(-cubeSize / 2.0, -cubeSize / 2.0, cubeSize / 2.0);
                glVertex3f(cubeSize / 2.0, -cubeSize / 2.0, cubeSize / 2.0);
                glVertex3f(cubeSize / 2.0, cubeSize / 2.0, cubeSize / 2.0);
                glVertex3f(-cubeSize / 2.0, cubeSize / 2.0, cubeSize / 2.0);
                glEnd();

                glBegin(GL_QUADS);
                glNormal3b(0.0f, -1.0f, 0.0f);
                glVertex3f(cubeSize / 2.0, -cubeSize / 2.0, cubeSize / 2.0);
                glVertex3f(cubeSize / 2.0, -cubeSize / 2.0, -cubeSize / 2.0);
                glVertex3f(cubeSize / 2.0, cubeSize / 2.0, -cubeSize / 2.0);
                glVertex3f(cubeSize / 2.0, cubeSize / 2.0, cubeSize / 2.0);
                glEnd();

                glBegin(GL_QUADS);
                glNormal3b(1.0f, 0.0f, 0.0f);
                glVertex3f(cubeSize / 2.0, -cubeSize / 2.0, -cubeSize / 2.0);
                glVertex3f(-cubeSize / 2.0, -cubeSize / 2.0, -cubeSize / 2.0);
                glVertex3f(-cubeSize / 2.0, cubeSize / 2.0, -cubeSize / 2.0);
                glVertex3f(cubeSize / 2.0, cubeSize / 2.0, -cubeSize / 2.0);
                glEnd();
                glBegin(GL_QUADS);
                glNormal3b(0.0f, 0.0f, -1.0f);
                glVertex3f(-cubeSize / 2.0, cubeSize / 2.0, -cubeSize / 2.0);
                glVertex3f(-cubeSize / 2.0, cubeSize / 2.0, cubeSize / 2.0);
                glVertex3f(cubeSize / 2.0, cubeSize / 2.0, cubeSize / 2.0);
                glVertex3f(cubeSize / 2.0, cubeSize / 2.0, -cubeSize / 2.0);
                glEnd();
                glBegin(GL_QUADS);
                glNormal3b(0.0f, 1.0f, 0.0f);
                glVertex3f(-cubeSize / 2.0, -cubeSize / 2.0, cubeSize / 2.0);
                glVertex3f(-cubeSize / 2.0, -cubeSize / 2.0, -cubeSize / 2.0);
                glVertex3f(cubeSize / 2.0, -cubeSize / 2.0, -cubeSize / 2.0);
                glVertex3f(cubeSize / 2.0, -cubeSize / 2.0, cubeSize / 2.0);
                glEnd();

                if (numCubes_ <= CubeSizeLimit)
                {
                    glColor3f(0.5f, 0.5f, 0.5f);
                    glLineWidth(3.0);

                    glBegin(GL_LINES);

                    // draw edges along x-axis
                    glVertex3f(-cubeSize / 2.0, -cubeSize / 2.0, -cubeSize / 2.0);
                    glVertex3f(cubeSize / 2.0, -cubeSize / 2.0, -cubeSize / 2.0);

                    glVertex3f(cubeSize / 2.0, -cubeSize / 2.0, -cubeSize / 2.0);
                    glVertex3f(cubeSize / 2.0, cubeSize / 2.0, -cubeSize / 2.0);

                    glVertex3f(cubeSize / 2.0, cubeSize / 2.0, -cubeSize / 2.0);
                    glVertex3f(-cubeSize / 2.0, cubeSize / 2.0, -cubeSize / 2.0);

                    glVertex3f(-cubeSize / 2.0, cubeSize / 2.0, -cubeSize / 2.0);
                    glVertex3f(-cubeSize / 2.0, -cubeSize / 2.0, -cubeSize / 2.0);

                    // draw edges along y-axis
                    glVertex3f(-cubeSize / 2.0, -cubeSize / 2.0, -cubeSize / 2.0);
                    glVertex3f(-cubeSize / 2.0, -cubeSize / 2.0, cubeSize / 2.0);

                    glVertex3f(-cubeSize / 2.0, -cubeSize / 2.0, cubeSize / 2.0);
                    glVertex3f(-cubeSize / 2.0, cubeSize / 2.0, cubeSize / 2.0);

                    glVertex3f(-cubeSize / 2.0, cubeSize / 2.0, cubeSize / 2.0);
                    glVertex3f(-cubeSize / 2.0, cubeSize / 2.0, -cubeSize / 2.0);

                    glVertex3f(-cubeSize / 2.0, cubeSize / 2.0, -cubeSize / 2.0);
                    glVertex3f(-cubeSize / 2.0, -cubeSize / 2.0, -cubeSize / 2.0);

                    // draw edges along z-axis
                    glVertex3f(-cubeSize / 2.0, -cubeSize / 2.0, cubeSize / 2.0);
                    glVertex3f(cubeSize / 2.0, -cubeSize / 2.0, cubeSize / 2.0);

                    glVertex3f(cubeSize / 2.0, -cubeSize / 2.0, cubeSize / 2.0);
                    glVertex3f(cubeSize / 2.0, cubeSize / 2.0, cubeSize / 2.0);

                    glVertex3f(cubeSize / 2.0, cubeSize / 2.0, cubeSize / 2.0);
                    glVertex3f(-cubeSize / 2.0, cubeSize / 2.0, cubeSize / 2.0);

                    glVertex3f(-cubeSize / 2.0, cubeSize / 2.0, cubeSize / 2.0);
                    glVertex3f(-cubeSize / 2.0, -cubeSize / 2.0, cubeSize / 2.0);

                    glEnd();
                }

                glPopMatrix();
                }
            }
        }
    }
}


void MyGLWidget::resizeGL(int width, int height)
{
    int side = qMin(width, height);
    glViewport(0, 0, width, height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    // Set up perspective projection using glFrustum
    GLfloat aspect = (GLfloat)width / (GLfloat)height;
    GLfloat fov = 45.0f;
    GLfloat nearVal = 0.01f;
    GLfloat farVal = 100.0f;
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
        distance -= numSteps * 0.1f; // adjust the distance based on the number of steps
    } else if (numSteps < 0) {
        // zoom out
        distance += -numSteps * 0.1f; // adjust the distance based on the number of steps (use the absolute value)
    }

    update(); // redraw the cube with the new camera position
}

void MyGLWidget::mousePressEvent(QMouseEvent *event)
{
    lastPos = event->pos();
}

void MyGLWidget::mouseMoveEvent(QMouseEvent *event)
{
    int dx = event->x() - lastPos.x();
    int dy = event->y() - lastPos.y();

    if (event->buttons() & Qt::LeftButton) {
        setXRotation(xRot + 8 * dy);
        setYRotation(yRot + 8 * dx);
    } else if (event->buttons() & Qt::RightButton) {
        setXRotation(xRot + 8 * dy);
        setZRotation(zRot + 8 * dx);
    }

    lastPos = event->pos();
}
