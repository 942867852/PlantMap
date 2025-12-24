#include "searchplant.h"
#include "ui_searchplant.h"

searchPlant::searchPlant(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::searchPlant)
{
    ui->setupUi(this);


}

searchPlant::~searchPlant()
{
    delete ui;
}
