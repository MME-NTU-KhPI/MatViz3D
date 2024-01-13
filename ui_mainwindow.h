/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 6.4.3
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QWidget>
#include "myglwidget.h"

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QAction *actionOpen;
    QAction *actionNew;
    QAction *actionSave_as_image;
    QWidget *centralwidget;
    QGridLayout *gridLayout_2;
    QWidget *widget;
    QHBoxLayout *horizontalLayout;
    QSpacerItem *horizontalSpacer_7;
    QPushButton *IconMenu;
    QSpacerItem *horizontalSpacer_8;
    QPushButton *FileButton;
    QSpacerItem *horizontalSpacer_9;
    QPushButton *WindowButton;
    QSpacerItem *horizontalSpacer_10;
    QPushButton *statistics;
    QSpacerItem *horizontalSpacer_11;
    QPushButton *AboutButton;
    QSpacerItem *horizontalSpacer_13;
    QLabel *MaterialVis;
    QSpacerItem *horizontalSpacer_12;
    QFrame *Rectangle1;
    QGridLayout *gridLayout_3;
    MyGLWidget *myGLWidget;
    QGridLayout *gridLayout;
    QSpacerItem *horizontalSpacer;
    QFrame *frameAnimation;
    QWidget *backgrAnim;
    QGridLayout *gridLayout_9;
    QFrame *frame;
    QSlider *SliderAnimationSpeed;
    QLabel *label_2;
    QFrame *horizontalFrame;
    QHBoxLayout *horizontalLayout_3;
    QLabel *label_3;
    QCheckBox *checkBoxAnimation;
    QFrame *frame_2;
    QSlider *Rectangle10;
    QLabel *Exploded_view;
    QSpacerItem *horizontalSpacer_2;
    QSpacerItem *verticalSpacer_3;
    QWidget *DataWidget;
    QGridLayout *gridLayout_7;
    QWidget *verticalWidget;
    QGridLayout *gridLayout_8;
    QLineEdit *Rectangle9;
    QLabel *label;
    QComboBox *AlgorithmsBox;
    QLineEdit *Rectangle8;
    QLabel *Cube_size;
    QLabel *Number_of_points;
    QFrame *horizontalFrame1;
    QHBoxLayout *horizontalLayout_2;
    QPushButton *Start;
    QSpacerItem *horizontalSpacer_3;
    QSpacerItem *verticalSpacer_2;
    QSpacerItem *verticalSpacer;
    QWidget *ConsoleWidget;
    QGridLayout *gridLayout_6;
    QScrollArea *scrollArea;
    QWidget *scrollAreaWidgetContents;
    QGridLayout *gridLayout_5;
    QTextEdit *textEdit;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName("MainWindow");
        MainWindow->resize(1280, 832);
        MainWindow->setMinimumSize(QSize(1280, 832));
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/icon/Plugin icon - 1icon.ico"), QSize(), QIcon::Normal, QIcon::Off);
        MainWindow->setWindowIcon(icon);
        MainWindow->setStyleSheet(QString::fromUtf8("background-color: rgb(3, 6, 9);"));
        actionOpen = new QAction(MainWindow);
        actionOpen->setObjectName("actionOpen");
        actionNew = new QAction(MainWindow);
        actionNew->setObjectName("actionNew");
        actionSave_as_image = new QAction(MainWindow);
        actionSave_as_image->setObjectName("actionSave_as_image");
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName("centralwidget");
        centralwidget->setMinimumSize(QSize(1280, 832));
        centralwidget->setMaximumSize(QSize(5000, 5000));
        centralwidget->setStyleSheet(QString::fromUtf8("border: 0px;\n"
"background-color: rgb(40, 40, 40);"));
        gridLayout_2 = new QGridLayout(centralwidget);
        gridLayout_2->setSpacing(0);
        gridLayout_2->setObjectName("gridLayout_2");
        gridLayout_2->setContentsMargins(0, 0, 0, 3);
        widget = new QWidget(centralwidget);
        widget->setObjectName("widget");
        QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(widget->sizePolicy().hasHeightForWidth());
        widget->setSizePolicy(sizePolicy);
        widget->setMinimumSize(QSize(1280, 67));
        widget->setMaximumSize(QSize(10000000, 67));
        widget->setStyleSheet(QString::fromUtf8("background-color: rgb(40, 40, 40);\n"
"padding: 0px;"));
        horizontalLayout = new QHBoxLayout(widget);
        horizontalLayout->setObjectName("horizontalLayout");
        horizontalSpacer_7 = new QSpacerItem(40, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer_7);

        IconMenu = new QPushButton(widget);
        IconMenu->setObjectName("IconMenu");
        IconMenu->setMinimumSize(QSize(48, 48));
        IconMenu->setMaximumSize(QSize(48, 48));
        QIcon icon1;
        icon1.addFile(QString::fromUtf8(":/icon/iconMenu.png"), QSize(), QIcon::Normal, QIcon::Off);
        IconMenu->setIcon(icon1);
        IconMenu->setIconSize(QSize(48, 48));

        horizontalLayout->addWidget(IconMenu);

        horizontalSpacer_8 = new QSpacerItem(38, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer_8);

        FileButton = new QPushButton(widget);
        FileButton->setObjectName("FileButton");
        FileButton->setMinimumSize(QSize(55, 29));
        FileButton->setMaximumSize(QSize(55, 29));
        FileButton->setStyleSheet(QString::fromUtf8("QPushButton {\n"
"color: #CFCECE;\n"
"font-family: Montserrat;\n"
"font-size: 24px;\n"
"font-style: normal;\n"
"font-weight: 400;\n"
"line-height: normal;\n"
"margin: 0px;\n"
"}\n"
"\n"
"QPushButton:hover {\n"
"    background-color: rgb(47, 47, 47);\n"
"}\n"
"\n"
"QPushButton:pressed {\n"
"    background-color: rgb(49, 49, 49);\n"
"}"));

        horizontalLayout->addWidget(FileButton);

        horizontalSpacer_9 = new QSpacerItem(38, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer_9);

        WindowButton = new QPushButton(widget);
        WindowButton->setObjectName("WindowButton");
        WindowButton->setMinimumSize(QSize(101, 29));
        WindowButton->setMaximumSize(QSize(101, 29));
        WindowButton->setStyleSheet(QString::fromUtf8("QPushButton {\n"
"color: #CFCECE;\n"
"font-family: Montserrat;\n"
"font-size: 24px;\n"
"font-style: normal;\n"
"font-weight: 400;\n"
"line-height: normal;\n"
"margin: 0px;\n"
"}\n"
"\n"
"QPushButton:hover {\n"
"    background-color: rgb(47, 47, 47);\n"
"}\n"
"\n"
"QPushButton:pressed {\n"
"    background-color: rgb(49, 49, 49);\n"
"}"));

        horizontalLayout->addWidget(WindowButton);

        horizontalSpacer_10 = new QSpacerItem(40, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer_10);

        statistics = new QPushButton(widget);
        statistics->setObjectName("statistics");
        statistics->setMinimumSize(QSize(109, 29));
        statistics->setMaximumSize(QSize(109, 29));
        statistics->setStyleSheet(QString::fromUtf8("QPushButton {\n"
"color: #CFCECE;\n"
"font-family: Montserrat;\n"
"font-size: 24px;\n"
"font-style: normal;\n"
"font-weight: 400;\n"
"line-height: normal;\n"
"margin: 0px;\n"
"}\n"
"\n"
"QPushButton:hover {\n"
"    background-color: rgb(47, 47, 47);\n"
"}\n"
"\n"
"QPushButton:pressed {\n"
"    background-color: rgb(49, 49, 49);\n"
"}"));

        horizontalLayout->addWidget(statistics);

        horizontalSpacer_11 = new QSpacerItem(40, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer_11);

        AboutButton = new QPushButton(widget);
        AboutButton->setObjectName("AboutButton");
        AboutButton->setMinimumSize(QSize(75, 29));
        AboutButton->setMaximumSize(QSize(75, 29));
        AboutButton->setStyleSheet(QString::fromUtf8("QPushButton {\n"
"color: #CFCECE;\n"
"font-family: Montserrat;\n"
"font-size: 24px;\n"
"font-style: normal;\n"
"font-weight: 400;\n"
"line-height: normal;\n"
"margin: 0px;\n"
"}\n"
"\n"
"QPushButton:hover {\n"
"    background-color: rgb(47, 47, 47);\n"
"}\n"
"\n"
"QPushButton:pressed {\n"
"    background-color: rgb(49, 49, 49);\n"
"}"));

        horizontalLayout->addWidget(AboutButton);

        horizontalSpacer_13 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer_13);

        MaterialVis = new QLabel(widget);
        MaterialVis->setObjectName("MaterialVis");
        MaterialVis->setMinimumSize(QSize(190, 44));
        MaterialVis->setMaximumSize(QSize(190, 44));
        MaterialVis->setLayoutDirection(Qt::LeftToRight);
        MaterialVis->setStyleSheet(QString::fromUtf8("/* MaterialVis */\n"
"\n"
"font-family: 'Inter';\n"
"font-style: normal;\n"
"font-weight: 800;\n"
"font-size: 36px;\n"
"line-height: normal;\n"
"/* identical to box height */\n"
"\n"
"\n"
"color: #00897B;\n"
""));

        horizontalLayout->addWidget(MaterialVis);

        horizontalSpacer_12 = new QSpacerItem(40, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer_12);


        gridLayout_2->addWidget(widget, 0, 1, 1, 1);

        Rectangle1 = new QFrame(centralwidget);
        Rectangle1->setObjectName("Rectangle1");
        Rectangle1->setMinimumSize(QSize(1012, 546));
        Rectangle1->setMaximumSize(QSize(1600, 982));
        Rectangle1->setStyleSheet(QString::fromUtf8("position: absolute;\n"
"width: 1012px;\n"
"height: 791px;\n"
"left: 268px;\n"
"top: 41px;\n"
"border: none;\n"
"background: #363636;"));
        Rectangle1->setFrameShape(QFrame::StyledPanel);
        Rectangle1->setFrameShadow(QFrame::Raised);
        gridLayout_3 = new QGridLayout(Rectangle1);
        gridLayout_3->setSpacing(0);
        gridLayout_3->setObjectName("gridLayout_3");
        gridLayout_3->setSizeConstraint(QLayout::SetMaximumSize);
        gridLayout_3->setContentsMargins(0, 0, 0, 0);
        myGLWidget = new MyGLWidget(Rectangle1);
        myGLWidget->setObjectName("myGLWidget");
        myGLWidget->setMinimumSize(QSize(1012, 400));
        myGLWidget->setMaximumSize(QSize(1518, 982));
        myGLWidget->setLayoutDirection(Qt::LeftToRight);
        myGLWidget->setStyleSheet(QString::fromUtf8("border: 0px;\n"
"padding: 0px;"));
        gridLayout = new QGridLayout(myGLWidget);
        gridLayout->setSpacing(0);
        gridLayout->setObjectName("gridLayout");
        gridLayout->setSizeConstraint(QLayout::SetMaximumSize);
        gridLayout->setContentsMargins(0, 0, 0, 0);
        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout->addItem(horizontalSpacer, 1, 3, 1, 1);

        frameAnimation = new QFrame(myGLWidget);
        frameAnimation->setObjectName("frameAnimation");
        frameAnimation->setMinimumSize(QSize(402, 275));
        frameAnimation->setMaximumSize(QSize(402, 16777215));
        frameAnimation->setStyleSheet(QString::fromUtf8("background-color: transparent;"));
        frameAnimation->setFrameShape(QFrame::StyledPanel);
        frameAnimation->setFrameShadow(QFrame::Raised);
        backgrAnim = new QWidget(frameAnimation);
        backgrAnim->setObjectName("backgrAnim");
        backgrAnim->setGeometry(QRect(0, 0, 402, 273));
        backgrAnim->setMinimumSize(QSize(402, 273));
        backgrAnim->setMaximumSize(QSize(402, 273));
        backgrAnim->setStyleSheet(QString::fromUtf8("QWidget {\n"
"	background-color: rgba(40, 40, 40, 0.5);\n"
"	border-radius: 15px;\n"
"	border: 0.5px solid rgba(0, 0, 0, 0.21);\n"
"}"));
        gridLayout_9 = new QGridLayout(backgrAnim);
        gridLayout_9->setObjectName("gridLayout_9");
        frame = new QFrame(backgrAnim);
        frame->setObjectName("frame");
        frame->setMinimumSize(QSize(222, 18));
        frame->setMaximumSize(QSize(222, 18));
        frame->setStyleSheet(QString::fromUtf8("background: none;\n"
"border: none;"));
        frame->setFrameShape(QFrame::StyledPanel);
        frame->setFrameShadow(QFrame::Raised);
        SliderAnimationSpeed = new QSlider(frame);
        SliderAnimationSpeed->setObjectName("SliderAnimationSpeed");
        SliderAnimationSpeed->setGeometry(QRect(0, 0, 222, 18));
        SliderAnimationSpeed->setMinimumSize(QSize(222, 18));
        SliderAnimationSpeed->setMaximumSize(QSize(222, 18));
        SliderAnimationSpeed->setLayoutDirection(Qt::LeftToRight);
        SliderAnimationSpeed->setStyleSheet(QString::fromUtf8("QSlider {\n"
"	border: none;\n"
" }\n"
"\n"
"QSlider::groove{\n"
"	border: none;\n"
"	background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:0, stop:0 rgba(0, 86, 77, 0.5) , stop:1 rgba(0, 137, 123, 0.5));\n"
"    height: 18px;\n"
"    border-radius: 9px;\n"
" }\n"
"\n"
"QSlider::handle {\n"
"	border: none;\n"
"	image: url(:/icon/EllipseExplodedView.png);\n"
"	width: 17px;\n"
"	height: 17px;\n"
"}\n"
"\n"
"\n"
"\n"
"\n"
""));
        SliderAnimationSpeed->setMinimum(1);
        SliderAnimationSpeed->setMaximum(100);
        SliderAnimationSpeed->setSingleStep(1);
        SliderAnimationSpeed->setPageStep(1);
        SliderAnimationSpeed->setValue(50);
        SliderAnimationSpeed->setSliderPosition(50);
        SliderAnimationSpeed->setOrientation(Qt::Horizontal);
        SliderAnimationSpeed->setTickPosition(QSlider::TicksAbove);
        SliderAnimationSpeed->setTickInterval(0);

        gridLayout_9->addWidget(frame, 3, 0, 1, 1);

        label_2 = new QLabel(backgrAnim);
        label_2->setObjectName("label_2");
        label_2->setMinimumSize(QSize(151, 22));
        label_2->setMaximumSize(QSize(151, 22));
        label_2->setStyleSheet(QString::fromUtf8("font-family: 'Inter';\n"
"font-style: normal;\n"
"font-weight: 700;\n"
"font-size: 16px;\n"
"line-height: 19px;\n"
"padding-bottom: -30px;\n"
"color: #969696;\n"
"border: none;\n"
"background-color: transparent;"));

        gridLayout_9->addWidget(label_2, 2, 0, 1, 1);

        horizontalFrame = new QFrame(backgrAnim);
        horizontalFrame->setObjectName("horizontalFrame");
        horizontalFrame->setMinimumSize(QSize(0, 31));
        horizontalFrame->setMaximumSize(QSize(16777215, 40));
        horizontalFrame->setStyleSheet(QString::fromUtf8("background: none;\n"
"border: none;"));
        horizontalLayout_3 = new QHBoxLayout(horizontalFrame);
        horizontalLayout_3->setObjectName("horizontalLayout_3");
        label_3 = new QLabel(horizontalFrame);
        label_3->setObjectName("label_3");
        label_3->setMinimumSize(QSize(151, 22));
        label_3->setMaximumSize(QSize(151, 22));
        label_3->setStyleSheet(QString::fromUtf8("font-family: 'Inter';\n"
"font-style: normal;\n"
"font-weight: 700;\n"
"font-size: 16px;\n"
"line-height: 19px;\n"
"padding-bottom: -30px;\n"
"color: #969696;\n"
"border: none;\n"
"background-color: transparent;"));

        horizontalLayout_3->addWidget(label_3);

        checkBoxAnimation = new QCheckBox(horizontalFrame);
        checkBoxAnimation->setObjectName("checkBoxAnimation");
        checkBoxAnimation->setMinimumSize(QSize(59, 31));
        checkBoxAnimation->setMaximumSize(QSize(59, 35));
        checkBoxAnimation->setStyleSheet(QString::fromUtf8("QCheckBox {\n"
"	background-color: transparent;\n"
"}\n"
"\n"
"QCheckBox::indicator {\n"
"	width: 59px;\n"
"	height: 35px;\n"
"	background-color: transparent;\n"
"}\n"
"\n"
"QCheckBox::indicator:unchecked {\n"
"	image: url(:/icon/AniOFF.png);\n"
"}\n"
"QCheckBox::indicator:checked {\n"
"	image: url(:/icon/AniON.png);\n"
"}"));

        horizontalLayout_3->addWidget(checkBoxAnimation);


        gridLayout_9->addWidget(horizontalFrame, 4, 0, 1, 1);

        frame_2 = new QFrame(backgrAnim);
        frame_2->setObjectName("frame_2");
        frame_2->setMinimumSize(QSize(222, 18));
        frame_2->setMaximumSize(QSize(222, 18));
        frame_2->setStyleSheet(QString::fromUtf8("background: none;\n"
"border: none;"));
        frame_2->setFrameShape(QFrame::StyledPanel);
        frame_2->setFrameShadow(QFrame::Raised);
        Rectangle10 = new QSlider(frame_2);
        Rectangle10->setObjectName("Rectangle10");
        Rectangle10->setGeometry(QRect(0, 0, 222, 18));
        Rectangle10->setMinimumSize(QSize(222, 18));
        Rectangle10->setMaximumSize(QSize(222, 18));
        Rectangle10->setLayoutDirection(Qt::LeftToRight);
        Rectangle10->setStyleSheet(QString::fromUtf8("QSlider {\n"
"	border: none;\n"
" }\n"
"\n"
"QSlider::groove{\n"
"	border: none;\n"
"	background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:0, stop:0 rgba(0, 86, 77, 0.5) , stop:1 rgba(0, 137, 123, 0.5));\n"
"    height: 18px;\n"
"    border-radius: 9px;\n"
" }\n"
"\n"
"QSlider::handle {\n"
"	border: none;\n"
"	image: url(:/icon/EllipseExplodedView.png);\n"
"	width: 17px;\n"
"	height: 17px;\n"
"}\n"
"\n"
"\n"
"\n"
"\n"
""));
        Rectangle10->setMaximum(10);
        Rectangle10->setSingleStep(1);
        Rectangle10->setPageStep(1);
        Rectangle10->setValue(0);
        Rectangle10->setSliderPosition(0);
        Rectangle10->setOrientation(Qt::Horizontal);
        Rectangle10->setTickPosition(QSlider::TicksAbove);
        Rectangle10->setTickInterval(0);

        gridLayout_9->addWidget(frame_2, 1, 0, 1, 1);

        Exploded_view = new QLabel(backgrAnim);
        Exploded_view->setObjectName("Exploded_view");
        Exploded_view->setMinimumSize(QSize(167, 9));
        Exploded_view->setMaximumSize(QSize(167, 25));
        Exploded_view->setStyleSheet(QString::fromUtf8("font-family: 'Inter';\n"
"font-style: normal;\n"
"font-weight: 700;\n"
"font-size: 16px;\n"
"line-height: 19px;\n"
"padding-bottom: -30px;\n"
"color: #969696;\n"
"border: none;\n"
"background-color: transparent;"));
        Exploded_view->setMargin(0);

        gridLayout_9->addWidget(Exploded_view, 0, 0, 1, 1);


        gridLayout->addWidget(frameAnimation, 1, 4, 1, 1);

        horizontalSpacer_2 = new QSpacerItem(20, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);

        gridLayout->addItem(horizontalSpacer_2, 1, 5, 1, 1);

        verticalSpacer_3 = new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Fixed);

        gridLayout->addItem(verticalSpacer_3, 0, 2, 1, 1);

        DataWidget = new QWidget(myGLWidget);
        DataWidget->setObjectName("DataWidget");
        DataWidget->setMinimumSize(QSize(402, 488));
        DataWidget->setMaximumSize(QSize(402, 488));
        DataWidget->setStyleSheet(QString::fromUtf8("background-color: rgba(40, 40, 40, 0.24);\n"
"border-radius: 13px;\n"
"border: 0.5px solid rgba(0, 0, 0, 0.21);"));
        gridLayout_7 = new QGridLayout(DataWidget);
        gridLayout_7->setObjectName("gridLayout_7");
        verticalWidget = new QWidget(DataWidget);
        verticalWidget->setObjectName("verticalWidget");
        verticalWidget->setMinimumSize(QSize(226, 0));
        verticalWidget->setMaximumSize(QSize(300, 16777215));
        verticalWidget->setStyleSheet(QString::fromUtf8("background-color: transparent;\n"
"border: 0;"));
        gridLayout_8 = new QGridLayout(verticalWidget);
        gridLayout_8->setObjectName("gridLayout_8");
        Rectangle9 = new QLineEdit(verticalWidget);
        Rectangle9->setObjectName("Rectangle9");
        Rectangle9->setMinimumSize(QSize(224, 21));
        Rectangle9->setMaximumSize(QSize(224, 21));
        Rectangle9->setStyleSheet(QString::fromUtf8("background: #282828;\n"
"border: 1px solid #969696;\n"
"border-radius: 5px;\n"
"color: #969696;\n"
"padding: 4px 0;\n"
"padding-left: 4px;"));

        gridLayout_8->addWidget(Rectangle9, 3, 0, 1, 1);

        label = new QLabel(verticalWidget);
        label->setObjectName("label");
        label->setMinimumSize(QSize(104, 24));
        label->setMaximumSize(QSize(104, 24));
        label->setStyleSheet(QString::fromUtf8("font-family: 'Inter';\n"
"font-style: normal;\n"
"font-weight: 700;\n"
"font-size: 20px;\n"
"line-height: 24px;\n"
"\n"
"color:#969696;"));

        gridLayout_8->addWidget(label, 4, 0, 1, 1);

        AlgorithmsBox = new QComboBox(verticalWidget);
        AlgorithmsBox->addItem(QString());
        AlgorithmsBox->addItem(QString());
        AlgorithmsBox->addItem(QString());
        AlgorithmsBox->addItem(QString());
        AlgorithmsBox->addItem(QString());
        AlgorithmsBox->setObjectName("AlgorithmsBox");
        AlgorithmsBox->setMinimumSize(QSize(224, 21));
        AlgorithmsBox->setMaximumSize(QSize(224, 21));
        AlgorithmsBox->setStyleSheet(QString::fromUtf8("QComboBox {\n"
"    background: #282828;\n"
"    border: 1px solid #969696;\n"
"    border-radius: 5px;\n"
"    color: #969696;\n"
"    padding: 4px 0;\n"
"    padding-left: 4px;\n"
"}\n"
"\n"
"QComboBox QAbstractItemView {\n"
"    width: 140px;\n"
"    background-color: rgba(65, 65, 65, 1);\n"
"    color: #969696;\n"
"    font-family: Montserrat;\n"
"    font-size: 13px;\n"
"    font-style: normal;\n"
"    font-weight: 500;\n"
"    line-height: normal;\n"
"	border-top-right-radius: 0; \n"
"	border-top-left-radius: 0;\n"
"    border-bottom-right-radius: 5px; \n"
"	border-bottom-left-radius: 5px;\n"
"}\n"
"\n"
"QComboBox::drop-down {\n"
"    width: 15px;  \n"
"    height: 10px;\n"
"    subcontrol-origin: padding;\n"
"    border: none;\n"
"	top: 5px;  /* \320\222\321\226\320\264\321\201\321\202\320\260\320\275\321\214 \320\262\321\226\320\264 \320\262\320\265\321\200\321\205\320\275\321\214\320\276\320\263\320\276 \320\272\321\200\320\260\321\216 */\n"
"	right: 5px;\n"
"}\n"
"\n"
"QComboBox::down-arrow {\n"
"   "
                        " image: url(:/icon/roll.png);\n"
"    width: 15px;  \n"
"    height: 10px;\n"
"}\n"
"\n"
"\n"
"QComboBox QAbstractItemView:hover {\n"
"    color: #FFF;\n"
"    font-family: Montserrat;\n"
"    font-size: 13px;\n"
"    font-style: normal;\n"
"    font-weight: 500;\n"
"    line-height: normal;\n"
"}\n"
""));

        gridLayout_8->addWidget(AlgorithmsBox, 5, 0, 1, 1);

        Rectangle8 = new QLineEdit(verticalWidget);
        Rectangle8->setObjectName("Rectangle8");
        Rectangle8->setMinimumSize(QSize(224, 21));
        Rectangle8->setMaximumSize(QSize(224, 21));
        Rectangle8->setStyleSheet(QString::fromUtf8("background: #282828;\n"
"border: 1px solid #969696;\n"
"border-radius: 5px;\n"
"color: #969696;\n"
"padding: 4px 0;\n"
"padding-left: 4px;"));

        gridLayout_8->addWidget(Rectangle8, 1, 0, 1, 1);

        Cube_size = new QLabel(verticalWidget);
        Cube_size->setObjectName("Cube_size");
        Cube_size->setMinimumSize(QSize(103, 24));
        Cube_size->setMaximumSize(QSize(103, 24));
        Cube_size->setStyleSheet(QString::fromUtf8("/* Cube size: */\n"
"\n"
"\n"
"color: #969696;\n"
"font-family: Inter;\n"
"font-size: 20px;\n"
"font-style: normal;\n"
"font-weight: 700;\n"
"line-height: normal;"));

        gridLayout_8->addWidget(Cube_size, 0, 0, 1, 1);

        Number_of_points = new QLabel(verticalWidget);
        Number_of_points->setObjectName("Number_of_points");
        Number_of_points->setMinimumSize(QSize(178, 24));
        Number_of_points->setMaximumSize(QSize(178, 24));
        Number_of_points->setStyleSheet(QString::fromUtf8("/* Number of points: */\n"
"\n"
"color: #969696;\n"
"font-family: Inter;\n"
"font-size: 20px;\n"
"font-style: normal;\n"
"font-weight: 700;\n"
"line-height: normal;"));

        gridLayout_8->addWidget(Number_of_points, 2, 0, 1, 1);

        horizontalFrame1 = new QFrame(verticalWidget);
        horizontalFrame1->setObjectName("horizontalFrame1");
        horizontalFrame1->setMinimumSize(QSize(0, 55));
        horizontalFrame1->setMaximumSize(QSize(16777215, 65));
        horizontalFrame1->setStyleSheet(QString::fromUtf8("border: 0;"));
        horizontalLayout_2 = new QHBoxLayout(horizontalFrame1);
        horizontalLayout_2->setObjectName("horizontalLayout_2");
        Start = new QPushButton(horizontalFrame1);
        Start->setObjectName("Start");
        Start->setMinimumSize(QSize(97, 54));
        Start->setMaximumSize(QSize(111, 54));
        Start->setStyleSheet(QString::fromUtf8("padding: 15px;\n"
"\n"
"border-radius: 15px;\n"
"border: 1px solid #969696;\n"
"background: #282828;\n"
"\n"
"color: rgba(150, 150, 150, 0.50);\n"
"text-align: center;\n"
"font-family: Inter;\n"
"font-size: 20px;\n"
"font-style: normal;\n"
"font-weight: 700;\n"
"line-height: normal;\n"
"text-transform: uppercase;"));

        horizontalLayout_2->addWidget(Start);


        gridLayout_8->addWidget(horizontalFrame1, 6, 0, 1, 1);


        gridLayout_7->addWidget(verticalWidget, 0, 0, 1, 1);


        gridLayout->addWidget(DataWidget, 1, 2, 1, 1);

        horizontalSpacer_3 = new QSpacerItem(20, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);

        gridLayout->addItem(horizontalSpacer_3, 3, 0, 1, 1);

        verticalSpacer_2 = new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Fixed);

        gridLayout->addItem(verticalSpacer_2, 0, 4, 1, 1);

        verticalSpacer = new QSpacerItem(20, 474, QSizePolicy::Minimum, QSizePolicy::Fixed);

        gridLayout->addItem(verticalSpacer, 3, 2, 1, 1);


        gridLayout_3->addWidget(myGLWidget, 0, 0, 1, 1);


        gridLayout_2->addWidget(Rectangle1, 1, 1, 1, 1);

        ConsoleWidget = new QWidget(centralwidget);
        ConsoleWidget->setObjectName("ConsoleWidget");
        ConsoleWidget->setMinimumSize(QSize(1012, 240));
        ConsoleWidget->setMaximumSize(QSize(16777215, 240));
        ConsoleWidget->setStyleSheet(QString::fromUtf8("padding: 0px;\n"
"border: none;"));
        gridLayout_6 = new QGridLayout(ConsoleWidget);
        gridLayout_6->setObjectName("gridLayout_6");
        scrollArea = new QScrollArea(ConsoleWidget);
        scrollArea->setObjectName("scrollArea");
        scrollArea->setMinimumSize(QSize(0, 0));
        scrollArea->setStyleSheet(QString::fromUtf8("QScrollBar:vertical {\n"
"	border: none;\n"
"	background-color: rgb(54, 54, 54);\n"
"	padding: 0px;\n"
"	width: 14px;\n"
"	margin: 15px 0 15px 0;\n"
"	border-radius: 0px;\n"
"}\n"
"\n"
"QScrollBar::handle:vertical {\n"
"	background-color: rgb(95, 95, 95);\n"
"	min-height: 30px;\n"
"	border-radius: 7px;\n"
"}\n"
"\n"
"QScrollBar::handle:vertical:hover {\n"
"	background-color: rgb(100, 100, 100);\n"
"}\n"
"\n"
"QScrollBar::handle:vertical:pressed {\n"
"	background-color: rgb(105, 105, 105);\n"
"}\n"
"\n"
"QScrollBar::sub-line:vertical {\n"
"	border: none;\n"
"	background-color: rgb(54, 54, 54);\n"
"	height: 15px;\n"
"	border-top-left-radius: 7px;\n"
"	border-top-right-radius: 7px;\n"
"	subcontrol-position: top;\n"
"	subcontrol-origin: margin;\n"
"}\n"
"\n"
"QScrollBar::sub-line:vertical:hover {\n"
"	background-color: rgb(100, 100, 100);\n"
"}\n"
"\n"
"QScrollBar::sub-line:vertical:pressed {\n"
"	background-color: rgb(105, 105, 105);\n"
"}\n"
"\n"
"QScrollBar::add-line:vertical {\n"
"	border: none;\n"
"	backgroun"
                        "d-color: rgb(54, 54, 54);\n"
"	height: 15px;\n"
"	border-bottom-left-radius: 7px;\n"
"	border-bottom-right-radius: 7px;\n"
"	subcontrol-position: bottom;\n"
"	subcontrol-origin: margin;\n"
"}\n"
"\n"
"QScrollBar::add-line:vertical:hover {\n"
"	background-color: rgb(100, 100, 100);\n"
"}\n"
"\n"
"QScrollBar::add-line:vertical:pressed {\n"
"	background-color: rgb(105, 105, 105);\n"
"}\n"
"\n"
"QScrollBar::up-arrow:vertical, QScrollBar::down-arrow:vertical {\n"
"	background: none;\n"
"}\n"
"\n"
"QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {\n"
"	background: none;\n"
"}\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
""));
        scrollArea->setFrameShape(QFrame::NoFrame);
        scrollArea->setWidgetResizable(true);
        scrollAreaWidgetContents = new QWidget();
        scrollAreaWidgetContents->setObjectName("scrollAreaWidgetContents");
        scrollAreaWidgetContents->setGeometry(QRect(0, 0, 1262, 222));
        gridLayout_5 = new QGridLayout(scrollAreaWidgetContents);
        gridLayout_5->setObjectName("gridLayout_5");
        textEdit = new QTextEdit(scrollAreaWidgetContents);
        textEdit->setObjectName("textEdit");
        textEdit->setStyleSheet(QString::fromUtf8("color: #969696;\n"
"font-family: Inter;\n"
"font-size: 16px;\n"
"font-style: normal;\n"
"font-weight: 700;\n"
"line-height: normal;"));

        gridLayout_5->addWidget(textEdit, 0, 0, 1, 1);

        scrollArea->setWidget(scrollAreaWidgetContents);

        gridLayout_6->addWidget(scrollArea, 0, 0, 1, 1);


        gridLayout_2->addWidget(ConsoleWidget, 2, 1, 1, 1);

        MainWindow->setCentralWidget(centralwidget);
