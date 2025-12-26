#include "mainwindow.h"
#include "./ui_mainwindow.h"


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    addPage = new AddPlantPage();
    searchPage = new SearchPlantPage();

    connect(ui->insertPlantBtn, &QPushButton::clicked, this, [this](){
        searchPage->show();
    });

    connect(ui->searchPlantBtn, &QPushButton::clicked, this, [this](){
        addPage->show();
    });


}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // 1. 检查searchPage是否存在且可见，存在则关闭
    if (searchPage && searchPage->isVisible()) {
        searchPage->close(); // 关闭窗口（会触发WA_DeleteOnClose释放内存）
    }
    // 2. 同理关闭showPage
    if (addPage && addPage->isVisible()) {
        addPage->close();
    }
    // 3. 允许主窗口正常关闭
    event->accept();
}


MainWindow::~MainWindow()
{
    delete ui;
    if (searchPage) delete searchPage;
    if (addPage) delete addPage;
}
