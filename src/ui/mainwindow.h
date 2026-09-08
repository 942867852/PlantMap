#pragma once

#include <QHash>
#include <QIcon>
#include <QMainWindow>
#include <QCloseEvent>
#include <QSet>

#include "favoritesstore.h"
#include "speciesview.h"
#include "taxondocument.h"
#include "undomanager.h"

class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QTreeWidget;
class QTreeWidgetItem;
class QAction;
class QTimer;
class QSplitter;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const QString& dataDir, QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onTreeSelectionChanged();
    void onSearchTextChanged(const QString& text);
    void runSearchDebounced();
    void openSpeciesEditor();
    void openAdvancedSearch();
    void importDatabase();
    void addUnderSelected();
    void renameSelected();
    void removeSelected();
    bool saveData();
    void openDataDir();
    void toggleFavorite(int nodeId);
    void openFavorites();
    void cloneSelected();
    void exportCopy();
    void exportCsv();
    void expandAllTree();
    void collapseAllTree();
    void batchSetAttribute();
    void batchDelete();
    void openStats();
    void openCompare();
    void undo();
    void redo();

private:
    void buildUi();
    void rebuildTree(int selectNodeId = 0);
    void refreshActionState();
    int selectedNodeId() const;
    int subtreeSize(int id) const;
    void goToPlantFromSearch(QListWidgetItem* item);
    bool selectFirstPlantItem();
    void showError(const QString& title, const QString& message);
    void setStatus(const QString& message);
    void refreshUndoActions();
    // 恢复/保存窗口与分隔条布局（QSettings）。
    void restoreUiState();
    void saveUiState();
    // 收集当前树中所有选中节点的 id（含多选）。
    QList<int> selectedNodeIds() const;
    // 收集某节点子树内所有被引用的照片文件名（相对 photos/ 的文件名）。
    QSet<QString> collectSubtreePhotos(int id) const;
    // 收集整个文档当前所有被引用的照片文件名。
    QSet<QString> collectAllReferencedPhotos() const;
    // 删除某节点子树，并清理不再被任何节点引用的照片文件。
    void removeSubtreeAndPhotos(int id);

    TaxonomyDocument* m_document = nullptr;
    SpeciesViewForm* m_view = nullptr;
    QTreeWidget* m_tree = nullptr;
    QSplitter* m_splitter = nullptr;
    QLabel* m_treeCaption = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QLabel* m_searchCaption = nullptr;
    QListWidget* m_searchResults = nullptr;
    QAction* m_addAction = nullptr;
    QAction* m_editAction = nullptr;
    QAction* m_renameAction = nullptr;
    QAction* m_deleteAction = nullptr;
    QAction* m_saveAction = nullptr;
    QAction* m_undoAction = nullptr;
    QAction* m_redoAction = nullptr;
    QHash<QString, QIcon> m_searchThumbCache;
    FavoritesStore m_favorites;
    UndoManager m_undoManager;
    QTimer* m_searchDebounceTimer = nullptr;
    QString m_pendingSearchText;
    QString m_dataDir;
    QString m_dataFile;
    bool m_loadFailed = false;
};
