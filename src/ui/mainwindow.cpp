#include "mainwindow.h"

#include "speciesform.h"

#include <QAction>
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QPixmap>
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

// 资料编辑窗口：点右上角 X 或“取消”时，若有未保存改动则先询问。
class SpeciesEditDialog : public QDialog
{
public:
    SpeciesEditDialog(TaxonomyDocument* document,
                      const QString& dataDir,
                      int nodeId,
                      QWidget* parent = nullptr)
        : QDialog(parent)
    {
        const TaxonNode* n = document->node(nodeId);
        setWindowTitle(QStringLiteral("编辑资料：%1（%2）")
                           .arg(n ? n->name : QString(),
                                n ? TaxonRanks::displayName(n->rank)
                                  : QString()));
        resize(760, 780);

        auto* layout = new QVBoxLayout(this);
        m_form = new SpeciesForm(this);
        m_form->setDocument(document);
        m_form->setDataDir(dataDir);
        m_form->showNode(nodeId);
        layout->addWidget(m_form);

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save
                                                 | QDialogButtonBox::Cancel,
                                             this);
        QPushButton* saveButton = buttons->button(QDialogButtonBox::Save);
        saveButton->setText(QStringLiteral("保存并关闭"));
        buttons->button(QDialogButtonBox::Cancel)
            ->setText(QStringLiteral("取消"));
        connect(saveButton, &QPushButton::clicked, this, [this] {
            if (m_form->requestSave())
                accept();
        });
        connect(buttons->button(QDialogButtonBox::Cancel),
                &QPushButton::clicked, this, &QDialog::reject);
        layout->addWidget(buttons);
    }

protected:
    void reject() override
    {
        if (m_form->hasUnsavedChanges()) {
            const QMessageBox::StandardButton answer =
                QMessageBox::warning(
                    this, QStringLiteral("有未保存的修改"),
                    QStringLiteral("当前编辑的内容还没有保存。\n\n"
                                   "确定要放弃这些修改并关闭吗？"),
                    QMessageBox::Yes | QMessageBox::No,
                    QMessageBox::No);
            if (answer != QMessageBox::Yes)
                return;
        }
        QDialog::reject();
    }

