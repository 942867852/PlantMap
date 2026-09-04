#pragma once

#include <QString>
#include <QVector>
#include <QHash>
#include <QSet>
#include <QJsonObject>
#include <QSharedPointer>

#include "taxonrank.h"
#include "speciesinfo.h"

// 分类树节点。id 全文档唯一，由 TaxonomyDocument 管理。
struct TaxonNode {
    int id = 0;
    int parentId = 0;      // 0 表示顶级节点
    TaxonRank rank = TaxonRank::Kingdom;
    QString name;          // 中文/常用名，同一父节点下唯一
    QVector<int> childIds;
    bool hasInfo = false;  // 只有种/亚种可以携带资料
    SpeciesInfo info;

    bool isRoot() const { return parentId == 0; }
    bool isTerminalForInfo() const { return TaxonRanks::canHostPlantInfo(rank); }
};

/*
 * 整个植物库：分类树 + 种/亚种资料 + 拉丁学名哈希索引。
 *
 * 拉丁学名索引：
 *   规范化（小写、去除空白与标点）后的学名 -> 节点 id。
 *   任何 setInfo / 删除节点都会同步更新索引，因此查重 O(1)。
 */
class TaxonomyDocument
{
public:
    TaxonomyDocument() = default;
    ~TaxonomyDocument() = default;

    TaxonomyDocument(const TaxonomyDocument&) = delete;
    TaxonomyDocument& operator=(const TaxonomyDocument&) = delete;

    void clear();
    bool isEmpty() const;
    int nodeCount() const;
    int infoCount() const;

    // ---- 查询 ----
    const TaxonNode* node(int id) const;
    TaxonNode* node(int id);
    QVector<int> roots() const;
    QVector<int> childIdsOf(int id) const;
    const TaxonNode* parent(int id) const;
    QString displayPathOf(int id) const;

    bool hasInfo(int id) const;
    const SpeciesInfo* infoOf(int id) const;
    int findNodeByScientificName(const QString& latinName) const;

    // ---- 分类树编辑 ----
    // parentId == 0 时新建“界”顶级节点；否则自动推导下一级。
    // 成功返回新节点 id；失败返回 0 并写 err。
    int addNode(int parentId, const QString& name, QString* error = nullptr);
    bool removeNode(int id, QString* error = nullptr);   // 级联删除子树
    bool renameNode(int id, const QString& newName, QString* error = nullptr);

    // ---- 资料编辑 ----
    // 校验等级、拉丁学名非空与全库唯一；全部通过才落盘到节点。
    bool setInfo(int id, const SpeciesInfo& info, QString* error = nullptr);
    bool clearInfo(int id, QString* error = nullptr);

    // ---- JSON 持久化 ----
    QJsonObject toJson() const;
    bool loadFromJson(const QJsonObject& root, QString* error = nullptr);
    bool saveToFile(const QString& fileName, QString* error = nullptr) const;
    bool loadFromFile(const QString& fileName, QString* error = nullptr);

    // 规范化拉丁学名，供索引与外部调用
    static QString normalizedScientificName(const QString& latinName);

private:
    const TaxonNode* childByName(int parentId, const QString& name) const;
    bool addLatinToIndex(const QString& latinName, int nodeId);
    void removeLatinForNode(int nodeId);
    void collectSubtreeIds(int id, QVector<int>& ids) const;
    int parseNodeJson(const QJsonObject& obj, int parentId, int& nextId,
                      QSet<int>& seenIds, QString& error);
    QJsonObject nodeToJson(const TaxonNode& node) const;

    QHash<int, QSharedPointer<TaxonNode>> m_nodes;
    QVector<int> m_rootIds;
    QHash<QString, int> m_latinIndex;   // 规范化学名 -> 节点 id
    int m_nextId = 1;
};
