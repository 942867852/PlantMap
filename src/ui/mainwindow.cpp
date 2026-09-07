#include "mainwindow.h"

#include "dbversion.h"
#include "speciesform.h"
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
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QPixmap>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

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
        m_document = document;
        m_dataDir = dataDir;
        m_nodeId = nodeId;

        const TaxonNode* n = document->node(nodeId);
        m_originalHasInfo = n && n->hasInfo;
        if (m_originalHasInfo)
            m_originalInfo = n->info;

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
        if (hasDiscardableChanges()) {
            const QMessageBox::StandardButton answer =
                QMessageBox::warning(
                    this, QStringLiteral("有未保存的修改"),
                    QStringLiteral("当前编辑的内容或照片操作还没有保存。\n\n"
                                   "确定要放弃本次所有修改并关闭吗？"
                                   "（新添加的照片也会被删除）"),
                    QMessageBox::Yes | QMessageBox::No,
                    QMessageBox::No);
            if (answer != QMessageBox::Yes)
                return;

            QString error;
            if (!rollback(&error)) {
                QMessageBox::critical(
                    this, QStringLiteral("无法还原"),
                    QStringLiteral("还原失败，为避免数据丢失窗口不会关闭。\n\n%1")
                        .arg(error));
                return;
            }
        }
        QDialog::reject();
    }

