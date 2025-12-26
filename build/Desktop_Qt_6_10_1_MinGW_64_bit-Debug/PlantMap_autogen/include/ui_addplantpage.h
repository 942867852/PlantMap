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
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>

QT_BEGIN_NAMESPACE

class Ui_AddPlantPage
{
public:
    QLabel *label;
    QLineEdit *lineEdit;

    void setupUi(QDialog *AddPlantPage)
    {
        if (AddPlantPage->objectName().isEmpty())
            AddPlantPage->setObjectName("AddPlantPage");
        AddPlantPage->resize(889, 598);
        label = new QLabel(AddPlantPage);
        label->setObjectName("label");
        label->setGeometry(QRect(100, 120, 69, 19));
        lineEdit = new QLineEdit(AddPlantPage);
        lineEdit->setObjectName("lineEdit");
        lineEdit->setGeometry(QRect(300, 120, 113, 25));

        retranslateUi(AddPlantPage);

        QMetaObject::connectSlotsByName(AddPlantPage);
    } // setupUi

    void retranslateUi(QDialog *AddPlantPage)
    {
        AddPlantPage->setWindowTitle(QCoreApplication::translate("AddPlantPage", "Dialog", nullptr));
        label->setText(QCoreApplication::translate("AddPlantPage", "TextLabel", nullptr));
    } // retranslateUi

};

namespace Ui {
    class AddPlantPage: public Ui_AddPlantPage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_ADDPLANTPAGE_H
