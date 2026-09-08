#include "mainwindow.h"

#include "comparedialog.h"
#include "csvexport.h"
#include "dbversion.h"
#include "photoimageutils.h"
#include "pinyin.h"
#include "specieseditdialog.h"
#include "speciesform.h"
#include "statsdialog.h"
#include "wheelignorefilter.h"

#include <QAbstractItemView>
#include <QAction>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QPixmap>
#include <QSettings>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSplitter>
#include <QStatusBar>
#include <QTableWidget>
#include <QTextStream>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace {

constexpr int NodeIdRole = Qt::UserRole;

bool copyPhotoFiles(const QDir& source, const QDir& destination,
                    QString* error)
{
    if (!destination.exists() && !QDir().mkpath(destination.absolutePath())) {
        if (error) {
            *error = QStringLiteral("无法创建照片目录：%1")
                         .arg(destination.absolutePath());
        }
        return false;
    }

    const QFileInfoList files = source.entryInfoList(
        QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo& fileInfo : files) {
        if (!QFile::copy(fileInfo.absoluteFilePath(),
                         destination.filePath(fileInfo.fileName()))) {
            if (error) {
                *error = QStringLiteral("复制照片失败：%1")
                             .arg(fileInfo.fileName());
            }
            return false;
        }
    }
    return true;
}

} // namespace

