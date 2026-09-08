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
    void expandAllTree();
    void collapseAllTree();
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
    // 收集某节点子树内所有被引用的照片文件名（相对 photos/ 的文件名）。
    QSet<QString> collectSubtreePhotos(int id) const;
    // 收集整个文档当前所有被引用的照片文件名。
    QSet<QString> collectAllReferencedPhotos() const;
    // 删除某节点子树，并清理不再被任何节点引用的照片文件。
    void removeSubtreeAndPhotos(int id);

    TaxonomyDocument* m_document = nullptr;
    SpeciesViewForm* m_view = nullptr;
    QTreeWidget* m_tree = nullptr;
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
    QString m_dataDir;
    QString m_dataFile;
    bool m_loadFailed = false;
};