private:
    bool hasDiscardableChanges() const
    {
        if (m_form->hasUnsavedChanges())
            return true;

        const TaxonNode* n = m_document->node(m_nodeId);
        const bool currentHasInfo = n && n->hasInfo;
        if (currentHasInfo != m_originalHasInfo)
            return true;

        const SpeciesInfo currentInfo =
            currentHasInfo ? n->info : SpeciesInfo();
        return !(currentInfo == m_originalInfo);
    }

    bool rollback(QString* error)
    {
        // 先记录当前资料里的照片，删除“本次会话新增”的图片文件。
        const TaxonNode* n = m_document->node(m_nodeId);
        QStringList currentPhotos;
        if (n && n->hasInfo)
            currentPhotos = n->info.photos;

        QStringList addedPhotos = currentPhotos;
        for (const QString& originalPhoto : m_originalInfo.photos)
            addedPhotos.removeAll(originalPhoto);

        bool ok = false;
        if (m_originalHasInfo) {
            ok = m_document->setInfo(m_nodeId, m_originalInfo, error);
        } else {
            ok = m_document->clearInfo(m_nodeId, error);
        }
        if (!ok)
            return false;

        // 文档已还原，再清理本次新增且已无引用的照片文件。
        QStringList failedToRemove;
        for (const QString& relativeName : addedPhotos) {
            const QString fullPath = QDir(m_dataDir).filePath(
                QStringLiteral("photos") + QLatin1Char('/') + relativeName);
            if (QFile::exists(fullPath) && !QFile::remove(fullPath))
                failedToRemove.append(relativeName);
        }
        if (!failedToRemove.isEmpty()) {
            QMessageBox::warning(
                this, QStringLiteral("部分照片文件未能删除"),
                QStringLiteral("数据库已还原，但以下照片文件可能正被占用，"
                               "无法删除：\n\n%1")
                    .arg(failedToRemove.join(QLatin1Char('\n'))));
        }
        return ok;
    }

    TaxonomyDocument* m_document = nullptr;
    QString m_dataDir;
    int m_nodeId = 0;
    bool m_originalHasInfo = false;
    SpeciesInfo m_originalInfo;
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
    selectFirstPlantItem();
    refreshActionState();
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
    m_editAction = toolbar->addAction(QStringLiteral("编辑资料…"));
    m_renameAction = toolbar->addAction(QStringLiteral("重命名"));
    m_deleteAction = toolbar->addAction(QStringLiteral("删除"));
    toolbar->addSeparator();
    m_saveAction = toolbar->addAction(QStringLiteral("保存数据"));
    toolbar->addSeparator();
    auto* advancedAction = toolbar->addAction(QStringLiteral("高级检索…"));
    auto* importAction = toolbar->addAction(QStringLiteral("导入数据库…"));

    connect(m_addAction, &QAction::triggered, this, &MainWindow::addUnderSelected);
    connect(m_editAction, &QAction::triggered, this, &MainWindow::openSpeciesEditor);
    connect(m_renameAction, &QAction::triggered, this, &MainWindow::renameSelected);
    connect(m_deleteAction, &QAction::triggered, this, &MainWindow::removeSelected);
    connect(m_saveAction, &QAction::triggered, this, &MainWindow::saveData);
    connect(advancedAction, &QAction::triggered, this, &MainWindow::openAdvancedSearch);
    connect(importAction, &QAction::triggered, this, &MainWindow::importDatabase);

    auto* menu = menuBar()->addMenu(QStringLiteral("数据"));
    menu->addAction(QStringLiteral("保存数据"), this, &MainWindow::saveData);
    menu->addAction(QStringLiteral("导入数据库…"), this, &MainWindow::importDatabase);
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

    if (!searching) {
        refreshActionState();
        return;
    }

    // 搜索激活时禁用针对“隐藏树节点”的增删改操作，避免误操作。
    m_addAction->setEnabled(false);
    m_editAction->setEnabled(false);
    m_renameAction->setEnabled(false);
    m_deleteAction->setEnabled(false);

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
    if (!m_document->isEmpty())
        saveData();

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
        if (oldPhotosDir.exists())
            oldPhotosDir.removeRecursively();
        QDir().mkpath(currentPhotosPath);
        if (QDir(backupPhotosDir).exists()
            && !copyPhotoFiles(QDir(backupPhotosDir),
                               QDir(currentPhotosPath), &restoreError)) {
            restoreError = QStringLiteral("还原照片失败：%1")
                               .arg(restoreError);
        }
        if (hadOldDb) {
            if (QFile::exists(m_dataFile))
                QFile::remove(m_dataFile);
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

    auto fillEnum = [](QComboBox* combo, int max,
                       auto label, auto key) {
        combo->addItem(QStringLiteral("不限"), QString());
        for (int i = 0; i <= max; ++i) {
            combo->addItem(label(i), key(i));
        }
    };

    auto* habitCombo = new QComboBox(filterBox);
    fillEnum(habitCombo, static_cast<int>(GrowthHabit::Other),
             [](int i) { return habitLabel(static_cast<GrowthHabit>(i)); },
             [](int i) { return habitToKey(static_cast<GrowthHabit>(i)); });
    filterForm->addRow(QStringLiteral("生长习性："), habitCombo);

    auto* lifeCombo = new QComboBox(filterBox);
    fillEnum(lifeCombo, static_cast<int>(LifeCycle::Perennial),
             [](int i) { return lifecycleLabel(static_cast<LifeCycle>(i)); },
             [](int i) { return lifecycleToKey(static_cast<LifeCycle>(i)); });
    filterForm->addRow(QStringLiteral("生命周期："), lifeCombo);

    auto* lightCombo = new QComboBox(filterBox);
    fillEnum(lightCombo, static_cast<int>(LightPreference::Shade),
             [](int i) { return lightLabel(static_cast<LightPreference>(i)); },
             [](int i) { return lightToKey(static_cast<LightPreference>(i)); });
    filterForm->addRow(QStringLiteral("光照："), lightCombo);

    auto* waterCombo = new QComboBox(filterBox);
    fillEnum(waterCombo, static_cast<int>(WaterPreference::Aquatic),
             [](int i) { return waterLabel(static_cast<WaterPreference>(i)); },
             [](int i) { return waterToKey(static_cast<WaterPreference>(i)); });
    filterForm->addRow(QStringLiteral("水分："), waterCombo);

    auto* foliageCombo = new QComboBox(filterBox);
    fillEnum(foliageCombo, static_cast<int>(FoliageType::Deciduous),
             [](int i) { return foliageLabel(static_cast<FoliageType>(i)); },
             [](int i) { return foliageToKey(static_cast<FoliageType>(i)); });
    filterForm->addRow(QStringLiteral("叶型："), foliageCombo);

    auto* rateCombo = new QComboBox(filterBox);
    fillEnum(rateCombo, static_cast<int>(GrowthRate::Fast),
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
    connect(searchButton, &QPushButton::clicked, &dialog, [&] {
        runSearch();
    });
    connect(openButton, &QPushButton::clicked, &dialog, [&] {
        openSelected();
    });
    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(results, &QListWidget::itemDoubleClicked,
            &dialog, [&](QListWidgetItem*) { openSelected(); });
    layout->addWidget(buttons);

    dialog.exec();
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