MainWindow::MainWindow(const QString& dataDir, QWidget* parent)
    : QMainWindow(parent)
    , m_dataDir(dataDir)
    , m_favorites(dataDir)
{
    m_dataFile = QDir(dataDir).filePath(QStringLiteral("plantmap.json"));
    m_document = new TaxonomyDocument;
    m_favorites.setDocument(m_document);
    m_undoManager.setDocument(m_document);
    m_undoManager.setDataDir(m_dataDir);

    QDir().mkpath(m_dataDir);
    if (QFile::exists(m_dataFile)) {
        QString error;
        if (!m_document->loadFromFile(m_dataFile, &error)) {
            m_loadFailed = true;
            // 立即把无法解析的原文件备份，避免被后续保存覆盖而丢失线索。
            const QString backupPath = QDir(m_dataDir).filePath(
                QStringLiteral("plantmap.corrupt-%1.json")
                    .arg(QDateTime::currentDateTime().toString(
                        QStringLiteral("yyyyMMdd_hhmmss_zzz"))));
            QFile::copy(m_dataFile, backupPath);
            QMessageBox::warning(this, QStringLiteral("数据加载失败"),
                                 error
                                     + QStringLiteral(
                                           "\n\n将按空库启动。"
                                           "无法解析的原文件已备份到：\n%1")
                                     .arg(backupPath));
        }
    }

    if (!m_loadFailed && m_document->isEmpty()) {
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
    m_favorites.load();
    rebuildTree();
    selectFirstPlantItem();
    refreshActionState();
    refreshUndoActions();
    restoreUiState();
    setWindowTitle(QStringLiteral("植物图谱 PlantMap v%1")
                       .arg(QStringLiteral(PLANTMAP_VERSION)));
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
    m_addAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+N")));
    m_editAction = toolbar->addAction(QStringLiteral("编辑资料…"));
    m_editAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+E")));
    m_renameAction = toolbar->addAction(QStringLiteral("重命名"));
    m_renameAction->setShortcut(QKeySequence(Qt::Key_F2));
    m_deleteAction = toolbar->addAction(QStringLiteral("删除"));
    m_deleteAction->setShortcut(QKeySequence(Qt::Key_Delete));
    m_cloneAction = toolbar->addAction(QStringLiteral("复制节点"));
    toolbar->addSeparator();
    m_batchSetAction = toolbar->addAction(QStringLiteral("批量设置属性…"));
    m_batchDeleteAction = toolbar->addAction(QStringLiteral("批量删除"));
    toolbar->addSeparator();
    m_undoAction = toolbar->addAction(QStringLiteral("撤销"));
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_redoAction = toolbar->addAction(QStringLiteral("重做"));
    m_redoAction->setShortcut(QKeySequence::Redo);
    toolbar->addSeparator();
    m_saveAction = toolbar->addAction(QStringLiteral("保存数据"));
    toolbar->addSeparator();
    auto* advancedAction = toolbar->addAction(QStringLiteral("高级检索…"));
    auto* statsAction = toolbar->addAction(QStringLiteral("统计"));
    auto* compareAction = toolbar->addAction(QStringLiteral("对比"));
    auto* favoritesAction = toolbar->addAction(QStringLiteral("收藏夹…"));
    auto* importAction = toolbar->addAction(QStringLiteral("导入数据库…"));

    connect(m_addAction, &QAction::triggered, this, &MainWindow::addUnderSelected);
    connect(m_editAction, &QAction::triggered, this, &MainWindow::openSpeciesEditor);
    connect(m_renameAction, &QAction::triggered, this, &MainWindow::renameSelected);
    connect(m_deleteAction, &QAction::triggered, this, &MainWindow::removeSelected);
    connect(m_cloneAction, &QAction::triggered, this, &MainWindow::cloneSelected);
    connect(m_batchSetAction, &QAction::triggered, this, &MainWindow::batchSetAttribute);
    connect(m_batchDeleteAction, &QAction::triggered, this, &MainWindow::batchDelete);
    connect(m_undoAction, &QAction::triggered, this, &MainWindow::undo);
    connect(m_redoAction, &QAction::triggered, this, &MainWindow::redo);
    connect(m_saveAction, &QAction::triggered, this, &MainWindow::saveData);
    connect(advancedAction, &QAction::triggered, this, &MainWindow::openAdvancedSearch);
    connect(statsAction, &QAction::triggered, this, &MainWindow::openStats);
    connect(compareAction, &QAction::triggered, this, &MainWindow::openCompare);
    connect(favoritesAction, &QAction::triggered, this, &MainWindow::openFavorites);
    connect(importAction, &QAction::triggered, this, &MainWindow::importDatabase);

    auto* menu = menuBar()->addMenu(QStringLiteral("数据"));
    menu->addAction(QStringLiteral("保存数据"), this, &MainWindow::saveData);
    menu->addAction(QStringLiteral("导入数据库…"), this, &MainWindow::importDatabase);
    menu->addAction(QStringLiteral("导出副本…"), this, &MainWindow::exportCopy);
    menu->addAction(QStringLiteral("导出 CSV…"), this, &MainWindow::exportCsv);
    menu->addAction(QStringLiteral("打开数据目录"), this, &MainWindow::openDataDir);
    menu->addSeparator();
    menu->addAction(QStringLiteral("退出"), this, &MainWindow::close);

    auto* helpMenu = menuBar()->addMenu(QStringLiteral("帮助"));
    helpMenu->addAction(QStringLiteral("使用说明"), this, [this] {
        QMessageBox::information(
            this, QStringLiteral("使用说明"),
            QStringLiteral(
                "① 在左侧输入框按“中文名 / 拉丁学名 / 别名”快速搜索，"
                "结果列表点击即可跳转。\n"
                "② 需要按属性精确检索时，点击工具栏“高级检索…”，"
                "可选择生长习性、生命周期、光照、水分、叶型、"
                "生长速度、花期月份等条件（条件之间为“并且”关系），"
                "检索后双击结果即可打开植物。\n"
                "③ 右侧上方会立即显示这棵植物的图片，"
                "下方列出全部属性（只读）。\n"
                "④ 要修改属性时，点击“编辑资料…”，"
                "修改后点“保存并关闭”。\n\n"
                "管理分类：选中节点后用工具栏的"
                "“添加下级分类 / 重命名 / 删除”。\n"
                "添加图片：在编辑资料窗口中点"
                "“添加照片…”，图片会自动复制到数据目录。"));
    });

    auto* central = new QWidget(this);
    auto* layout = new QHBoxLayout(central);
    layout->setContentsMargins(4, 4, 4, 4);

    m_splitter = new QSplitter(Qt::Horizontal, central);
    m_splitter->setChildrenCollapsible(true);
    m_splitter->setHandleWidth(6);
    layout->addWidget(m_splitter);
    setCentralWidget(central);

    auto* treePanel = new QWidget(m_splitter);
    treePanel->setMinimumWidth(0);
    auto* treeLayout = new QVBoxLayout(treePanel);
    treeLayout->setContentsMargins(0, 0, 0, 0);

    m_searchEdit = new QLineEdit(treePanel);
    m_searchEdit->setPlaceholderText(
        QStringLiteral("搜索：中文名 / 拉丁学名 / 别名 / 拼音首字母"));
    m_searchEdit->setClearButtonEnabled(true);
    treeLayout->addWidget(m_searchEdit);

    // 展开/折叠按钮放在树界面顶部，方便随时操作。
    auto* treeToolbar = new QHBoxLayout;
    treeToolbar->setSpacing(4);
    auto* expandButton = new QPushButton(QStringLiteral("展开全部"), treePanel);
    auto* collapseButton = new QPushButton(QStringLiteral("折叠全部"), treePanel);
    for (auto* b : { expandButton, collapseButton }) {
        b->setFlat(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setToolTip(b == expandButton
                          ? QStringLiteral("展开分类树的全部层级")
                          : QStringLiteral("折叠分类树，只显示顶级“界”"));
    }
    treeToolbar->addWidget(expandButton);
    treeToolbar->addWidget(collapseButton);
    treeToolbar->addStretch();
    treeLayout->addLayout(treeToolbar);
    connect(expandButton, &QPushButton::clicked, this, &MainWindow::expandAllTree);
    connect(collapseButton, &QPushButton::clicked, this, &MainWindow::collapseAllTree);

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
    m_tree->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    m_tree->setColumnCount(3);
    m_tree->setHeaderLabels({ QStringLiteral("名称"),
                              QStringLiteral("等级"),
                              QStringLiteral("拉丁学名") });
    // 三列都允许用户直接拖动表头调整宽度。
    m_tree->header()->setSectionResizeMode(QHeaderView::Interactive);
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setMinimumSectionSize(30);
    m_tree->setColumnWidth(0, 180);
    m_tree->setColumnWidth(1, 45);
    m_tree->setColumnWidth(2, 170);
    m_tree->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_tree->setUniformRowHeights(true);
    // 支持多选，配合批量操作（Ctrl/Shift 多选，或按住框选）。
    m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    treeLayout->addWidget(m_tree);
    m_splitter->addWidget(treePanel);

    m_view = new SpeciesViewForm(m_splitter);
    m_view->setDocument(m_document);
    m_view->setDataDir(m_dataDir);
    m_splitter->addWidget(m_view);
    m_splitter->setStretchFactor(0, 3);
    m_splitter->setStretchFactor(1, 5);
    m_splitter->setSizes({ 420, 900 });

    connect(m_tree, &QTreeWidget::currentItemChanged,
            this, &MainWindow::onTreeSelectionChanged);
    // 多选变化（Ctrl/Shift/框选）只触发 selectionChanged，用它刷新批量按钮状态。
    connect(m_tree, &QTreeWidget::itemSelectionChanged,
            this, &MainWindow::refreshActionState);
    connect(m_view, &SpeciesViewForm::editRequested,
            this, &MainWindow::openSpeciesEditor);
    connect(m_view, &SpeciesViewForm::favoriteToggled,
            this, &MainWindow::toggleFavorite);
    connect(m_view, &SpeciesViewForm::compareToggled,
            this, &MainWindow::toggleCompare);
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
    const int id = selectedNodeId();
    m_view->showNode(id);
    m_view->setFavorite(m_favorites.contains(id));
    m_view->setCompared(m_compareList.contains(id));
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

    if (!searching) {
        refreshActionState();
        return;
    }

    // 搜索激活时禁用针对“隐藏树节点”的增删改操作，避免误操作。
    m_addAction->setEnabled(false);
    m_editAction->setEnabled(false);
    m_renameAction->setEnabled(false);
    m_deleteAction->setEnabled(false);
    m_cloneAction->setEnabled(false);
    if (m_batchSetAction)
        m_batchSetAction->setEnabled(false);
    if (m_batchDeleteAction)
        m_batchDeleteAction->setEnabled(false);
    m_view->setEditEnabled(false);

    // 防抖：输入停顿 200ms 后再真正执行搜索，避免每敲一个字符都全量遍历。
    m_pendingSearchText = query;
    if (!m_searchDebounceTimer) {
        m_searchDebounceTimer = new QTimer(this);
        m_searchDebounceTimer->setSingleShot(true);
        m_searchDebounceTimer->setInterval(200);
        connect(m_searchDebounceTimer, &QTimer::timeout,
                this, &MainWindow::runSearchDebounced);
    }
    m_searchDebounceTimer->start();
}

void MainWindow::runSearchDebounced()
{
    const QString query = m_pendingSearchText;

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
                    && n->hasInfo
                    && !n->info.scientificName.isEmpty()
                    && TaxonomyDocument::normalizedScientificName(
                           n->info.scientificName)
                           .contains(normalizedQuery)) {
                    matched = true;
                }
                if (!matched && n->hasInfo) {
                    for (const QString& alias : n->info.aliases) {
                        if (!alias.trimmed().isEmpty()
                            && alias.contains(query, Qt::CaseInsensitive)) {
                            matched = true;
                            break;
                        }
                    }
                }
                // 拼音首字母匹配：输入“ys”可匹配“银杏”(YX)，输入“md”匹配“牡丹”。
                if (!matched && Pinyin::matchesInitials(n->name, query)) {
                    matched = true;
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
                        const QString relativeName = n->info.photos.first();
                        // 缓存键带文件修改时间，外部替换同名文件后图标能自动更新；
                        // 缓存过大时整体清空重建，避免长时间使用只增不减。
                        if (m_searchThumbCache.size() > 256)
                            m_searchThumbCache.clear();
                        const QString photoPath =
                            QDir(m_dataDir)
                                .filePath(QStringLiteral("photos")
                                          + QLatin1Char('/')
                                          + relativeName);
                        const QString cacheKey = QStringLiteral("%1@%2")
                            .arg(relativeName)
                            .arg(QFileInfo(photoPath).lastModified()
                                     .toMSecsSinceEpoch());
                        auto cacheIt = m_searchThumbCache.find(cacheKey);
                        if (cacheIt == m_searchThumbCache.end()) {
                            cacheIt = m_searchThumbCache.insert(
                                cacheKey, QIcon(PhotoImageUtils::loadSearchIcon(
                                                photoPath)));
                        }
                        if (!cacheIt->isNull())
                            item->setIcon(*cacheIt);
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
    m_view->setFavorite(m_favorites.contains(id));
    m_view->setCompared(m_compareList.contains(id));
    refreshActionState();
}

bool MainWindow::selectFirstPlantItem()
{
    if (m_document->roots().isEmpty())
        return false;

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

    QTreeWidgetItem* target = firstPlantItem
        ? firstPlantItem
        : (m_tree->topLevelItemCount() > 0
               ? m_tree->topLevelItem(0)
               : nullptr);
    if (!target)
        return false;

    m_tree->setCurrentItem(target, 0);
    QTreeWidgetItem* parent = target->parent();
    while (parent) {
        parent->setExpanded(true);
        parent = parent->parent();
    }
    target->setExpanded(true);
    return true;
}

void MainWindow::importDatabase()
{
    const QString selectedFile = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择要导入的数据库文件"),
        QString(), QStringLiteral("PlantMap 数据库 (plantmap.json);;"
                                  "JSON 文件 (*.json)"));
    if (selectedFile.isEmpty())
        return;

    QFile source(selectedFile);
    if (!source.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, QStringLiteral("导入失败"),
                             QStringLiteral("无法读取所选文件：%1")
                                 .arg(source.errorString()));
        return;
    }
    const QJsonDocument importedDoc =
        QJsonDocument::fromJson(source.readAll());
    source.close();
    if (!importedDoc.isObject()) {
        QMessageBox::warning(this, QStringLiteral("导入失败"),
                             QStringLiteral("所选文件不是有效的 JSON 数据库。"));
        return;
    }

    QJsonObject imported = importedDoc.object();
    const int importedVersion = DbVersion::schemaVersionOf(imported);
    if (importedVersion <= 0) {
        QMessageBox::warning(this, QStringLiteral("导入失败"),
                             QStringLiteral("无法识别所选数据库的格式版本。"));
        return;
    }
    if (importedVersion > DbVersion::kCurrentSchemaVersion) {
        QMessageBox::warning(
            this, QStringLiteral("禁止导入"),
            QStringLiteral("所选数据库版本为 %1，高于当前软件支持的版本 %2，"
                           "无法导入。")
                .arg(importedVersion)
                .arg(DbVersion::kCurrentSchemaVersion));
        return;
    }

    QString error;
    if (importedVersion < DbVersion::kCurrentSchemaVersion
        && !DbVersion::migrateToLatest(imported, &error)) {
        QMessageBox::warning(this, QStringLiteral("无法升级数据库"), error);
        return;
    }

    // 先用临时文档完整校验一次，确认无误后才触碰当前数据。
    TaxonomyDocument probe;
    if (!probe.loadFromJson(imported, &error)) {
        QMessageBox::warning(this, QStringLiteral("导入失败"),
                             QStringLiteral("数据库内容校验失败：\n%1")
                                 .arg(error));
        return;
    }

    const QMessageBox::StandardButton answer = QMessageBox::question(
        this, QStringLiteral("确认导入"),
        QStringLiteral("导入将用所选数据库替换当前数据库。\n"
                       "导入前会先备份当前数据库，失败时会自动还原。\n\n"
                       "是否继续？"));
    if (answer != QMessageBox::Yes)
        return;

    // 保证磁盘上的当前数据与内存一致，备份才有意义。
    // 保存失败必须中止导入：否则备份的是磁盘上的旧文件，
    // 导入成功后内存里未保存的修改会被静默覆盖，失败还原也只能还原旧文件。
    if (!m_document->isEmpty() && !saveData())
        return;

    const QString backupDir = QDir(m_dataDir).filePath(
        QStringLiteral("backups/import_")
        + QDateTime::currentDateTime().toString(
            QStringLiteral("yyyyMMdd_hhmmss_zzz")));
    if (!QDir().mkpath(backupDir)) {
        QMessageBox::warning(this, QStringLiteral("导入失败"),
                             QStringLiteral("无法创建备份目录：%1")
                                 .arg(backupDir));
        return;
    }
    const QString backupDb = QDir(backupDir).filePath(
        QStringLiteral("plantmap.json"));
    const QString backupPhotosDir = QDir(backupDir).filePath(
        QStringLiteral("photos"));

    const bool hadOldDb = QFile::exists(m_dataFile);
    if (hadOldDb && !QFile::copy(m_dataFile, backupDb)) {
        QMessageBox::warning(this, QStringLiteral("导入失败"),
                             QStringLiteral("备份当前数据库文件失败。"));
        return;
    }

    const QString currentPhotosPath =
        QDir(m_dataDir).filePath(QStringLiteral("photos"));
    QDir oldPhotosDir(currentPhotosPath);
    if (oldPhotosDir.exists()) {
        if (!QDir().mkpath(backupPhotosDir)) {
            QMessageBox::warning(this, QStringLiteral("导入失败"),
                                 QStringLiteral("无法创建照片备份目录。"));
            return;
        }
        if (!copyPhotoFiles(oldPhotosDir, QDir(backupPhotosDir), &error)) {
            QMessageBox::warning(this, QStringLiteral("导入失败"),
                                 QStringLiteral("备份当前照片失败：%1")
                                     .arg(error));
            return;
        }
    }

    const QDir sourcePhotosDir(
        QFileInfo(selectedFile).absoluteDir().filePath(
            QStringLiteral("photos")));
    const bool samePhotosDir =
        QDir::cleanPath(sourcePhotosDir.absolutePath())
        == QDir::cleanPath(currentPhotosPath);

    bool ok = true;
    if (!samePhotosDir) {
        if (oldPhotosDir.exists() && !oldPhotosDir.removeRecursively())
            ok = false;
        if (ok && !QDir().mkpath(currentPhotosPath))
            ok = false;
        if (ok && sourcePhotosDir.exists()
            && !copyPhotoFiles(sourcePhotosDir,
                               QDir(currentPhotosPath), &error))
            ok = false;
    }

    if (ok && !m_document->loadFromJson(imported, &error))
        ok = false;
    if (ok && !m_document->saveToFile(m_dataFile, &error))
        ok = false;

    if (!ok) {
        QString restoreError;
        // 还原数据库与照片。
        // samePhotosDir 时照片目录从未被改动，跳过一次无谓的“删除再拷回”，
        // 避免还原过程本身反而成为照片丢失的风险点。
        if (!samePhotosDir) {
            if (oldPhotosDir.exists() && !oldPhotosDir.removeRecursively())
                restoreError = QStringLiteral("清空当前照片目录失败。");
            if (restoreError.isEmpty())
                QDir().mkpath(currentPhotosPath);
            if (restoreError.isEmpty()
                && QDir(backupPhotosDir).exists()
                && !copyPhotoFiles(QDir(backupPhotosDir),
                                   QDir(currentPhotosPath), &restoreError)) {
                restoreError = QStringLiteral("还原照片失败：%1")
                                   .arg(restoreError);
            }
        }
        if (hadOldDb) {
            if (QFile::exists(m_dataFile) && !QFile::remove(m_dataFile)) {
                restoreError = QStringLiteral("无法删除导入产生的数据文件：%1")
                                   .arg(m_dataFile);
            }
            if (restoreError.isEmpty()
                && !QFile::copy(backupDb, m_dataFile)) {
                restoreError = QStringLiteral("还原数据库文件失败。");
            }
        }
        if (restoreError.isEmpty()) {
            if (hadOldDb) {
                if (!m_document->loadFromFile(m_dataFile, &restoreError))
                    restoreError = QStringLiteral("重新载入还原后的数据库失败：%1")
                                       .arg(restoreError);
            } else {
                m_document->clear();
            }
        }
        // 还原出问题时一律按“加载失败”对待：之后任何保存都会先给磁盘上的
        // 文件留副本，避免把用户以为已回滚的数据静默写回去。
        m_loadFailed = !restoreError.isEmpty() && hadOldDb;

        m_searchEdit->clear();
        m_searchThumbCache.clear();
        m_undoManager.clear();
        // 文档已替换/还原：旧的收藏与对比 id 不再适用，清空避免指向错误节点。
        m_favorites.clearAll();
        m_compareList.clear();
        rebuildTree();
        selectFirstPlantItem();
        refreshActionState();

        if (!restoreError.isEmpty()) {
            QMessageBox::critical(
                this, QStringLiteral("导入失败且无法完全还原"),
                QStringLiteral("导入失败：%1\n\n还原过程也出现问题：%2\n"
                               "备份位于：%3")
                    .arg(error, restoreError, backupDir));
        } else {
            QMessageBox::warning(
                this, QStringLiteral("导入失败"),
                QStringLiteral("导入失败，已恢复导入前的数据库。\n"
                               "原因：%1\n\n备份目录：%2")
                    .arg(error, backupDir));
        }
        return;
    }

    m_searchEdit->clear();
    m_searchThumbCache.clear();
    m_loadFailed = false;
    m_undoManager.clear();
    // 导入的是新库：旧的收藏与对比 id 不适用，清空避免指向错误节点。
    m_favorites.clearAll();
    m_compareList.clear();
    rebuildTree();
    selectFirstPlantItem();
    refreshActionState();
    setStatus(QStringLiteral("数据库导入成功，原数据库已备份到 %1")
                  .arg(backupDir));
}

