#include "undomanager.h"

#include "taxondocument.h"

#include <QDir>
#include <QFile>

#include <functional>

UndoManager::UndoManager() = default;

void UndoManager::setDocument(TaxonomyDocument* document)
{
    m_document = document;
}

void UndoManager::setDataDir(const QString& dataDir)
{
    m_dataDir = dataDir;
}

void UndoManager::push(const UndoCommand& command)
{
    m_undoStack.append(command);
    m_redoStack.clear();
    if (m_undoStack.size() > 100)
        m_undoStack.removeFirst();
}

int UndoManager::undo()
{
    if (m_undoStack.isEmpty())
        return -1;
    UndoCommand c = m_undoStack.takeLast();
    const int newId = apply(c, false);
    if (c.type == UndoCommand::AddNode || c.type == UndoCommand::RemoveNode)
        c.nodeId = newId;
    m_redoStack.append(c);
    return newId;
}

int UndoManager::redo()
{
    if (m_redoStack.isEmpty())
        return -1;
    UndoCommand c = m_redoStack.takeLast();
    const int newId = apply(c, true);
    if (c.type == UndoCommand::AddNode || c.type == UndoCommand::RemoveNode)
        c.nodeId = newId;
    m_undoStack.append(c);
    return newId;
}

bool UndoManager::canUndo() const
{
    return !m_undoStack.isEmpty();
}

bool UndoManager::canRedo() const
{
    return !m_redoStack.isEmpty();
}

void UndoManager::clear()
{
    m_undoStack.clear();
    m_redoStack.clear();
}

QSet<QString> UndoManager::collectSubtreePhotos(int id) const
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

QSet<QString> UndoManager::collectAllReferencedPhotos() const
{
    QSet<QString> photos;
    for (int rootId : m_document->roots())
        photos.unite(collectSubtreePhotos(rootId));
    return photos;
}

// 执行一个命令（inverse=false 为撤销方向，true 为重做方向）。
// 返回命令涉及的节点新 id（Add/Remove 会重建节点，id 可能变化）。
int UndoManager::apply(const UndoCommand& c, bool inverse)
{
    switch (c.type) {
    case UndoCommand::AddNode:
        if (!inverse) {
            m_document->removeNode(c.nodeId);
            return 0;
        }
        return m_document->restoreSubtreeFromJson(c.subtree, c.parentId);

    case UndoCommand::RemoveNode:
        if (!inverse) {
            const int newId = m_document->restoreSubtreeFromJson(c.subtree, c.parentId);
            // 恢复被删的照片文件。
            const QDir photosDir(QDir(m_dataDir).filePath(QStringLiteral("photos")));
            QDir().mkpath(photosDir.absolutePath());
            for (auto it = c.photos.constBegin(); it != c.photos.constEnd(); ++it) {
                const QString path = photosDir.filePath(it.key());
                if (!QFile::exists(path)) {
                    QFile f(path);
                    if (f.open(QIODevice::WriteOnly)) {
                        f.write(it.value());
                        f.close();
                    }
                }
            }
            return newId;
        } else {
            // 重做删除：删除当前子树，并清理不再被引用的照片。
            const QSet<QString> subPhotos = collectSubtreePhotos(c.nodeId);
            m_document->removeNode(c.nodeId);
            const QSet<QString> still = collectAllReferencedPhotos();
            const QDir photosDir(QDir(m_dataDir).filePath(QStringLiteral("photos")));
            for (const QString& fn : subPhotos) {
                if (!still.contains(fn)) {
                    const QString path = photosDir.filePath(fn);
                    if (QFile::exists(path))
                        QFile::remove(path);
                }
            }
            return 0;
        }

    case UndoCommand::RenameNode:
        if (!inverse)
            m_document->renameNode(c.nodeId, c.oldName);
        else
            m_document->renameNode(c.nodeId, c.newName);
        return c.nodeId;

    case UndoCommand::SetInfo:
        if (!inverse) {
            if (c.hadOldInfo)
                m_document->setInfo(c.nodeId, c.oldInfo);
            else
                m_document->clearInfo(c.nodeId);
        } else {
            if (c.hadNewInfo)
                m_document->setInfo(c.nodeId, c.newInfo);
            else
                m_document->clearInfo(c.nodeId);
        }
        return c.nodeId;
    }
    return 0;
}
