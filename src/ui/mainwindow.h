#pragma once

#include <QMainWindow>
#include <QCloseEvent>

#include "speciesform.h"
#include "taxondocument.h"

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
    void showError(const QString& title, const QString& message);
    void setStatus(const QString& message);

    TaxonomyDocument* m_document = nullptr;
    SpeciesForm* m_form = nullptr;
    QTreeWidget* m_tree = nullptr;
    QAction* m_addAction = nullptr;
    QAction* m_renameAction = nullptr;
    QAction* m_deleteAction = nullptr;
    QAction* m_saveAction = nullptr;
    QString m_dataDir;
    QString m_dataFile;
};