void MainWindow::openAdvancedSearch()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("高级检索"));
    dialog.resize(760, 720);

    auto* layout = new QVBoxLayout(&dialog);
    auto* filterBox = new QGroupBox(
        QStringLiteral("按植物属性精确检索（条件之间为“并且”关系）"), &dialog);
    auto* filterForm = new QFormLayout(filterBox);

    // 显式传入枚举值列表，避免依赖“枚举值从 0 连续递增”这一隐含假设；
    // 未来若插入非连续枚举值，只需改这里传的列表即可。
    auto fillEnum = [](QComboBox* combo, const QList<int>& values,
                       auto label, auto key) {
        combo->addItem(QStringLiteral("不限"), QString());
        for (int v : values) {
            combo->addItem(label(v), key(v));
        }
    };

    auto* habitCombo = new QComboBox(filterBox);
    fillEnum(habitCombo,
             { static_cast<int>(GrowthHabit::Tree),
               static_cast<int>(GrowthHabit::Shrub),
               static_cast<int>(GrowthHabit::Herb),
               static_cast<int>(GrowthHabit::Vine),
               static_cast<int>(GrowthHabit::Aquatic),
               static_cast<int>(GrowthHabit::Succulent),
               static_cast<int>(GrowthHabit::Fern),
               static_cast<int>(GrowthHabit::Other) },
             [](int i) { return habitLabel(static_cast<GrowthHabit>(i)); },
             [](int i) { return habitToKey(static_cast<GrowthHabit>(i)); });
    filterForm->addRow(QStringLiteral("生长习性："), habitCombo);

    auto* lifeCombo = new QComboBox(filterBox);
    fillEnum(lifeCombo,
             { static_cast<int>(LifeCycle::Annual),
               static_cast<int>(LifeCycle::Biennial),
               static_cast<int>(LifeCycle::Perennial) },
             [](int i) { return lifecycleLabel(static_cast<LifeCycle>(i)); },
             [](int i) { return lifecycleToKey(static_cast<LifeCycle>(i)); });
    filterForm->addRow(QStringLiteral("生命周期："), lifeCombo);

    auto* lightCombo = new QComboBox(filterBox);
    fillEnum(lightCombo,
             { static_cast<int>(LightPreference::FullSun),
               static_cast<int>(LightPreference::HalfSun),
               static_cast<int>(LightPreference::HalfShade),
               static_cast<int>(LightPreference::Shade) },
             [](int i) { return lightLabel(static_cast<LightPreference>(i)); },
             [](int i) { return lightToKey(static_cast<LightPreference>(i)); });
    filterForm->addRow(QStringLiteral("光照："), lightCombo);

    auto* waterCombo = new QComboBox(filterBox);
    fillEnum(waterCombo,
             { static_cast<int>(WaterPreference::Dry),
               static_cast<int>(WaterPreference::Moderate),
               static_cast<int>(WaterPreference::Moist),
               static_cast<int>(WaterPreference::Aquatic) },
             [](int i) { return waterLabel(static_cast<WaterPreference>(i)); },
             [](int i) { return waterToKey(static_cast<WaterPreference>(i)); });
    filterForm->addRow(QStringLiteral("水分："), waterCombo);

    auto* foliageCombo = new QComboBox(filterBox);
    fillEnum(foliageCombo,
             { static_cast<int>(FoliageType::Evergreen),
               static_cast<int>(FoliageType::SemiEvergreen),
               static_cast<int>(FoliageType::Deciduous) },
             [](int i) { return foliageLabel(static_cast<FoliageType>(i)); },
             [](int i) { return foliageToKey(static_cast<FoliageType>(i)); });
    filterForm->addRow(QStringLiteral("叶型："), foliageCombo);

    auto* rateCombo = new QComboBox(filterBox);
    fillEnum(rateCombo,
             { static_cast<int>(GrowthRate::Slow),
               static_cast<int>(GrowthRate::Medium),
               static_cast<int>(GrowthRate::Fast) },
             [](int i) { return growthRateLabel(static_cast<GrowthRate>(i)); },
             [](int i) { return growthRateToKey(static_cast<GrowthRate>(i)); });
    filterForm->addRow(QStringLiteral("生长速度："), rateCombo);

    auto* bloomMonthCombo = new QComboBox(filterBox);
    bloomMonthCombo->addItem(QStringLiteral("不限"), 0);
    for (int month = 1; month <= 12; ++month)
        bloomMonthCombo->addItem(QStringLiteral("%1月").arg(month), month);
    filterForm->addRow(QStringLiteral("花期月份："), bloomMonthCombo);

    auto* wheelGuard = new WheelIgnoreFilter(filterBox);
    for (QComboBox* combo : filterBox->findChildren<QComboBox*>()) {
        combo->installEventFilter(wheelGuard);
        for (QObject* child : combo->children()) {
            if (auto* childWidget = qobject_cast<QWidget*>(child))
                childWidget->installEventFilter(wheelGuard);
        }
        if (QAbstractItemView* view = combo->view()) {
            view->installEventFilter(wheelGuard);
            if (view->viewport())
                view->viewport()->installEventFilter(wheelGuard);
        }
    }
    layout->addWidget(filterBox);

    // ---------------- 筛选预设 ----------------
    const QString presetsPath =
        QDir(m_dataDir).filePath(QStringLiteral("filter_presets.json"));
    QJsonObject presets;
    {
        QFile presetFile(presetsPath);
        if (presetFile.open(QIODevice::ReadOnly))
            presets = QJsonDocument::fromJson(presetFile.readAll()).object();
    }
    auto* presetRow = new QHBoxLayout;
    auto* presetCombo = new QComboBox(&dialog);
    presetCombo->setMinimumWidth(200);
    presetCombo->addItem(QStringLiteral("（选择预设以加载）"), QString());
    const QStringList presetNames = presets.keys();
    for (const QString& name : presetNames)
        presetCombo->addItem(name, name);
    auto* savePresetButton = new QPushButton(QStringLiteral("保存当前筛选为预设…"), &dialog);
    presetRow->addWidget(new QLabel(QStringLiteral("预设："), &dialog));
    presetRow->addWidget(presetCombo, 1);
    presetRow->addWidget(savePresetButton);
    layout->addLayout(presetRow);

    auto* countLabel = new QLabel(&dialog);
    layout->addWidget(countLabel);
    auto* results = new QListWidget(&dialog);
    results->setMinimumHeight(240);
    layout->addWidget(results, 1);

    auto runSearch = [&]() {
        results->clear();
        const QString habitKey = habitCombo->currentData().toString();
        const QString lifeKey = lifeCombo->currentData().toString();
        const QString lightKey = lightCombo->currentData().toString();
        const QString waterKey = waterCombo->currentData().toString();
        const QString foliageKey = foliageCombo->currentData().toString();
        const QString rateKey = rateCombo->currentData().toString();
        const int bloomMonth = bloomMonthCombo->currentData().toInt();

        std::function<void(int)> visit = [&](int parentId) {
            for (int childId : m_document->childIdsOf(parentId)) {
                const TaxonNode* n = m_document->node(childId);
                if (n && TaxonRanks::canHostPlantInfo(n->rank)) {
                    bool ok = true;
                    if (ok && !habitKey.isEmpty())
                        ok = habitToKey(n->info.habit) == habitKey;
                    if (ok && !lifeKey.isEmpty())
                        ok = lifecycleToKey(n->info.lifeCycle) == lifeKey;
                    if (ok && !lightKey.isEmpty())
                        ok = lightToKey(n->info.light) == lightKey;
                    if (ok && !waterKey.isEmpty())
                        ok = waterToKey(n->info.water) == waterKey;
                    if (ok && !foliageKey.isEmpty())
                        ok = foliageToKey(n->info.foliage) == foliageKey;
                    if (ok && !rateKey.isEmpty())
                        ok = growthRateToKey(n->info.growthRate) == rateKey;
                    if (ok && bloomMonth > 0)
                        ok = n->info.bloomMonths.contains(bloomMonth);

                    if (ok) {
                        auto* item = new QListWidgetItem(
                            n->info.scientificName.isEmpty()
                                ? n->name
                                : QStringLiteral("%1（%2）")
                                      .arg(n->name, n->info.scientificName));
                        item->setData(NodeIdRole, n->id);
                        item->setToolTip(m_document->displayPathOf(n->id));
                        results->addItem(item);
                    }
                }
                visit(childId);
            }
        };
        for (int rootId : m_document->roots())
            visit(rootId);

        countLabel->setText(
            results->count() == 0
                ? QStringLiteral("没有符合条件的植物。")
                : QStringLiteral("找到 %1 个符合条件的植物，双击可打开：")
                      .arg(results->count()));
        if (results->count() == 0) {
            auto* emptyItem = new QListWidgetItem(QStringLiteral("（无结果）"));
            emptyItem->setFlags(emptyItem->flags() & ~Qt::ItemIsEnabled);
            results->addItem(emptyItem);
        }
    };

    auto openSelected = [&]() {
        QListWidgetItem* item = results->currentItem();
        if (!item)
            item = results->count() > 0 ? results->item(0) : nullptr;
        if (!item)
            return;
        const int id = item->data(NodeIdRole).toInt();
        if (id <= 0)
            return;
        m_searchEdit->clear();
        rebuildTree(id);
        m_view->showNode(id);
        refreshActionState();
        dialog.accept();
    };

    // 加载预设：按预设内容回填各筛选控件并立即检索。
    auto applyPreset = [&](const QString& name) {
        const QJsonObject p = presets.value(name).toObject();
        auto setCombo = [](QComboBox* combo, const QString& key) {
            const int idx = combo->findData(key);
            combo->setCurrentIndex(idx >= 0 ? idx : 0);
        };
        setCombo(habitCombo, p.value(QStringLiteral("habit")).toString());
        setCombo(lifeCombo, p.value(QStringLiteral("life")).toString());
        setCombo(lightCombo, p.value(QStringLiteral("light")).toString());
        setCombo(waterCombo, p.value(QStringLiteral("water")).toString());
        setCombo(foliageCombo, p.value(QStringLiteral("foliage")).toString());
        setCombo(rateCombo, p.value(QStringLiteral("rate")).toString());
        const int bloomIdx = bloomMonthCombo->findData(
            p.value(QStringLiteral("bloom")).toInt());
        bloomMonthCombo->setCurrentIndex(bloomIdx >= 0 ? bloomIdx : 0);
        runSearch();
    };
    connect(presetCombo, &QComboBox::currentIndexChanged, &dialog, [&](int) {
        const QString name = presetCombo->currentData().toString();
        if (!name.isEmpty())
            applyPreset(name);
    });

    // 保存当前筛选为预设。
    connect(savePresetButton, &QPushButton::clicked, &dialog, [&] {
        bool ok = false;
        const QString name = QInputDialog::getText(
            &dialog, QStringLiteral("保存筛选预设"),
            QStringLiteral("预设名称："), QLineEdit::Normal, QString(), &ok).trimmed();
        if (!ok || name.isEmpty())
            return;
        QJsonObject p;
        p[QStringLiteral("habit")] = habitCombo->currentData().toString();
        p[QStringLiteral("life")] = lifeCombo->currentData().toString();
        p[QStringLiteral("light")] = lightCombo->currentData().toString();
        p[QStringLiteral("water")] = waterCombo->currentData().toString();
        p[QStringLiteral("foliage")] = foliageCombo->currentData().toString();
        p[QStringLiteral("rate")] = rateCombo->currentData().toString();
        p[QStringLiteral("bloom")] = bloomMonthCombo->currentData().toInt();
        presets[name] = p;
        QFile presetFile(presetsPath);
        if (presetFile.open(QIODevice::WriteOnly))
            presetFile.write(QJsonDocument(presets).toJson(QJsonDocument::Indented));
        int idx = presetCombo->findData(name);
        if (idx < 0)
            presetCombo->addItem(name, name);
        presetCombo->setCurrentIndex(presetCombo->findData(name));
    });

    // 把检索结果导出为 CSV（带 UTF-8 BOM，便于 Excel 直接打开中文）。
    auto exportCsv = [&]() {
        QVector<int> ids;
        for (int i = 0; i < results->count(); ++i) {
            const int id = results->item(i)->data(NodeIdRole).toInt();
            if (id > 0)
                ids.append(id);
        }
        if (ids.isEmpty()) {
            QMessageBox::information(&dialog, QStringLiteral("导出"),
                                     QStringLiteral("当前没有可导出的结果。"));
            return;
        }
        const QString file = QFileDialog::getSaveFileName(
            &dialog, QStringLiteral("导出检索结果"),
            QStringLiteral("植物检索结果.csv"), QStringLiteral("CSV 文件 (*.csv)"));
        if (file.isEmpty())
            return;
        QFile csvFile(file);
        if (!csvFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::warning(&dialog, QStringLiteral("导出失败"),
                                 QStringLiteral("无法写入文件：%1").arg(file));
            return;
        }
        csvFile.write("\xEF\xBB\xBF");  // UTF-8 BOM
        QTextStream ts(&csvFile);
        ts << QStringLiteral("中文名,拉丁学名,分类路径,生长习性,生命周期,叶型,光照,水分\n");
        for (int id : ids) {
            const TaxonNode* n = m_document->node(id);
            if (!n)
                continue;
            ts << '"' << n->name << "\",\""
               << n->info.scientificName << "\",\""
               << m_document->displayPathOf(id) << "\",\""
               << habitLabel(n->info.habit) << "\",\""
               << lifecycleLabel(n->info.lifeCycle) << "\",\""
               << foliageLabel(n->info.foliage) << "\",\""
               << lightLabel(n->info.light) << "\",\""
               << waterLabel(n->info.water) << "\"\n";
        }
        csvFile.close();
        setStatus(QStringLiteral("已导出 %1 条结果到 %2").arg(ids.size()).arg(file));
    };

    auto* buttons = new QDialogButtonBox(&dialog);
    QPushButton* searchButton =
        buttons->addButton(QStringLiteral("检索"),
                           QDialogButtonBox::AcceptRole);
    QPushButton* openButton =
        buttons->addButton(QStringLiteral("打开选中植物"),
                           QDialogButtonBox::ActionRole);
    QPushButton* closeButton =
        buttons->addButton(QStringLiteral("关闭"),
                           QDialogButtonBox::RejectRole);
    QPushButton* exportButton =
        buttons->addButton(QStringLiteral("导出 CSV"),
                           QDialogButtonBox::ActionRole);
    connect(searchButton, &QPushButton::clicked, &dialog, [&] {
        runSearch();
    });
    connect(openButton, &QPushButton::clicked, &dialog, [&] {
        openSelected();
    });
    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(exportButton, &QPushButton::clicked, &dialog, [&] {
        exportCsv();
    });
    connect(results, &QListWidget::itemDoubleClicked,
            &dialog, [&](QListWidgetItem*) { openSelected(); });
    layout->addWidget(buttons);

    dialog.exec();
}

