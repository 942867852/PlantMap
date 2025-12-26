#ifndef SEARCHPLANTPAGE_H
#define SEARCHPLANTPAGE_H

#include <QWidget>

namespace Ui {
class SearchPlantPage;
}

class SearchPlantPage : public QWidget
{
    Q_OBJECT

public:
    explicit SearchPlantPage(QWidget *parent = nullptr);
    ~SearchPlantPage();

private:
    Ui::SearchPlantPage *ui;
};

#endif // SEARCHPLANTPAGE_H
