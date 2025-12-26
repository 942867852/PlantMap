#ifndef ADDPLANTPAGE_H
#define ADDPLANTPAGE_H

#include <QDialog>

namespace Ui {
class AddPlantPage;
}

class AddPlantPage : public QDialog
{
    Q_OBJECT

public:
    explicit AddPlantPage(QWidget *parent = nullptr);
    ~AddPlantPage();

private:
    Ui::AddPlantPage *ui;
};

#endif // ADDPLANTPAGE_H