void MainWindow::openStats()
{
    StatsDialog dialog(m_document, this);
    dialog.exec();
}

void MainWindow::openCompare()
{
    // 清理已不存在的节点（例如被删除的植物）。
    QList<int> valid;
    for (int id : m_compareList) {
        const TaxonNode* n = m_document->node(id);
        if (n && TaxonRanks::canHostPlantInfo(n->rank))
            valid.append(id);
    }
    m_compareList = valid;

    if (m_compareList.isEmpty()) {
        QMessageBox::information(
            this, QStringLiteral("物种对比"),
            QStringLiteral("对比列表还是空的。\n\n"
                           "请先浏览植物，在详情页点击“＋ 加入对比”，"
                           "把要比较的植物加入列表，再点“对比”查看。"));
        return;
    }

    CompareDialog dialog(m_document, &m_compareList, this);
    dialog.exec();

    // 对比对话框关闭后，同步当前节点的按钮状态（可能在对话框里删除了植物）。
    const int id = selectedNodeId();
    m_view->setCompared(m_compareList.contains(id));
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

    const TaxonNode* before = m_document->node(id);
    const bool hadOld = before && before->hasInfo;
    const SpeciesInfo oldInfo = hadOld ? before->info : SpeciesInfo();

    SpeciesEditDialog dialog(m_document, m_dataDir, id, this);
    dialog.exec();

    // 编辑窗口内资料有变化时记录一条可撤销命令。
    const TaxonNode* after = m_document->node(id);
    const bool hadNew = after && after->hasInfo;
    const SpeciesInfo newInfo = hadNew ? after->info : SpeciesInfo();
    if (hadOld != hadNew || !(oldInfo == newInfo)) {
        UndoCommand cmd;
        cmd.type = UndoCommand::SetInfo;
        cmd.nodeId = id;
        cmd.oldInfo = oldInfo;
        cmd.hadOldInfo = hadOld;
        cmd.newInfo = newInfo;
        cmd.hadNewInfo = hadNew;
        m_undoManager.push(cmd);
    }

    // 编辑器只在“保存”时写文档，取消不会改动数据；
    // 这里统一刷新视图并把可能的修改落盘。
    m_view->showNode(id);
    saveData();
    refreshActionState();
}

