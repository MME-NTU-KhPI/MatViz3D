
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




MyGLWidget::MyGLWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
    xRot = 0;
    yRot = 0;
    zRot = 0;
    distance = 5.0f;
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

void MyGLWidget::setVoxel(uint16_t*** voxels, short int numCubes)
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
    //srand(1);

    // Define an array of 5 colors
    GLfloat colors[7][4] = {{1.0, 0.0, 0.0, 0.8f},    // red
                            {0.0, 1.0, 0.0, 0.8f},    // green
                            {0.0, 0.0, 1.0, 0.8f},    // blue
                            {1.0, 1.0, 0.0, 0.8f},    // yellow
                            {0.0, 1.0, 1.0, 0.8f},    // magenta
                            {1.0, 0.0, 1.0, 0.8f},    //
                            {1.0, 1.0, 1.0, 0.8f}};   // white


    //int lastIndex = -1;
    // Draw the small cubes
    //int numCubes = 5; // Number of small cubes in each dimension
    float cubeSize = 1.0 / numCubes_; // Size of each small cube

    for (int i = 0; i < numCubes_; i++) {
        for (int j = 0; j < numCubes_; j++) {
            for (int k = 0; k < numCubes_; k++) {
                if (voxels[k][i][j] > 0) {
                    int index = voxels[k][i][j] % 7; // Use the value in the array as an index for color selection
                // Choose a random color from the array
                //int index = rand() % 7;
                glColor4fv(colors[index]);
                glPushMatrix();
                glTranslatef((i + 0.5) * cubeSize - 0.5 + distanceFactor*i,
                             (j + 0.5) * cubeSize - 0.5 + distanceFactor*j,
                             (k + 0.5) * cubeSize - 0.5 + distanceFactor*k);
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
    int numSteps = numDegrees / 15;

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

