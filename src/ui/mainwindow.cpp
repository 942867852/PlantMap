#include "mainwindow.h"

#include <QAction>
#include <QCloseEvent>
#include <QDir>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <functional>

namespace {

constexpr int NodeIdRole = Qt::UserRole;

} // namespace

MainWindow::MainWindow(const QString& dataDir, QWidget* parent)
    : QMainWindow(parent)
    , m_dataDir(dataDir)
{
    m_dataFile = QDir(dataDir).filePath(QStringLiteral("plantmap.json"));
    m_document = new TaxonomyDocument;

    QDir().mkpath(m_dataDir);
    if (QFile::exists(m_dataFile)) {
        QString error;
        if (!m_document->loadFromFile(m_dataFile, &error)) {
            QMessageBox::warning(this, QStringLiteral("数据加载失败"),
                                 error + QStringLiteral("\n\n将创建一份空数据；请勿覆盖原文件。"));
        }
    }

    if (m_document->isEmpty()) {
        QString error;
        const int rootId = m_document->addNode(0, QStringLiteral("植物界"), &error);
        if (rootId > 0) {
            setStatus(QStringLiteral("已创建顶级分类“植物界”，新数据将在保存时写入 %1")
                          .arg(m_dataFile));
        } else {
            showError(QStringLiteral("初始化失败"), error);
        }
    }

    buildUi();
    rebuildTree();
    if (!m_document->roots().isEmpty())
        m_tree->setCurrentItem(m_tree->topLevelItem(0), 0);
    refreshActionState();
    setWindowTitle(QStringLiteral("植物图谱 PlantMap"));
    setStatus(QStringLiteral("数据文件：%1").arg(m_dataFile));
}

MainWindow::~MainWindow()
{
    delete m_document;
}

void MainWindow::buildUi()
{
    auto* toolbar = addToolBar(QStringLiteral("分类"));
    toolbar->setMovable(false);

    m_addAction = toolbar->addAction(QStringLiteral("添加下级分类"));
    m_renameAction = toolbar->addAction(QStringLiteral("重命名"));
    m_deleteAction = toolbar->addAction(QStringLiteral("删除"));
    toolbar->addSeparator();
    m_saveAction = toolbar->addAction(QStringLiteral("保存数据"));

    connect(m_addAction, &QAction::triggered, this, &MainWindow::addUnderSelected);
    connect(m_renameAction, &QAction::triggered, this, &MainWindow::renameSelected);
    connect(m_deleteAction, &QAction::triggered, this, &MainWindow::removeSelected);
    connect(m_saveAction, &QAction::triggered, this, &MainWindow::saveData);

    auto* menu = menuBar()->addMenu(QStringLiteral("数据"));
    menu->addAction(QStringLiteral("保存数据"), this, &MainWindow::saveData);
    menu->addAction(QStringLiteral("打开数据目录"), this, &MainWindow::openDataDir);
    menu->addSeparator();
    menu->addAction(QStringLiteral("退出"), this, &MainWindow::close);

    auto* central = new QWidget(this);
    auto* layout = new QHBoxLayout(central);
    layout->setContentsMargins(4, 4, 4, 4);

    auto* splitter = new QSplitter(Qt::Horizontal, central);
    layout->addWidget(splitter);
    setCentralWidget(central);

    auto* treePanel = new QWidget(splitter);
    auto* treeLayout = new QVBoxLayout(treePanel);
    treeLayout->setContentsMargins(0, 0, 0, 0);
    auto* treeCaption = new QLabel(
        QStringLiteral("分类树：界 → 门 → 纲 → 目 → 科 → 属 → 种 → 亚种\n"
                       "资料只能挂在“种 / 亚种”节点。"), treePanel);
    treeCaption->setWordWrap(true);
    treeLayout->addWidget(treeCaption);

    m_tree = new QTreeWidget(treePanel);
    m_tree->setColumnCount(3);
    m_tree->setHeaderLabels({ QStringLiteral("名称"),
                              QStringLiteral("等级"),
                              QStringLiteral("拉丁学名") });
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_tree->setUniformRowHeights(true);
    treeLayout->addWidget(m_tree);
    splitter->addWidget(treePanel);

    m_form = new SpeciesForm(splitter);
    m_form->setDocument(m_document);
    m_form->setDataDir(m_dataDir);
    splitter->addWidget(m_form);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 5);
    splitter->setSizes({ 420, 900 });

    connect(m_tree, &QTreeWidget::currentItemChanged,
            this, &MainWindow::onTreeSelectionChanged);
    connect(m_form, &SpeciesForm::infoSaved, this, &MainWindow::saveData);
    connect(m_form, &SpeciesForm::infoSaveError,
            this, [this](const QString& message) { setStatus(message); });
}

void MainWindow::rebuildTree(int selectNodeId)
{
    const QSignalBlocker blocker(m_tree);
    m_tree->clear();

    std::function<QTreeWidgetItem*(QTreeWidgetItem*, int)> addLevel =
        [&](QTreeWidgetItem* parentItem, int id) -> QTreeWidgetItem* {
        const TaxonNode* n = m_document->node(id);
        if (!n)
            return nullptr;

        auto* item = parentItem
            ? new QTreeWidgetItem(parentItem)
            : new QTreeWidgetItem(m_tree);
        item->setText(0, n->name);
        item->setText(1, TaxonRanks::displayName(n->rank));
        item->setText(2, n->hasInfo ? n->info.scientificName : QString());
        item->setData(0, NodeIdRole, n->id);

        for (int childId : n->childIds)
            addLevel(item, childId);

        if (selectNodeId == id)
            m_tree->setCurrentItem(item);
        return item;
    };

    for (int rootId : m_document->roots())
        addLevel(nullptr, rootId);

    m_tree->expandAll();
}

