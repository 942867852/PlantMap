#ifndef SEARCHPLANT_H
#define SEARCHPLANT_H

#include <QWidget>

namespace Ui {
class searchPlant;
}

class searchPlant : public QWidget
{
    Q_OBJECT

public:
    explicit searchPlant(QWidget *parent = nullptr);
    ~searchPlant();

private:
    Ui::searchPlant *ui;
};

#endif // SEARCHPLANT_H