int MainWindow::selectedNodeId() const
{
    QTreeWidgetItem* item = m_tree->currentItem();
    return item ? item->data(0, NodeIdRole).toInt() : 0;
}

QList<int> MainWindow::selectedNodeIds() const
{
    QList<int> ids;
    for (QTreeWidgetItem* item : m_tree->selectedItems()) {
        const int id = item->data(0, NodeIdRole).toInt();
        if (id > 0 && !ids.contains(id))
            ids.append(id);
    }
    return ids;
}

void MainWindow::refreshActionState()
{
    const int id = selectedNodeId();
    const TaxonNode* n = id > 0 ? m_document->node(id) : nullptr;
    const bool searching =
        m_searchEdit && !m_searchEdit->text().trimmed().isEmpty();
    m_addAction->setEnabled(!n || TaxonRanks::canHaveChildren(n->rank));
    m_editAction->setEnabled(
        n && TaxonRanks::canHostPlantInfo(n->rank) && !searching);
    m_renameAction->setEnabled(n != nullptr);
    m_deleteAction->setEnabled(n != nullptr);
    // 复制节点与批量操作同样只在非搜索状态可用（搜索时树隐藏，不能操作隐藏选择）。
    m_cloneAction->setEnabled(n != nullptr && !searching);
    m_view->setEditEnabled(m_editAction->isEnabled());

    // 批量操作只在“非搜索”状态可用；搜索时树被隐藏，不能对隐藏树的旧选择操作。
    const int multiSelected = m_tree->selectedItems().size();
    const bool batchable = !searching && multiSelected >= 2;
    if (m_batchSetAction)
        m_batchSetAction->setEnabled(batchable);
    if (m_batchDeleteAction)
        m_batchDeleteAction->setEnabled(batchable);

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

    // “种”之下并列四个末级等级，让用户选择要新建哪一种。
    if (parentNode && parentNode->rank == TaxonRank::Species) {
        QStringList items;
        for (TaxonRank r : TaxonRanks::subRanks())
            items.append(TaxonRanks::displayName(r));
        bool ok = false;
        const QString choice = QInputDialog::getItem(
            this, QStringLiteral("选择下级等级"),
            QStringLiteral("“%1”下可以建立哪一类？").arg(parentNode->name),
            items, 0, false, &ok);
        if (!ok || choice.isEmpty())
            return;
        for (TaxonRank r : TaxonRanks::subRanks()) {
            if (TaxonRanks::displayName(r) == choice) {
                newRank = r;
                break;
            }
        }
    }

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
    const int newId = parentNode && parentNode->rank == TaxonRank::Species
        ? m_document->addNode(parentId, name, newRank, &error)
        : m_document->addNode(parentId, name, &error);
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

    UndoCommand cmd;
    cmd.type = UndoCommand::AddNode;
    cmd.nodeId = newId;
    cmd.parentId = parentId;
    cmd.subtree = m_document->subtreeToJson(newId);
    m_undoManager.push(cmd);

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
    UndoCommand cmd;
    cmd.type = UndoCommand::RenameNode;
    cmd.nodeId = id;
    cmd.oldName = n->name;
    cmd.newName = newName;
    m_undoManager.push(cmd);

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

QSet<QString> MainWindow::collectSubtreePhotos(int id) const
{
    QSet<QString> photos;
    std::function<void(int)> visit = [&](int nodeId) {
        const TaxonNode* n = m_document->node(nodeId);
        if (!n)
            return;
        if (n->hasInfo) {
            for (const QString& fileName : n->info.photos)
                photos.insert(fileName);
        }
        for (int childId : n->childIds)
            visit(childId);
    };
    visit(id);
    return photos;
}

QSet<QString> MainWindow::collectAllReferencedPhotos() const
{
    QSet<QString> photos;
    for (int rootId : m_document->roots())
        photos.unite(collectSubtreePhotos(rootId));
    return photos;
}

void MainWindow::removeSubtreeAndPhotos(int id)
{
    // 删除前先收集该子树引用的照片，删除后再判断它们是否还被其余节点引用，
    // 只有变成“孤儿”的文件才会从 photos/ 目录删除。
    const QSet<QString> subtreePhotos = collectSubtreePhotos(id);

    // 删除前收集子树内所有节点 id，用于同步清理对比列表。
    QList<int> doomedIds;
    {
        std::function<void(int)> collectIds = [&](int nodeId) {
            doomedIds.append(nodeId);
            const TaxonNode* n = m_document->node(nodeId);
            if (n) {
                for (int childId : n->childIds)
                    collectIds(childId);
            }
        };
        collectIds(id);
    }

    // 记录子树 JSON 与照片字节，供“撤销删除”恢复。
    UndoCommand cmd;
    cmd.type = UndoCommand::RemoveNode;
    cmd.nodeId = id;
    const TaxonNode* parentNode = m_document->parent(id);
    cmd.parentId = parentNode ? parentNode->id : 0;
    cmd.subtree = m_document->subtreeToJson(id);
    const QDir photosDir(QDir(m_dataDir).filePath(QStringLiteral("photos")));
    for (const QString& fileName : subtreePhotos) {
        QFile f(photosDir.filePath(fileName));
        if (f.open(QIODevice::ReadOnly)) {
            cmd.photos.insert(fileName, f.readAll());
            f.close();
        }
    }

    QString error;
    if (!m_document->removeNode(id, &error)) {
        showError(QStringLiteral("删除失败"), error);
        return;
    }
    m_undoManager.push(cmd);

    // 清理对比列表与收藏夹中已删除节点及其子树的 id。
    for (int doomedId : doomedIds)
        m_compareList.removeAll(doomedId);
    m_favorites.removeIds(doomedIds);

    const QSet<QString> stillReferenced = collectAllReferencedPhotos();
    QStringList removed;
    for (const QString& fileName : subtreePhotos) {
        if (stillReferenced.contains(fileName))
            continue;  // 其他节点仍引用该照片，保留。
        const QString fullPath = photosDir.filePath(fileName);
        if (QFile::exists(fullPath) && QFile::remove(fullPath))
            removed.append(fileName);
    }

    rebuildTree();
    m_view->showNode(0);
    refreshActionState();
    saveData();

    if (!removed.isEmpty()) {
        setStatus(QStringLiteral("已删除分类，并清理 %1 个不再被引用的照片文件。")
                      .arg(removed.size()));
    }
}

void MainWindow::removeSelected()
{
    const int id = selectedNodeId();
    const TaxonNode* n = id > 0 ? m_document->node(id) : nullptr;
    if (!n)
        return;

    // subtreeSize 包含节点自身，“其下”的子孙节点数要减 1。
    const int descendants = subtreeSize(id) - 1;
    const QString question = descendants > 0
        ? QStringLiteral(
              "确定删除“%1”吗？\n\n将级联删除其下 %2 个分类节点，"
              "相关物种资料（描述、属性、拉丁学名）会一并删除。\n"
              "删除后，不再被其他植物引用的照片文件也会一并清理。")
              .arg(n->name).arg(descendants)
        : QStringLiteral(
              "确定删除“%1”吗？\n\n"
              "该节点的物种资料（如有）会一并删除；"
              "不再被其他植物引用的照片文件也会一并清理。")
              .arg(n->name);
    if (QMessageBox::question(this, QStringLiteral("确认删除"), question)
        != QMessageBox::Yes)
        return;

    removeSubtreeAndPhotos(id);
}

void MainWindow::batchDelete()
{
    const QList<int> ids = selectedNodeIds();
    if (ids.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("批量删除"),
                                 QStringLiteral("请先在左侧分类树中多选要删除的节点"
                                                "（按住 Ctrl 或 Shift 点选）。"));
        return;
    }

    // 若选中节点存在父子关系，只保留“最上层”节点，避免重复删除子树。
    QSet<int> idSet(ids.begin(), ids.end());
    QList<int> topIds;
    for (int id : ids) {
        const TaxonNode* n = m_document->node(id);
        if (!n)
            continue;
        // 向上检查祖先是否也在选中集合里。
        bool ancestorSelected = false;
        const TaxonNode* p = m_document->parent(id);
        while (p) {
            if (idSet.contains(p->id)) {
                ancestorSelected = true;
                break;
            }
            p = m_document->parent(p->id);
        }
        if (!ancestorSelected)
            topIds.append(id);
    }

    if (topIds.isEmpty())
        return;

    const QString question = QStringLiteral(
        "确定删除选中的 %1 个节点吗？\n\n"
        "将级联删除这些节点下的所有子节点，"
        "相关物种资料与不再被引用的照片也会一并清理。\n"
        "此操作可通过“撤销”逐步恢复。").arg(topIds.size());
    if (QMessageBox::question(this, QStringLiteral("确认批量删除"), question)
        != QMessageBox::Yes)
        return;

    for (int id : topIds)
        removeSubtreeAndPhotos(id);

    rebuildTree();
    refreshActionState();
    saveData();
    setStatus(QStringLiteral("已批量删除 %1 个节点。").arg(topIds.size()));
}