private:
    SpeciesForm* m_form = nullptr;
};

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
    if (!m_document->roots().isEmpty()) {
        std::function<QTreeWidgetItem*(QTreeWidgetItem*, bool)> findFirstPlantItem =
            [&](QTreeWidgetItem* item, bool withPhoto) -> QTreeWidgetItem* {
            const int id = item->data(0, NodeIdRole).toInt();
            const TaxonNode* n = m_document->node(id);
            if (n && n->hasInfo
                && (!withPhoto || !n->info.photos.isEmpty()))
                return item;
            for (int i = 0; i < item->childCount(); ++i) {
                if (QTreeWidgetItem* found =
                        findFirstPlantItem(item->child(i), withPhoto))
                    return found;
            }
            return nullptr;
        };

        QTreeWidgetItem* firstPlantItem = nullptr;
        for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
            firstPlantItem = findFirstPlantItem(m_tree->topLevelItem(i), true);
            if (firstPlantItem)
                break;
        }
        if (!firstPlantItem) {
            for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
                firstPlantItem =
                    findFirstPlantItem(m_tree->topLevelItem(i), false);
                if (firstPlantItem)
                    break;
            }
        }
        m_tree->setCurrentItem(firstPlantItem ? firstPlantItem
                                              : m_tree->topLevelItem(0), 0);
        // 展开当前选中的植物节点，让下面的“亚种”名称直接可见。
        if (QTreeWidgetItem* current = m_tree->currentItem()) {
            QTreeWidgetItem* parent = current->parent();
            while (parent) {
                parent->setExpanded(true);
                parent = parent->parent();
            }
            current->setExpanded(true);
        }
    }
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
    m_editAction = toolbar->addAction(QStringLiteral("编辑资料…"));
    m_renameAction = toolbar->addAction(QStringLiteral("重命名"));
    m_deleteAction = toolbar->addAction(QStringLiteral("删除"));
    toolbar->addSeparator();
    m_saveAction = toolbar->addAction(QStringLiteral("保存数据"));

    connect(m_addAction, &QAction::triggered, this, &MainWindow::addUnderSelected);
    connect(m_editAction, &QAction::triggered, this, &MainWindow::openSpeciesEditor);
    connect(m_renameAction, &QAction::triggered, this, &MainWindow::renameSelected);
    connect(m_deleteAction, &QAction::triggered, this, &MainWindow::removeSelected);
    connect(m_saveAction, &QAction::triggered, this, &MainWindow::saveData);

    auto* menu = menuBar()->addMenu(QStringLiteral("数据"));
    menu->addAction(QStringLiteral("保存数据"), this, &MainWindow::saveData);
    menu->addAction(QStringLiteral("打开数据目录"), this, &MainWindow::openDataDir);
    menu->addSeparator();
    menu->addAction(QStringLiteral("退出"), this, &MainWindow::close);

    auto* helpMenu = menuBar()->addMenu(QStringLiteral("帮助"));
    helpMenu->addAction(QStringLiteral("使用说明"), this, [this] {
        QMessageBox::information(
            this, QStringLiteral("使用说明"),
            QStringLiteral(
                "① 在左侧输入框按“中文名 / 拉丁学名 / 别名”搜索植物，"
                "或直接在分类树中点击植物。\n"
                "② 右侧上方会立即显示这棵植物的图片，"
                "下方列出全部属性（只读）。\n"
                "③ 要修改属性时，点击“编辑资料…”，"
                "修改后点“保存并关闭”。\n\n"
                "管理分类：选中节点后用工具栏的"
                "“添加下级分类 / 重命名 / 删除”。\n"
                "添加图片：在编辑资料窗口中点"
                "“添加照片…”，图片会自动复制到数据目录。"));
    });

    auto* central = new QWidget(this);
    auto* layout = new QHBoxLayout(central);
    layout->setContentsMargins(4, 4, 4, 4);

    auto* splitter = new QSplitter(Qt::Horizontal, central);
    splitter->setChildrenCollapsible(true);
    splitter->setHandleWidth(6);
    layout->addWidget(splitter);
    setCentralWidget(central);

    auto* treePanel = new QWidget(splitter);
    treePanel->setMinimumWidth(0);
    auto* treeLayout = new QVBoxLayout(treePanel);
    treeLayout->setContentsMargins(0, 0, 0, 0);

    m_searchEdit = new QLineEdit(treePanel);
    m_searchEdit->setPlaceholderText(
        QStringLiteral("搜索：中文名 / 拉丁学名 / 别名"));
    m_searchEdit->setClearButtonEnabled(true);
    treeLayout->addWidget(m_searchEdit);

    m_searchCaption = new QLabel(treePanel);
    m_searchCaption->setWordWrap(true);
    m_searchCaption->hide();
    treeLayout->addWidget(m_searchCaption);

    m_searchResults = new QListWidget(treePanel);
    m_searchResults->hide();
    treeLayout->addWidget(m_searchResults);

    m_treeCaption = new QLabel(
        QStringLiteral("① 点击“种 / 亚种”植物\n"
                       "② 右侧上方即时显示植物大图\n"
                       "③ 下方查看属性，修改请点“编辑资料…”\n\n"
                       "树按 界 → 门 → 纲 → 目 → 科 → 属 → 种 → 亚种 排列。"),
        treePanel);
    m_treeCaption->setWordWrap(true);
    treeLayout->addWidget(m_treeCaption);

    m_tree = new QTreeWidget(treePanel);
    m_tree->setMinimumWidth(0);
    m_tree->setColumnCount(3);
    m_tree->setHeaderLabels({ QStringLiteral("名称"),
                              QStringLiteral("等级"),
                              QStringLiteral("拉丁学名") });
    m_tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_tree->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_tree->setUniformRowHeights(true);
    treeLayout->addWidget(m_tree);
    splitter->addWidget(treePanel);

    m_view = new SpeciesViewForm(splitter);
    m_view->setDocument(m_document);
    m_view->setDataDir(m_dataDir);
    splitter->addWidget(m_view);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 5);
    splitter->setSizes({ 420, 900 });

    connect(m_tree, &QTreeWidget::currentItemChanged,
            this, &MainWindow::onTreeSelectionChanged);
    connect(m_view, &SpeciesViewForm::editRequested,
            this, &MainWindow::openSpeciesEditor);
    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &MainWindow::onSearchTextChanged);
    connect(m_searchResults, &QListWidget::itemClicked,
            this, [this](QListWidgetItem* item) {
        goToPlantFromSearch(item);
    });
    connect(m_searchResults, &QListWidget::itemActivated,
            this, [this](QListWidgetItem* item) {
        goToPlantFromSearch(item);
    });
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

    // 默认只展开到“目”，避免刚打开时分类树过长。
    std::function<void(QTreeWidgetItem*)> expandToOrder =
        [&](QTreeWidgetItem* item) {
        const int id = item->data(0, NodeIdRole).toInt();
        const TaxonNode* n = m_document->node(id);
        if (n
            && static_cast<int>(n->rank)
                   <= static_cast<int>(TaxonRank::Order)) {
            item->setExpanded(true);
        } else {
            item->setExpanded(false);
        }
        for (int i = 0; i < item->childCount(); ++i)
            expandToOrder(item->child(i));
    };
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        expandToOrder(m_tree->topLevelItem(i));

    // 有明确的选中节点时（如新增/搜索），把它的整条路径展开，方便看到位置。
    if (selectNodeId > 0) {
        std::function<QTreeWidgetItem*(QTreeWidgetItem*)> findItem =
            [&](QTreeWidgetItem* item) -> QTreeWidgetItem* {
            if (item->data(0, NodeIdRole).toInt() == selectNodeId)
                return item;
            for (int i = 0; i < item->childCount(); ++i) {
                if (QTreeWidgetItem* found = findItem(item->child(i)))
                    return found;
            }
            return nullptr;
        };

        QTreeWidgetItem* target = nullptr;
        for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
            target = findItem(m_tree->topLevelItem(i));
            if (target)
                break;
        }
        if (target) {
            QTreeWidgetItem* p = target->parent();
            while (p) {
                p->setExpanded(true);
                p = p->parent();
            }
            target->setExpanded(true);
            m_tree->scrollToItem(target);
        }
    }
}

