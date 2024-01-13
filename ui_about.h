/********************************************************************************
** Form generated from reading UI file 'about.ui'
**
** Created by: Qt User Interface Compiler version 6.4.3
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_ABOUT_H
#define UI_ABOUT_H

#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_About
{
public:
    QFrame *gridFrame;
    QGridLayout *gridLayout;
    QLabel *label;
    QLabel *label_2;
    QLabel *label_3;

    void setupUi(QWidget *About)
    {
        if (About->objectName().isEmpty())
            About->setObjectName("About");
        About->resize(800, 600);
        About->setMinimumSize(QSize(800, 600));
        About->setMaximumSize(QSize(16777215, 16777215));
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/icon/Plugin icon - 1icon.ico"), QSize(), QIcon::Normal, QIcon::Off);
        About->setWindowIcon(icon);
        About->setStyleSheet(QString::fromUtf8("background-color: #282828;\n"
"border: none;"));
        gridFrame = new QFrame(About);
        gridFrame->setObjectName("gridFrame");
        gridFrame->setGeometry(QRect(0, 0, 800, 600));
        gridFrame->setMinimumSize(QSize(800, 600));
        gridFrame->setMaximumSize(QSize(800, 600));
        gridFrame->setStyleSheet(QString::fromUtf8("border: none;"));
        gridLayout = new QGridLayout(gridFrame);
        gridLayout->setObjectName("gridLayout");
        label = new QLabel(gridFrame);
        label->setObjectName("label");
        label->setMinimumSize(QSize(650, 177));
        label->setMaximumSize(QSize(650, 177));
        label->setStyleSheet(QString::fromUtf8("color: #FFF;\n"
"font-family: Montserrat;\n"
"font-size: 22px;\n"
"font-style: normal;\n"
"font-weight: 600;\n"
"line-height: 133%; /* 29.26px */"));

        gridLayout->addWidget(label, 0, 0, 1, 1);

        label_2 = new QLabel(gridFrame);
        label_2->setObjectName("label_2");
        label_2->setMinimumSize(QSize(0, 224));
        label_2->setMaximumSize(QSize(16777215, 255));
        label_2->setStyleSheet(QString::fromUtf8("color: #FFF;\n"
"font-family: Inter;\n"
"font-size: 22px;\n"
"font-style: normal;\n"
"font-weight: 400;\n"
"line-height: 143%;"));

        gridLayout->addWidget(label_2, 1, 0, 1, 1);

        label_3 = new QLabel(gridFrame);
        label_3->setObjectName("label_3");
        label_3->setMinimumSize(QSize(510, 59));
        label_3->setMaximumSize(QSize(510, 59));
        label_3->setStyleSheet(QString::fromUtf8("color: #FFF;\n"
"font-family: Inter;\n"
"font-size: 22px;\n"
"font-style: normal;\n"
"font-weight: 600;\n"
"line-height: 133%; /* 29.26px */"));

        gridLayout->addWidget(label_3, 2, 0, 1, 1);


        retranslateUi(About);

        QMetaObject::connectSlotsByName(About);
    } // setupUi

    void retranslateUi(QWidget *About)
    {
        About->setWindowTitle(QCoreApplication::translate("About", "About", nullptr));
        label->setText(QCoreApplication::translate("About", "<font color='#06B09E'><b style='font-size: 24px;'>MaterialViz</b></font> - this is a computer application for studying<br> \n"
"the structure of a material, which implements four algorithms<br>\n"
"for analyzing the structure of a material: Moore, von Neumann,<br> \n"
"probability circle, probability ellipse.\n"
"<p>A detailed overview of the material structure using<br> visualization<p>", nullptr));
        label_2->setText(QCoreApplication::translate("About", "<html><head/><body><p align=\"center\"><span style=\" font-size:24px; font-weight:700; color:#06b09e;\">Project team</span></p><p align=\"center\">Algorithm developer: Nikita Mityasov, Oleh Semenenko</p><p align=\"center\">Software developers: Oleh Semenenko, Valeriia Hritskova</p><p align=\"center\">Testers: Anastasiia Korzh, Hanna Khominich</p><p align=\"center\">UX/UA designers: Kateryna Skrynnyk, Violetta Katsylo</p><p align=\"center\">3D technology developer: Yulia Chepela</p><p align=\"center\">Mentors: Oleksii Vodka, Lyudmila Rozova</p></body></html>", nullptr));
        label_3->setText(QCoreApplication::translate("About", "<html><head/><body><p>Link to Github, where the project is hosted:<br/><a href=\"https://github.com/MME-NTU-KhPI/MatViz3D\"><span style=\" text-decoration: underline; color:#007af4;\"><font color='#06B09E'>https://github.com/MME-NTU-KhPI/MatViz3D<\\font></span></a></p></body></html>", nullptr));
    } // retranslateUi

};

namespace Ui {
    class About: public Ui_About {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_ABOUT_H
