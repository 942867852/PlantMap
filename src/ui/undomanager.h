#pragma once

#include <QMap>
#include <QByteArray>
#include <QJsonObject>
#include <QSet>
#include <QString>
#include <QVector>

#include "speciesinfo.h"

class TaxonomyDocument;

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

/*
 * 分类树编辑的撤销/重做管理器：维护 undo/redo 双栈（栈深上限 100）。
 *
 * 命令执行需要访问 TaxonomyDocument 和照片目录，故把 apply 逻辑也放在
 * 这里。调用方（MainWindow）负责在命令执行后重建树、刷新视图。
 *
 * apply 返回命令涉及的节点新 id（Add/Remove 会重建节点，id 可能变化），
 * 0 表示命令不再对应任何节点（如撤销“新增”后节点已删除）。
 */
class UndoManager
{
public:
    UndoManager();

    void setDocument(TaxonomyDocument* document);
    void setDataDir(const QString& dataDir);

    // 记录一条已执行的操作；会清空 redo 栈。
    void push(const UndoCommand& command);

    // 执行撤销；栈空时返回 -1。
    int undo();
    // 执行重做；栈空时返回 -1。
    int redo();

    bool canUndo() const;
    bool canRedo() const;

    // 供调用方在应用命令前后刷新动作按钮 enable 状态。
    void clear();

private:
    int apply(const UndoCommand& c, bool inverse);
    QSet<QString> collectSubtreePhotos(int id) const;
    QSet<QString> collectAllReferencedPhotos() const;

    TaxonomyDocument* m_document = nullptr;
    QString m_dataDir;
    QVector<UndoCommand> m_undoStack;
    QVector<UndoCommand> m_redoStack;
};