void MainWindow::onTreeSelectionChanged()
{
    m_form->showNode(selectedNodeId());
    refreshActionState();
}

int MainWindow::selectedNodeId() const
{
    QTreeWidgetItem* item = m_tree->currentItem();
    return item ? item->data(0, NodeIdRole).toInt() : 0;
}

void MainWindow::refreshActionState()
{
    const int id = selectedNodeId();
    const TaxonNode* n = id > 0 ? m_document->node(id) : nullptr;
    m_addAction->setEnabled(!n || TaxonRanks::canHaveChildren(n->rank));
    m_renameAction->setEnabled(n != nullptr);
    m_deleteAction->setEnabled(n != nullptr);

    if (!n) {
        m_addAction->setText(QStringLiteral("新建第一级分类（界）"));
    } else if (TaxonRanks::canHaveChildren(n->rank)) {
        m_addAction->setText(QStringLiteral("添加下级分类（%1）")
                                 .arg(TaxonRanks::displayName(
                                     TaxonRanks::nextLower(n->rank))));
    } else {
        m_addAction->setText(QStringLiteral("当前已是末级（%1）")
                                 .arg(TaxonRanks::displayName(n->rank)));
    }
}

void MainWindow::addUnderSelected()
{
    const int parentId = selectedNodeId();
    const TaxonNode* parentNode = parentId > 0 ? m_document->node(parentId) : nullptr;
    if (parentNode && !TaxonRanks::canHaveChildren(parentNode->rank))
        return;

    TaxonRank newRank = parentNode
        ? TaxonRanks::nextLower(parentNode->rank)
        : TaxonRank::Kingdom;

    const QString dialogTitle = parentNode
        ? QStringLiteral("添加下级分类")
        : QStringLiteral("新建第一级分类");
    const QString prompt = parentNode
        ? QStringLiteral("将在“%1（%2）”下新建“%3”节点。\n请输入分类名称：")
              .arg(parentNode->name, TaxonRanks::displayName(parentNode->rank),
                   TaxonRanks::displayName(newRank))
        : QStringLiteral("请输入顶级分类名称（等级：界）：");

    bool ok = false;
    const QString name = QInputDialog::getText(
        this, dialogTitle, prompt, QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || name.isEmpty())
        return;

    QString error;
    const int newId = m_document->addNode(parentId, name, &error);
    if (newId <= 0) {
        showError(dialogTitle, error);
        return;
    }

    // 新建的是“种 / 亚种”时，顺便收集拉丁学名，便于尽早发现重复。
    if (TaxonRanks::canHostPlantInfo(newRank)) {
        const QString latin = QInputDialog::getText(
            this, QStringLiteral("填写拉丁学名"),
            QStringLiteral("“%1”的拉丁学名（全库唯一，可稍后补填）：")
                .arg(name),
            QLineEdit::Normal, QString(), &ok).trimmed();
        if (ok && !latin.isEmpty()) {
            SpeciesInfo info;
            info.scientificName = latin;
            if (!m_document->setInfo(newId, info, &error))
                showError(QStringLiteral("拉丁学名冲突"), error);
        }
    }

    rebuildTree(newId);
    m_form->showNode(newId);
    refreshActionState();
    saveData();
}

void MainWindow::renameSelected()
{
    const int id = selectedNodeId();
    const TaxonNode* n = id > 0 ? m_document->node(id) : nullptr;
    if (!n)
        return;

    bool ok = false;
    const QString newName = QInputDialog::getText(
        this, QStringLiteral("重命名分类"),
        QStringLiteral("“%1”的新名称：").arg(n->name),
        QLineEdit::Normal, n->name, &ok).trimmed();
    if (!ok || newName.isEmpty() || newName == n->name)
        return;

    QString error;
    if (!m_document->renameNode(id, newName, &error)) {
        showError(QStringLiteral("重命名失败"), error);
        return;
    }
    rebuildTree(id);
    m_form->showNode(id);
    saveData();
}

int MainWindow::subtreeSize(int id) const
{
    int total = 1;
    for (int childId : m_document->childIdsOf(id))
        total += subtreeSize(childId);
    return total;
}

void MainWindow::removeSelected()
{
    const int id = selectedNodeId();
    const TaxonNode* n = id > 0 ? m_document->node(id) : nullptr;
    if (!n)
        return;

    const int total = subtreeSize(id);
    const QString question = QStringLiteral(
        "确定删除“%1”吗？\n\n将级联删除其下 %2 个分类节点，"
        "相关物种资料（描述、属性、拉丁学名）会一并删除。\n"
        "已复制的照片文件仍保留在数据目录中，不会自动删除。")
        .arg(n->name).arg(total);
    if (QMessageBox::question(this, QStringLiteral("确认删除"), question)
        != QMessageBox::Yes)
        return;

    QString error;
    if (!m_document->removeNode(id, &error)) {
        showError(QStringLiteral("删除失败"), error);
        return;
    }
    rebuildTree();
    m_form->showNode(0);
    refreshActionState();
    saveData();
}

void MainWindow::saveData()
{
    if (!m_document || m_document->isEmpty())
        return;
    QString error;
    if (!m_document->saveToFile(m_dataFile, &error)) {
        showError(QStringLiteral("保存失败"), error);
        return;
    }
    setStatus(QStringLiteral("已保存：%1").arg(m_dataFile));
}

void MainWindow::openDataDir()
{
    QDir().mkpath(m_dataDir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_dataDir));
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    saveData();
    event->accept();
}

void MainWindow::showError(const QString& title, const QString& message)
{
    QMessageBox::warning(this, title, message);
}

void MainWindow::setStatus(const QString& message)
{
    statusBar()->showMessage(message, 6000);
}
