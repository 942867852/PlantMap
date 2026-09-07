#pragma once

#include <QByteArray>
#include <QHash>
#include <QIcon>
#include <QJsonObject>
#include <QMainWindow>
#include <QCloseEvent>
#include <QMap>
#include <QSet>
#include <QVector>

#include "speciesview.h"
#include "taxondocument.h"

class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QTreeWidget;
class QTreeWidgetItem;
class QAction;

// 撤销/重做命令。删除节点的撤销需要子树 JSON 与被删照片文件字节，
// 故一并保存在命令里。
struct UndoCommand {
    enum Type { AddNode, RemoveNode, RenameNode, SetInfo };
    Type type = AddNode;
    int nodeId = 0;
    int parentId = 0;
    QString oldName;
    QString newName;
    QJsonObject subtree;
    QMap<QString, QByteArray> photos;
    SpeciesInfo oldInfo;
    SpeciesInfo newInfo;
    bool hadOldInfo = false;
    bool hadNewInfo = false;
};

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
    void loadFavorites();
    void saveFavorites();
    void pushUndo(const UndoCommand& command);
    int applyUndoCommand(const UndoCommand& command, bool inverse);
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
    QSet<int> m_favorites;
    QVector<UndoCommand> m_undoStack;
    QVector<UndoCommand> m_redoStack;
    QString m_dataDir;
    QString m_dataFile;
    bool m_loadFailed = false;
};
