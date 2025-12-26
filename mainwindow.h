#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include <QCloseEvent>
#include <QMainWindow>
#include "addplantpage.h"
#include "searchplantpage.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void closeEvent(QCloseEvent *event) override;

private:
    Ui::MainWindow *ui;
    AddPlantPage *addPage;
    SearchPlantPage *searchPage;

};
#endif // MAINWINDOW_H