#if QT_CONFIG(shortcut)
        Exploded_view->setBuddy(Rectangle10);
        Cube_size->setBuddy(Rectangle8);
        Number_of_points->setBuddy(Rectangle9);
#endif // QT_CONFIG(shortcut)

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "MaterialViz3D", nullptr));
        actionOpen->setText(QCoreApplication::translate("MainWindow", "Open", nullptr));
        actionNew->setText(QCoreApplication::translate("MainWindow", "New", nullptr));
        actionSave_as_image->setText(QCoreApplication::translate("MainWindow", "Save as image", nullptr));
        IconMenu->setText(QString());
        FileButton->setText(QCoreApplication::translate("MainWindow", "File", nullptr));
        WindowButton->setText(QCoreApplication::translate("MainWindow", "Window", nullptr));
        statistics->setText(QCoreApplication::translate("MainWindow", "Statistics", nullptr));
        AboutButton->setText(QCoreApplication::translate("MainWindow", "About", nullptr));
        MaterialVis->setText(QCoreApplication::translate("MainWindow", "<html><head/><body><p>Material<span style=\" color:#00564d;\">Viz</span></p></body></html>", nullptr));
        label_2->setText(QCoreApplication::translate("MainWindow", "Animation speed", nullptr));
        label_3->setText(QCoreApplication::translate("MainWindow", "Animation on/off", nullptr));
        checkBoxAnimation->setText(QString());
        Exploded_view->setText(QCoreApplication::translate("MainWindow", "Exploded view", nullptr));
        label->setText(QCoreApplication::translate("MainWindow", "Algorithms:", nullptr));
        AlgorithmsBox->setItemText(0, QCoreApplication::translate("MainWindow", "Neumann", nullptr));
        AlgorithmsBox->setItemText(1, QCoreApplication::translate("MainWindow", "Radial", nullptr));
        AlgorithmsBox->setItemText(2, QCoreApplication::translate("MainWindow", "Probability Circle", nullptr));
        AlgorithmsBox->setItemText(3, QCoreApplication::translate("MainWindow", "Probability Ellipse", nullptr));
        AlgorithmsBox->setItemText(4, QCoreApplication::translate("MainWindow", "Moore", nullptr));

        Cube_size->setText(QCoreApplication::translate("MainWindow", "Cube size:", nullptr));
        Number_of_points->setText(QCoreApplication::translate("MainWindow", "Number of points:", nullptr));
        Start->setText(QCoreApplication::translate("MainWindow", "START", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
