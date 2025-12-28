/********************************************************************************
** Form generated from reading UI file 'addplantpage.ui'
**
** Created by: Qt User Interface Compiler version 6.10.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_ADDPLANTPAGE_H
#define UI_ADDPLANTPAGE_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_AddPlantPage
{
public:
    QWidget *verticalLayoutWidget;
    QVBoxLayout *verticalLayout;
    QHBoxLayout *horizontalLayout_2;
    QLabel *label_3;
    QLineEdit *lineEdit_3;
    QHBoxLayout *horizontalLayout_6;
    QLabel *label_7;
    QLineEdit *lineEdit_9;
    QLabel *label_10;
    QLineEdit *lineEdit_10;
    QHBoxLayout *horizontalLayout_3;
    QLabel *label_4;
    QLineEdit *lineEdit_4;
    QHBoxLayout *horizontalLayout_4;
    QLabel *label_5;
    QLineEdit *lineEdit_5;
    QHBoxLayout *horizontalLayout_5;
    QLabel *label_6;
    QLineEdit *lineEdit_6;
    QSpacerItem *verticalSpacer;
    QWidget *horizontalLayoutWidget;
    QHBoxLayout *horizontalLayout;
    QLabel *label_2;
    QLineEdit *lineEdit_2;
    QLabel *label_8;
    QLineEdit *lineEdit_7;
    QLabel *label_9;
    QLineEdit *lineEdit_8;

    void setupUi(QDialog *AddPlantPage)
    {
        if (AddPlantPage->objectName().isEmpty())
            AddPlantPage->setObjectName("AddPlantPage");
        AddPlantPage->resize(889, 598);
        verticalLayoutWidget = new QWidget(AddPlantPage);
        verticalLayoutWidget->setObjectName("verticalLayoutWidget");
        verticalLayoutWidget->setGeometry(QRect(100, 280, 391, 241));
        verticalLayout = new QVBoxLayout(verticalLayoutWidget);
        verticalLayout->setObjectName("verticalLayout");
        verticalLayout->setContentsMargins(0, 0, 0, 0);
        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName("horizontalLayout_2");
        label_3 = new QLabel(verticalLayoutWidget);
        label_3->setObjectName("label_3");

        horizontalLayout_2->addWidget(label_3);

        lineEdit_3 = new QLineEdit(verticalLayoutWidget);
        lineEdit_3->setObjectName("lineEdit_3");

        horizontalLayout_2->addWidget(lineEdit_3);


        verticalLayout->addLayout(horizontalLayout_2);

        horizontalLayout_6 = new QHBoxLayout();
        horizontalLayout_6->setObjectName("horizontalLayout_6");
        label_7 = new QLabel(verticalLayoutWidget);
        label_7->setObjectName("label_7");

        horizontalLayout_6->addWidget(label_7);

        lineEdit_9 = new QLineEdit(verticalLayoutWidget);
        lineEdit_9->setObjectName("lineEdit_9");

        horizontalLayout_6->addWidget(lineEdit_9);

        label_10 = new QLabel(verticalLayoutWidget);
        label_10->setObjectName("label_10");

        horizontalLayout_6->addWidget(label_10);

        lineEdit_10 = new QLineEdit(verticalLayoutWidget);
        lineEdit_10->setObjectName("lineEdit_10");

        horizontalLayout_6->addWidget(lineEdit_10);


        verticalLayout->addLayout(horizontalLayout_6);

        horizontalLayout_3 = new QHBoxLayout();
        horizontalLayout_3->setObjectName("horizontalLayout_3");
        label_4 = new QLabel(verticalLayoutWidget);
        label_4->setObjectName("label_4");

        horizontalLayout_3->addWidget(label_4);

        lineEdit_4 = new QLineEdit(verticalLayoutWidget);
        lineEdit_4->setObjectName("lineEdit_4");

        horizontalLayout_3->addWidget(lineEdit_4);


        verticalLayout->addLayout(horizontalLayout_3);

        horizontalLayout_4 = new QHBoxLayout();
        horizontalLayout_4->setObjectName("horizontalLayout_4");
        label_5 = new QLabel(verticalLayoutWidget);
        label_5->setObjectName("label_5");

        horizontalLayout_4->addWidget(label_5);

        lineEdit_5 = new QLineEdit(verticalLayoutWidget);
        lineEdit_5->setObjectName("lineEdit_5");

        horizontalLayout_4->addWidget(lineEdit_5);


        verticalLayout->addLayout(horizontalLayout_4);

        horizontalLayout_5 = new QHBoxLayout();
        horizontalLayout_5->setObjectName("horizontalLayout_5");
        label_6 = new QLabel(verticalLayoutWidget);
        label_6->setObjectName("label_6");

        horizontalLayout_5->addWidget(label_6);

        lineEdit_6 = new QLineEdit(verticalLayoutWidget);
        lineEdit_6->setObjectName("lineEdit_6");

        horizontalLayout_5->addWidget(lineEdit_6);


        verticalLayout->addLayout(horizontalLayout_5);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout->addItem(verticalSpacer);

        horizontalLayoutWidget = new QWidget(AddPlantPage);
        horizontalLayoutWidget->setObjectName("horizontalLayoutWidget");
        horizontalLayoutWidget->setGeometry(QRect(80, 130, 411, 80));
        horizontalLayout = new QHBoxLayout(horizontalLayoutWidget);
        horizontalLayout->setObjectName("horizontalLayout");
        horizontalLayout->setContentsMargins(0, 0, 0, 0);
        label_2 = new QLabel(horizontalLayoutWidget);
        label_2->setObjectName("label_2");

        horizontalLayout->addWidget(label_2);

        lineEdit_2 = new QLineEdit(horizontalLayoutWidget);
        lineEdit_2->setObjectName("lineEdit_2");

        horizontalLayout->addWidget(lineEdit_2);

        label_8 = new QLabel(horizontalLayoutWidget);
        label_8->setObjectName("label_8");

        horizontalLayout->addWidget(label_8);

        lineEdit_7 = new QLineEdit(horizontalLayoutWidget);
        lineEdit_7->setObjectName("lineEdit_7");

        horizontalLayout->addWidget(lineEdit_7);

        label_9 = new QLabel(horizontalLayoutWidget);
        label_9->setObjectName("label_9");

        horizontalLayout->addWidget(label_9);

        lineEdit_8 = new QLineEdit(horizontalLayoutWidget);
        lineEdit_8->setObjectName("lineEdit_8");

        horizontalLayout->addWidget(lineEdit_8);


        retranslateUi(AddPlantPage);

        QMetaObject::connectSlotsByName(AddPlantPage);
    } // setupUi

    void retranslateUi(QDialog *AddPlantPage)
    {
        AddPlantPage->setWindowTitle(QCoreApplication::translate("AddPlantPage", "Dialog", nullptr));
        label_3->setText(QCoreApplication::translate("AddPlantPage", "\346\244\215\347\211\251\345\220\215\347\247\260", nullptr));
        label_7->setText(QCoreApplication::translate("AddPlantPage", "\350\212\261\346\234\237\345\247\213", nullptr));
        label_10->setText(QCoreApplication::translate("AddPlantPage", "\350\212\261\346\234\237\346\234\253", nullptr));
        label_4->setText(QCoreApplication::translate("AddPlantPage", "\346\244\215\347\211\251\345\210\206\347\261\273", nullptr));
        label_5->setText(QCoreApplication::translate("AddPlantPage", "TextLabel", nullptr));
        label_6->setText(QCoreApplication::translate("AddPlantPage", "TextLabel", nullptr));
        label_2->setText(QCoreApplication::translate("AddPlantPage", "TextLabel", nullptr));
        label_8->setText(QCoreApplication::translate("AddPlantPage", "TextLabel", nullptr));
        label_9->setText(QCoreApplication::translate("AddPlantPage", "TextLabel", nullptr));
    } // retranslateUi

};

namespace Ui {
    class AddPlantPage: public Ui_AddPlantPage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_ADDPLANTPAGE_H