void MainWindow::batchSetAttribute()
{
    // 只对“种/亚种/变种/变型/品种”这类能承载资料的节点批量设置属性。
    // 注意：没有资料（即还没有拉丁学名）的节点无法 setInfo（学名必填），
    // 需要预先过滤并在提示中说明，避免“说改 N 个实际只改几个”的误导。
    QList<int> plantIds;
    int skippedNoInfo = 0;
    for (int id : selectedNodeIds()) {
        const TaxonNode* n = m_document->node(id);
        if (!n || !TaxonRanks::canHostPlantInfo(n->rank))
            continue;
        if (!n->hasInfo) {
            ++skippedNoInfo;
            continue;
        }
        plantIds.append(id);
    }
    if (plantIds.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("批量设置属性"),
                                 skippedNoInfo > 0
                                     ? QStringLiteral(
                                           "选中的植物都没有已保存的资料"
                                           "（尚未填写拉丁学名），无法批量设置属性。\n\n"
                                           "请先为它们逐个编辑并保存资料，"
                                           "或改选已保存资料的植物。")
                                     : QStringLiteral("请先多选“种 / 亚种”等可承载资料的植物节点。"));
        return;
    }

    // 用一个简单对话框让用户选“属性 + 值”。
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("批量设置属性"));
    dialog.resize(420, 200);
    auto* layout = new QVBoxLayout(&dialog);

    auto* form = new QFormLayout;
    auto* attrCombo = new QComboBox(&dialog);
    attrCombo->addItem(QStringLiteral("生长习性"), QStringLiteral("habit"));
    attrCombo->addItem(QStringLiteral("光照"), QStringLiteral("light"));
    attrCombo->addItem(QStringLiteral("水分"), QStringLiteral("water"));
    attrCombo->addItem(QStringLiteral("生命周期"), QStringLiteral("lifecycle"));
    attrCombo->addItem(QStringLiteral("叶型"), QStringLiteral("foliage"));
    attrCombo->addItem(QStringLiteral("生长速度"), QStringLiteral("growth_rate"));
    attrCombo->addItem(QStringLiteral("生境"), QStringLiteral("habitat"));
    form->addRow(QStringLiteral("要设置的属性："), attrCombo);

    auto* valueCombo = new QComboBox(&dialog);
    form->addRow(QStringLiteral("设置为："), valueCombo);
    layout->addLayout(form);

    // 属性切换时，动态更新“值”下拉框的候选项。
    auto updateValueOptions = [&]() {
        const QString key = attrCombo->currentData().toString();
        valueCombo->clear();
        if (key == QLatin1String("habit")) {
            for (auto v : { GrowthHabit::Tree, GrowthHabit::Shrub, GrowthHabit::Herb,
                            GrowthHabit::Vine, GrowthHabit::Aquatic, GrowthHabit::Succulent,
                            GrowthHabit::Fern, GrowthHabit::Other })
                valueCombo->addItem(habitLabel(v), habitToKey(v));
        } else if (key == QLatin1String("light")) {
            for (auto v : { LightPreference::FullSun, LightPreference::HalfSun,
                            LightPreference::HalfShade, LightPreference::Shade })
                valueCombo->addItem(lightLabel(v), lightToKey(v));
        } else if (key == QLatin1String("water")) {
            for (auto v : { WaterPreference::Dry, WaterPreference::Moderate,
                            WaterPreference::Moist, WaterPreference::Aquatic })
                valueCombo->addItem(waterLabel(v), waterToKey(v));
        } else if (key == QLatin1String("lifecycle")) {
            for (auto v : { LifeCycle::Annual, LifeCycle::Biennial, LifeCycle::Perennial })
                valueCombo->addItem(lifecycleLabel(v), lifecycleToKey(v));
        } else if (key == QLatin1String("foliage")) {
            for (auto v : { FoliageType::Evergreen, FoliageType::SemiEvergreen,
                            FoliageType::Deciduous })
                valueCombo->addItem(foliageLabel(v), foliageToKey(v));
        } else if (key == QLatin1String("growth_rate")) {
            for (auto v : { GrowthRate::Slow, GrowthRate::Medium, GrowthRate::Fast })
                valueCombo->addItem(growthRateLabel(v), growthRateToKey(v));
        } else if (key == QLatin1String("habitat")) {
            // 生境是自由文本，用可编辑下拉框。
            valueCombo->setEditable(true);
            valueCombo->setInsertPolicy(QComboBox::NoInsert);
        }
    };
    connect(attrCombo, &QComboBox::currentIndexChanged, &dialog, updateValueOptions);
    updateValueOptions();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                         &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("应用到所选 %1 个植物")
                                                       .arg(plantIds.size()));
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString key = attrCombo->currentData().toString();
    const QString value = valueCombo->currentData().toString();

    // 生境为自由文本：空文本意味着“未填写”，直接应用会清空所有选中植物的
    // 生境。误触概率高，拒绝空值并提示。
    if (key == QLatin1String("habitat")
        && valueCombo->currentText().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("批量设置属性"),
                             QStringLiteral("生境不能为空。\n"
                                            "请输入要设置的生境文本，"
                                            "或取消本次操作。"));
        return;
    }

    int applied = 0;
    for (int id : plantIds) {
        const TaxonNode* n = m_document->node(id);
        if (!n || !n->hasInfo)
            continue;

        SpeciesInfo info = n->info;
        if (key == QLatin1String("habit"))
            info.habit = habitFromKey(value);
        else if (key == QLatin1String("light"))
            info.light = lightFromKey(value);
        else if (key == QLatin1String("water"))
            info.water = waterFromKey(value);
        else if (key == QLatin1String("lifecycle"))
            info.lifeCycle = lifecycleFromKey(value);
        else if (key == QLatin1String("foliage"))
            info.foliage = foliageFromKey(value);
        else if (key == QLatin1String("growth_rate"))
            info.growthRate = growthRateFromKey(value);
        else if (key == QLatin1String("habitat"))
            info.habitat = valueCombo->currentText().trimmed();

        // 记录撤销命令（每个节点一条，可逐个撤销）。
        UndoCommand cmd;
        cmd.type = UndoCommand::SetInfo;
        cmd.nodeId = id;
        cmd.oldInfo = n->info;
        cmd.hadOldInfo = true;
        cmd.newInfo = info;
        cmd.hadNewInfo = true;

        QString error;
        if (m_document->setInfo(id, info, &error)) {
            m_undoManager.push(cmd);
            ++applied;
        }
    }

    rebuildTree();
    m_view->showNode(selectedNodeId());
    refreshActionState();
    saveData();
    if (skippedNoInfo > 0) {
        setStatus(QStringLiteral("已对 %1 个植物批量设置属性"
                                 "（%2 个尚无资料的植物被跳过）。")
                      .arg(applied).arg(skippedNoInfo));
    } else {
        setStatus(QStringLiteral("已对 %1 个植物批量设置属性。").arg(applied));
    }
}

