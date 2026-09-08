#pragma once

#include <QList>
#include <QSet>
#include <QString>

class TaxonomyDocument;

/*
 * 收藏夹存储：维护“已收藏植物节点 id”集合，并持久化到数据目录的
 * favorites.json（一个 int 数组）。加载时会过滤掉已不存在或不允许
 * 承载资料的节点 id。
 */
class FavoritesStore
{
public:
    explicit FavoritesStore(const QString& dataDir);

    void setDocument(TaxonomyDocument* document);

    // 从 favorites.json 载入（过滤无效 id）；文件不存在时保持空集合。
    void load();
    // 把当前集合写回 favorites.json（按 id 升序）。
    void save() const;
    // 清空全部收藏并写回磁盘（导入新数据库后调用，旧 id 不适用新库）。
    void clearAll();
    // 批量移除收藏（删除节点子树后调用，避免死 id 残留）；若集合有变化会写盘。
    void removeIds(const QList<int>& nodeIds);

    bool contains(int nodeId) const;
    // 切换收藏状态；返回切换后的状态（true=已收藏）。
    bool toggle(int nodeId);
    // 所有已收藏节点 id（升序）。
    QList<int> ids() const;

private:
    QString m_dataDir;
    TaxonomyDocument* m_document = nullptr;
    QSet<int> m_favorites;
};
