#include "searchplantpage.h"
#include "ui_searchplantpage.h"

SearchPlantPage::SearchPlantPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::SearchPlantPage)
{
    ui->setupUi(this);
}

SearchPlantPage::~SearchPlantPage()
{
    delete ui;
}