bool MainWindow::saveData()
{
    if (!m_document || m_document->isEmpty())
        return true;

    QString backupNote;
    if (m_loadFailed) {
        const QString backupPath = QDir(m_dataDir).filePath(
            QStringLiteral("plantmap.invalid-%1.json")
                .arg(QDateTime::currentDateTime().toString(
                    QStringLiteral("yyyyMMdd_hhmmss_zzz"))));
        if (QFile::exists(m_dataFile) && !QFile::copy(m_dataFile, backupPath)) {
            showError(QStringLiteral("无法备份原数据文件"),
                      QStringLiteral("保存前需要先保留无法加载的原文件，"
                                     "但复制备份失败：\n%1\n\n"
                                     "为避免丢失原数据，本次没有保存。")
                          .arg(backupPath));
            return false;
        }
        backupNote = backupPath;
    }

    QString error;
    if (!m_document->saveToFile(m_dataFile, &error)) {
        showError(QStringLiteral("保存失败"), error);
        return false;
    }

    m_loadFailed = false;
    if (!backupNote.isEmpty()) {
        setStatus(QStringLiteral("原数据文件已备份到 %1，新数据已保存：%2")
                      .arg(backupNote, m_dataFile));
    } else {
        setStatus(QStringLiteral("已保存：%1").arg(m_dataFile));
    }
    return true;
}