void MainWindow::onTreeSelectionChanged()
{
    m_view->showNode(selectedNodeId());
    refreshActionState();
}

void MainWindow::onSearchTextChanged(const QString& text)
{
    const QString query = text.trimmed();
    const bool searching = !query.isEmpty();

    m_searchResults->setVisible(searching);
    m_searchCaption->setVisible(searching);
    m_tree->setVisible(!searching);
    m_treeCaption->setVisible(!searching);

    if (!searching)
        return;

    m_searchResults->clear();
    const QString normalizedQuery =
        TaxonomyDocument::normalizedScientificName(query);

    std::function<void(int)> visit = [&](int parentId) {
        for (int childId : m_document->childIdsOf(parentId)) {
            const TaxonNode* n = m_document->node(childId);
            if (n && TaxonRanks::canHostPlantInfo(n->rank)) {
                bool matched =
                    n->name.contains(query, Qt::CaseInsensitive);
                if (!matched && !normalizedQuery.isEmpty()
                    && n->hasInfo && !n->info.scientificName.isEmpty()
                    && TaxonomyDocument::normalizedScientificName(
                           n->info.scientificName)
                           .contains(normalizedQuery)) {
                    matched = true;
                }
                if (!matched) {
                    for (const QString& alias : n->info.aliases) {
                        if (!alias.trimmed().isEmpty()
                            && alias.contains(query, Qt::CaseInsensitive)) {
                            matched = true;
                            break;
                        }
                    }
                }

                if (matched) {
                    auto* item = new QListWidgetItem(
                        n->info.scientificName.isEmpty()
                            ? n->name
                            : QStringLiteral("%1（%2）")
                                  .arg(n->name, n->info.scientificName));
                    item->setData(NodeIdRole, n->id);
                    item->setToolTip(m_document->displayPathOf(n->id));
                    if (!n->info.photos.isEmpty()) {
                        const QPixmap pixmap(
                            QDir(m_dataDir)
                                .filePath(QStringLiteral("photos")
                                          + QLatin1Char('/')
                                          + n->info.photos.first()));
                        if (!pixmap.isNull())
                            item->setIcon(QIcon(pixmap.scaled(
                                QSize(36, 36), Qt::KeepAspectRatio,
                                Qt::SmoothTransformation)));
                    }
                    m_searchResults->addItem(item);
                }
            }
            visit(childId);
        }
    };
    for (int rootId : m_document->roots())
        visit(rootId);

    m_searchCaption->setText(
        QStringLiteral("找到 %1 个植物，点击可跳转：")
            .arg(m_searchResults->count()));
    if (m_searchResults->count() == 0) {
        auto* emptyItem = new QListWidgetItem(QStringLiteral("（没有找到匹配的植物）"));
        emptyItem->setFlags(emptyItem->flags() & ~Qt::ItemIsEnabled);
        m_searchResults->addItem(emptyItem);
    }
}

void MainWindow::goToPlantFromSearch(QListWidgetItem* item)
{
    if (!item)
        return;
    const int id = item->data(NodeIdRole).toInt();
    if (id <= 0)
        return;

    m_searchEdit->clear();
    rebuildTree(id);
    m_view->showNode(id);
    refreshActionState();
}

void MainWindow::openSpeciesEditor()
{
    const int id = selectedNodeId();
    const TaxonNode* n = id > 0 ? m_document->node(id) : nullptr;
    if (!n || !TaxonRanks::canHostPlantInfo(n->rank)) {
        QMessageBox::information(
            this, QStringLiteral("编辑资料"),
            QStringLiteral("请先在左侧选择一个“种 / 亚种”植物。"));
        return;
    }

    SpeciesEditDialog dialog(m_document, m_dataDir, id, this);
    dialog.exec();
    // 即使取消，编辑器里“添加照片 / 移除照片”等操作也可能已经写入了文档，
    // 所以统一刷新并落盘。
    m_view->showNode(id);
    saveData();
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
    m_editAction->setEnabled(n && TaxonRanks::canHostPlantInfo(n->rank));
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
    m_view->showNode(newId);
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
    m_view->showNode(id);
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
    m_view->showNode(0);
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
