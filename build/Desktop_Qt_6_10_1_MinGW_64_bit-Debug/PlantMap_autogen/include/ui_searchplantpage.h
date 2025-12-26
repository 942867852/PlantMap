/********************************************************************************
** Form generated from reading UI file 'searchplantpage.ui'
**
** Created by: Qt User Interface Compiler version 6.10.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_SEARCHPLANTPAGE_H
#define UI_SEARCHPLANTPAGE_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_SearchPlantPage
{
public:
    QPushButton *pushButton;
    QLabel *label;

    void setupUi(QWidget *SearchPlantPage)
    {
        if (SearchPlantPage->objectName().isEmpty())
            SearchPlantPage->setObjectName("SearchPlantPage");
        SearchPlantPage->resize(839, 558);
        pushButton = new QPushButton(SearchPlantPage);
        pushButton->setObjectName("pushButton");
        pushButton->setGeometry(QRect(350, 470, 93, 28));
        label = new QLabel(SearchPlantPage);
        label->setObjectName("label");
        label->setGeometry(QRect(170, 80, 69, 19));

        retranslateUi(SearchPlantPage);

        QMetaObject::connectSlotsByName(SearchPlantPage);
    } // setupUi

    void retranslateUi(QWidget *SearchPlantPage)
    {
        SearchPlantPage->setWindowTitle(QCoreApplication::translate("SearchPlantPage", "Form", nullptr));
        pushButton->setText(QCoreApplication::translate("SearchPlantPage", "PushButton", nullptr));
        label->setText(QCoreApplication::translate("SearchPlantPage", "TextLabel", nullptr));
    } // retranslateUi

};

namespace Ui {
    class SearchPlantPage: public Ui_SearchPlantPage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_SEARCHPLANTPAGE_H
