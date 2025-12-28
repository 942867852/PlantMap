// taxonomy.cpp
#include "taxonomy.h"
#include <QFile>
#include <QJsonDocument>
#include <QTextStream>
#include <QJsonArray>

// -------------------------------
// 工具函数
// -------------------------------
QString rankToString(TaxonomicRank rank) {
    switch (rank) {
    case TaxonomicRank::Kingdom: return QObject::tr("界");
    case TaxonomicRank::Phylum:  return QObject::tr("门");
    case TaxonomicRank::Class:   return QObject::tr("纲");
    case TaxonomicRank::Order:   return QObject::tr("目");
    case TaxonomicRank::Family:  return QObject::tr("科");
    case TaxonomicRank::Genus:   return QObject::tr("属");
    case TaxonomicRank::Species: return QObject::tr("种");
    case TaxonomicRank::Variety: return QObject::tr("变种");
    default:                     return QObject::tr("未知");
    }
}

TaxonomicRank stringToRank(const QString &str) {
    if (str == QObject::tr("界")) return TaxonomicRank::Kingdom;
    if (str == QObject::tr("门")) return TaxonomicRank::Phylum;
    if (str == QObject::tr("纲")) return TaxonomicRank::Class;
    if (str == QObject::tr("目")) return TaxonomicRank::Order;
    if (str == QObject::tr("科")) return TaxonomicRank::Family;
    if (str == QObject::tr("属")) return TaxonomicRank::Genus;
    if (str == QObject::tr("种")) return TaxonomicRank::Species;
    if (str == QObject::tr("变种")) return TaxonomicRank::Variety;
    return TaxonomicRank::Kingdom;
}

// -------------------------------
// TaxonNode 实现
// -------------------------------
TaxonNode::TaxonNode(const QString &name, TaxonomicRank rank, TaxonNode *parent)
    : m_name(name), m_rank(rank), m_parent(parent)
{}

void TaxonNode::addChild(QSharedPointer<TaxonNode> child)
{
    m_children.append(child);
}

QList<QSharedPointer<TaxonNode>> TaxonNode::getChildren() const
{
    return m_children;
}

bool TaxonNode::hasChild(const QString &name) const
{
    for (const auto& c : m_children) {
        if (c->getName() == name)
            return true;
    }
    return false;
}

QStringList TaxonNode::getFullPath() const
{
    QStringList path;
    const TaxonNode* current = this;
    while (current) {
        path.prepend(current->m_name);
        current = current->m_parent;
    }
    return path;
}

QString TaxonNode::getAncestorName(TaxonomicRank targetRank) const
{
    const TaxonNode* current = this;
    while (current && current->m_rank != targetRank) {
        current = current->m_parent;
    }
    return current ? current->m_name : QString();
}

QJsonObject TaxonNode::toJson() const
{
    QJsonObject obj;
    obj["name"] = m_name;
    obj["rank_str"] = rankToString(m_rank);

    QJsonArray children;
    for (const auto& c : m_children) {
        children.append(c->toJson());
    }
    obj["children"] = children;

    return obj;
}

QSharedPointer<TaxonNode> TaxonNode::fromJson(const QJsonObject &obj, TaxonNode *parent)
{
    QString name = obj["name"].toString();
    TaxonomicRank rank = stringToRank(obj["rank_str"].toString());

    auto node = QSharedPointer<TaxonNode>(new TaxonNode(name, rank, parent));

    QJsonArray children = obj["children"].toArray();
    for (const auto& val : children) {
        auto childObj = val.toObject();
        auto child = TaxonNode::fromJson(childObj, node.data());
        node->addChild(child);
    }

    return node;
}

// -------------------------------
// TaxonomyRegistry 实现
// -------------------------------
TaxonomyRegistry& TaxonomyRegistry::instance()
{
    static TaxonomyRegistry inst;
    return inst;
}

TaxonomyRegistry::TaxonomyRegistry(QObject *parent)
    : QObject(parent)
{
    m_root = QSharedPointer<TaxonNode>(new TaxonNode("生命", TaxonomicRank::Kingdom));
}

bool TaxonomyRegistry::addTaxonByParts(const QStringList &parts)
{
    if (parts.isEmpty()) return false;

    auto current = m_root;
    TaxonomicRank expectedRank = TaxonomicRank::Kingdom;

    for (const QString& part : parts) {
        if (static_cast<int>(expectedRank) > 7) break; //查看

        bool found = false;
        for (const auto& child : current->getChildren()) {
            if (child->getName() == part) {
                current = child;
                found = true;
                break;
            }
        }

        if (!found) {
            auto newNode = QSharedPointer<TaxonNode>(new TaxonNode(part, expectedRank, current.data()));
            current->addChild(newNode);
            current = newNode;

            // 建立名称索引
            m_nodesByName[part].append(newNode);
        }

        expectedRank = static_cast<TaxonomicRank>(static_cast<int>(expectedRank) + 1);
    }

    emit taxonomyChanged();
    return true;
}

bool TaxonomyRegistry::addTaxon(const QString &fullName, const QString &separator)
{
    QStringList parts = fullName.split(separator, Qt::SkipEmptyParts);
    return addTaxonByParts(parts);
}

QSharedPointer<TaxonNode> TaxonomyRegistry::findNodeByName(const QString &name) const
{
    auto it = m_nodesByName.find(name);
    if (it != m_nodesByName.end() && !it.value().isEmpty())
        return it.value().first();
    return {};
}

QList<QSharedPointer<TaxonNode>> TaxonomyRegistry::findNodesByRank(TaxonomicRank rank, const QString &name) const
{
    QList<QSharedPointer<TaxonNode>> result;
    // 这里可以递归遍历整棵树查找
    std::function<void(const QSharedPointer<TaxonNode>&)> search = [&](const QSharedPointer<TaxonNode>& node) {
        if (node->getRank() == rank && node->getName() == name) {
            result.append(node);
        }
        for (const auto& child : node->getChildren()) {
            search(child);
        }
    };
    search(m_root);
    return result;
}

bool TaxonomyRegistry::exportToJson(const QString &filename) const
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;

    QJsonDocument doc(m_root->toJson());
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

bool TaxonomyRegistry::importFromJson(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;

    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return false;

    auto newRoot = TaxonNode::fromJson(doc.object(), nullptr);
    if (newRoot) {
        m_root = newRoot;
        // 重建 m_nodesByName 索引...
        m_nodesByName.clear();
        std::function<void(const QSharedPointer<TaxonNode>&)> buildIndex = [&](const QSharedPointer<TaxonNode>& node) {
            m_nodesByName[node->getName()].append(node);
            for (const auto& child : node->getChildren()) {
                buildIndex(child);
            }
        };
        buildIndex(m_root);

        emit taxonomyChanged();
        return true;
    }
    return false;
}