void MainWindow::cloneSelected()
{
    const int id = selectedNodeId();
    const TaxonNode* n = id > 0 ? m_document->node(id) : nullptr;
    if (!n)
        return;

    bool ok = false;
    const QString newName = QInputDialog::getText(
        this, QStringLiteral("复制节点"),
        QStringLiteral("将复制“%1”及其全部下级。\n请输入副本名称：").arg(n->name),
        QLineEdit::Normal, n->name + QStringLiteral("（副本）"), &ok).trimmed();
    if (!ok || newName.isEmpty())
        return;

    QString error;
    const int newId = m_document->cloneSubtree(id, n->parentId, newName, &error);
    if (newId <= 0) {
        showError(QStringLiteral("复制失败"), error);
        return;
    }
    rebuildTree(newId);
    m_view->showNode(newId);
    refreshActionState();
    saveData();
    setStatus(QStringLiteral("已复制节点，副本内拉丁学名已自动加后缀避免冲突。"));
}

void MainWindow::exportCopy()
{
    // 先落盘，保证导出的副本是当前最新数据。
    if (!m_document->isEmpty() && !saveData())
        return;

    const QString target = QFileDialog::getExistingDirectory(
        this, QStringLiteral("选择导出目录（将在此新建一个副本文件夹）"));
    if (target.isEmpty())
        return;

    const QString dest = QDir(target).filePath(
        QStringLiteral("PlantMap_%1").arg(
            QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss"))));
    if (!QDir().mkpath(dest)) {
        showError(QStringLiteral("导出失败"), QStringLiteral("无法创建副本目录：%1").arg(dest));
        return;
    }

    QString error;
    bool ok = true;
    if (QFile::exists(m_dataFile)
        && !QFile::copy(m_dataFile, QDir(dest).filePath(QStringLiteral("plantmap.json")))) {
        ok = false;
        error = QStringLiteral("复制数据库文件失败。");
    }
    if (ok) {
        const QDir srcPhotos(QDir(m_dataDir).filePath(QStringLiteral("photos")));
        if (srcPhotos.exists()) {
            if (!QDir().mkpath(QDir(dest).filePath(QStringLiteral("photos")))) {
                ok = false;
                error = QStringLiteral("创建照片目录失败。");
            } else if (!copyPhotoFiles(srcPhotos,
                                       QDir(QDir(dest).filePath(QStringLiteral("photos"))),
                                       &error)) {
                ok = false;
            }
        }
    }

    if (!ok) {
        showError(QStringLiteral("导出失败"), error);
        return;
    }
    QMessageBox::information(this, QStringLiteral("导出完成"),
                             QStringLiteral("副本已导出到：\n%1").arg(dest));
}

void MainWindow::exportCsv()
{
    QString csv;
    QString error;
    if (!CsvExport::generate(*m_document, &csv, &error)) {
        showError(QStringLiteral("导出失败"), error);
        return;
    }

    const QString defaultName = QDir(m_dataDir).filePath(
        QStringLiteral("plantmap_导出_%1.csv").arg(
            QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss"))));
    const QString filePath = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出 CSV"),
        defaultName, QStringLiteral("CSV 文件 (*.csv)"));
    if (filePath.isEmpty())
        return;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        showError(QStringLiteral("导出失败"),
                  QStringLiteral("无法写入文件：%1").arg(file.errorString()));
        return;
    }
    file.write(csv.toUtf8());
    file.close();

    QMessageBox::information(this, QStringLiteral("导出完成"),
                             QStringLiteral("已导出 %1 条植物资料到：\n%2")
                                 .arg(QString::number(m_document->infoCount()))
                                 .arg(filePath));
}

void MainWindow::expandAllTree()
{
    m_tree->expandAll();
}

void MainWindow::collapseAllTree()
{
    m_tree->collapseAll();
}

void MainWindow::refreshUndoActions()
{
    if (m_undoAction)
        m_undoAction->setEnabled(m_undoManager.canUndo());
    if (m_redoAction)
        m_redoAction->setEnabled(m_undoManager.canRedo());
}

void MainWindow::undo()
{
    const int newId = m_undoManager.undo();
    if (newId < 0)
        return;
    rebuildTree(newId);
    m_view->showNode(newId > 0 ? newId : 0);
    m_view->setFavorite(newId > 0 && m_favorites.contains(newId));
    m_view->setCompared(newId > 0 && m_compareList.contains(newId));
    refreshActionState();
    refreshUndoActions();
}

void MainWindow::redo()
{
    const int newId = m_undoManager.redo();
    if (newId < 0)
        return;
    rebuildTree(newId);
    m_view->showNode(newId > 0 ? newId : 0);
    m_view->setFavorite(newId > 0 && m_favorites.contains(newId));
    m_view->setCompared(newId > 0 && m_compareList.contains(newId));
    refreshActionState();
    refreshUndoActions();
}

void MainWindow::openDataDir()
{
    QDir().mkpath(m_dataDir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_dataDir));
}

void MainWindow::toggleFavorite(int nodeId)
{
    const bool nowFavorite = m_favorites.toggle(nodeId);
    m_view->setFavorite(nowFavorite);
    m_favorites.save();
}

void MainWindow::toggleCompare(int nodeId)
{
    if (nodeId <= 0)
        return;
    if (m_compareList.contains(nodeId)) {
        m_compareList.removeAll(nodeId);
    } else {
        // 对比上限 5 个，超出时提示。
        if (m_compareList.size() >= 5) {
            QMessageBox::information(
                this, QStringLiteral("加入对比"),
                QStringLiteral("对比列表最多 5 个植物。\n"
                               "请先在对比页面移除一个，再加入新的。"));
            m_view->setCompared(false);
            return;
        }
        m_compareList.append(nodeId);
    }
    m_view->setCompared(m_compareList.contains(nodeId));
    setStatus(m_compareList.contains(nodeId)
                  ? QStringLiteral("已加入对比（当前 %1 个）。").arg(m_compareList.size())
                  : QStringLiteral("已从对比列表移除（当前 %1 个）。").arg(m_compareList.size()));
}

void MainWindow::openFavorites()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("收藏夹"));
    dialog.resize(440, 520);

    auto* layout = new QVBoxLayout(&dialog);
    auto* list = new QListWidget(&dialog);
    list->setIconSize(QSize(40, 40));
    layout->addWidget(list, 1);

    const QList<int> ids = m_favorites.ids();
    for (int id : ids) {
        const TaxonNode* n = m_document->node(id);
        if (!n)
            continue;
        auto* item = new QListWidgetItem(
            n->info.scientificName.isEmpty()
                ? n->name
                : QStringLiteral("%1（%2）").arg(n->name, n->info.scientificName));
        item->setData(NodeIdRole, n->id);
        item->setToolTip(m_document->displayPathOf(n->id));
        list->addItem(item);
    }
    if (list->count() == 0) {
        auto* empty = new QListWidgetItem(
            QStringLiteral("还没有收藏任何植物。\n\n在植物详情页点击“☆ 收藏”即可加入收藏夹。"));
        empty->setFlags(empty->flags() & ~Qt::ItemIsEnabled);
        list->addItem(empty);
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    connect(list, &QListWidget::itemDoubleClicked, &dialog,
            [&](QListWidgetItem* item) {
        const int id = item->data(NodeIdRole).toInt();
        if (id <= 0)
            return;
        m_searchEdit->clear();
        rebuildTree(id);
        m_view->showNode(id);
        m_view->setFavorite(m_favorites.contains(id));
        m_view->setCompared(m_compareList.contains(id));
        refreshActionState();
        dialog.accept();
    });

    dialog.exec();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (!saveData()) {
        event->ignore();
        return;
    }
    saveUiState();
    event->accept();
}

void MainWindow::restoreUiState()
{
    QSettings settings;
    const QByteArray geometry =
        settings.value(QStringLiteral("ui/geometry")).toByteArray();
    if (!geometry.isEmpty())
        restoreGeometry(geometry);
    const QByteArray splitterState =
        settings.value(QStringLiteral("ui/splitter")).toByteArray();
    if (!splitterState.isEmpty() && m_splitter)
        m_splitter->restoreState(splitterState);
}

void MainWindow::saveUiState()
{
    QSettings settings;
    settings.setValue(QStringLiteral("ui/geometry"), saveGeometry());
    if (m_splitter)
        settings.setValue(QStringLiteral("ui/splitter"), m_splitter->saveState());
}

void MainWindow::showError(const QString& title, const QString& message)
{
    QMessageBox::warning(this, title, message);
}

void MainWindow::setStatus(const QString& message)
{
    statusBar()->showMessage(message, 6000);
}
