#include "mainwindow.h"
#include "./ui_mainwindow.h"


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    searchPage = new searchPlant();
    showPage = new showPlant();

    connect(ui->insertPlantBtn, &QPushButton::clicked, this, [this](){
        searchPage->show();
    });

    connect(ui->searchPlantBtn, &QPushButton::clicked, this, [this](){
        showPage->show();
    });

    // 核心：监听主窗口销毁信号，关闭子窗口
    connect(this, &MainWindow::close, this, [this](){
        // 此时searchPage仍有效，能正常调用close()
        if (searchPage && searchPage->isVisible()) {
            searchPage->close();
        }
        if (showPage && showPage->isVisible()) {
            showPage->close();
        }
    });

    // 备选：改用程序退出信号（适合整个程序退出时关闭所有窗口）
    // connect(qApp, &QApplication::aboutToQuit, this, [this](){
    //     if (searchPage) searchPage->close();
    //     if (showPage) showPage->close();
    // });
}

// void MainWindow::closeEvent(QCloseEvent *event)
// {
//     // 1. 检查searchPage是否存在且可见，存在则关闭
//     if (searchPage && searchPage->isVisible()) {
//         searchPage->close(); // 关闭窗口（会触发WA_DeleteOnClose释放内存）
//     }
//     // 2. 同理关闭showPage
//     if (showPage && showPage->isVisible()) {
//         showPage->close();
//     }
//     // 3. 允许主窗口正常关闭
//     event->accept();
// }


MainWindow::~MainWindow()
{
    delete ui;
    if (searchPage) delete searchPage;
    if (showPage) delete showPage;
}
