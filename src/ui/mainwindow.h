#pragma once

#include <QMainWindow>
#include <QCloseEvent>

#include "speciesview.h"
#include "taxondocument.h"

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
    void saveData();
    void openDataDir();

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
    QString m_dataDir;
    QString m_dataFile;
};
