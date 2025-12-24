#include "showplant.h"
#include "ui_showplant.h"

showPlant::showPlant(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::showPlant)
{
    ui->setupUi(this);
}

showPlant::~showPlant()
{
    delete ui;
}
