// taxonomy.h
#ifndef TAXONOMY_H
#define TAXONOMY_H

#include <QString>
#include <QMap>
#include <QSet>
#include <QSharedPointer>
#include <QJsonObject>

// 前向声明
class TaxonNode;

// 分类等级枚举，门纲目科属
enum class TaxonomicRank {
    Kingdom = 0,  // 界
    Phylum,       // 门
    Class,        // 纲
    Order,        // 目
    Family,       // 科
    Genus,        // 属
    Species,      // 种
    Variety       // 变种
};

QString rankToString(TaxonomicRank rank);
TaxonomicRank stringToRank(const QString& str);

// 单个分类节点（代表一个具体的生物分类单元）
class TaxonNode : public QSharedData
{
public:
    explicit TaxonNode(const QString& name, TaxonomicRank rank, TaxonNode* parent = nullptr);

    QString getName() const { return m_name; }
    TaxonomicRank getRank() const { return m_rank; }
    TaxonNode* getParent() const { return m_parent; }

    // 子节点管理
    void addChild(QSharedPointer<TaxonNode> child);
    QList<QSharedPointer<TaxonNode>> getChildren() const;
    bool hasChild(const QString& name) const;

    // 路径相关
    QStringList getFullPath() const; // e.g., "植物界 > 被子植物门 > ... > 玫瑰"
    QString getAncestorName(TaxonomicRank rank) const; // 向上查找并返回上一级的名称

    // JSON 序列化
    QJsonObject toJson() const;
    static QSharedPointer<TaxonNode> fromJson(const QJsonObject& obj, TaxonNode* parent = nullptr);
private:
    QString m_name;
    TaxonomicRank m_rank;
    TaxonNode* m_parent;
    QList<QSharedPointer<TaxonNode>> m_children;
};

// 全局分类树管理器（类似 TraitRegistry）
class TaxonomyRegistry : public QObject
{
    Q_OBJECT

public:
    static TaxonomyRegistry& instance();

    // 获取根节点
    QSharedPointer<TaxonNode> getRoot() const { return m_root; }

    // 添加新分类（自动按层级插入）
    bool addTaxon(const QString& fullName, const QString& separator = " > ");
    bool addTaxonByParts(const QStringList& parts); // ["植物界", "被子植物门", ..., "玫瑰"]

    // 查找节点
    QSharedPointer<TaxonNode> findNodeByName(const QString& name) const;
    QList<QSharedPointer<TaxonNode>> findNodesByRank(TaxonomicRank rank, const QString& name) const;

    // 导入导出
    bool exportToJson(const QString& filename) const;
    bool importFromJson(const QString& filename);

signals:
    void taxonomyChanged(); // UI 刷新

private:
    explicit TaxonomyRegistry(QObject* parent = nullptr);
    ~TaxonomyRegistry() = default;

    QSharedPointer<TaxonNode> m_root;
    QMap<QString, QList<QSharedPointer<TaxonNode>>> m_nodesByName; // 名称索引（用于快速查找）
};

inline TaxonomyRegistry& Taxons() { return TaxonomyRegistry::instance(); }

#endif // TAXONOMY_H
