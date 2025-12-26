#include "addplantpage.h"
#include "ui_addplantpage.h"

AddPlantPage::AddPlantPage(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AddPlantPage)
{
    ui->setupUi(this);
}

AddPlantPage::~AddPlantPage()
{
    delete ui;
}
