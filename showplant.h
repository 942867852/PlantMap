#ifndef SHOWPLANT_H
#define SHOWPLANT_H

#include <QWidget>

namespace Ui {
class showPlant;
}

class showPlant : public QWidget
{
    Q_OBJECT

public:
    explicit showPlant(QWidget *parent = nullptr);
    ~showPlant();

private:
    Ui::showPlant *ui;
};

#endif // SHOWPLANT_H
