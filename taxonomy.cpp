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
    case TaxonomicRank::Earth:   return QObject::tr("生物圈");
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
    if (str == QObject::tr("生物圈")) return TaxonomicRank::Earth;
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
    // 步骤1：拆分路径
    QStringList parts = fullName.split(separator, Qt::SkipEmptyParts);
    if (parts.isEmpty()) return false;

    // 步骤2：从后往前找第一个已在树中存在的节点
    int startIndex = -1;
    QSharedPointer<TaxonNode> foundNode;

    for (int i = parts.size() - 1; i >= 0; i--) {
        QString name = parts[i];
        auto node = findNodeByName(name); // 全局查找名字为 name 的节点

        if (node) {
            startIndex = i;
            foundNode = node;
            break;
        }
    }

    // 步骤3：如果找到了已有节点，从那里继续建树
    if (foundNode && startIndex < parts.size() - 1) {
        auto current = foundNode;
        TaxonomicRank expectedRank = static_cast<TaxonomicRank>(static_cast<int>(current->getRank()) + 1);

        for (int i = startIndex + 1; i < parts.size(); ++i) {
            bool childExists = false;
            for (const auto& child : current->getChildren()) {
                if (child->getName() == parts[i]) {
                    current = child;
                    expectedRank = static_cast<TaxonomicRank>(static_cast<int>(expectedRank) + 1);
                    childExists = true;
                    break;
                }
            }

            if (!childExists) {
                auto newNode = QSharedPointer<TaxonNode>(
                    new TaxonNode(parts[i], expectedRank, current.data())
                    );
                current->addChild(newNode);
                current = newNode;
            }

            expectedRank = static_cast<TaxonomicRank>(static_cast<int>(expectedRank) + 1);
        }

        emit taxonomyChanged();
        return true;
    }
    // 步骤4：如果完全没找到任何已有节点 → 按原逻辑从根开始创建（或报错）
    else
    {
        // 从根节点开始
        auto current = m_root;  // 假设 m_root 是 “生命” 或 “根”

        TaxonomicRank expectedRank = TaxonomicRank::Kingdom;

        // 第二步：逐级处理每一个部分
        for (const QString& name : parts) {
            bool found = false;

            // 在当前节点的所有子节点中查找是否有同名的
            for (auto child : current->getChildren()) {
                if (child->getName() == name) {
                    current = child;  // 找到了，进入下一级
                    found = true;
                    break;
                }
            }

            // 如果没找到，就创建新节点
            if (!found) {
                auto newNode = QSharedPointer<TaxonNode>(new TaxonNode(name, expectedRank, current.data()));
                current->addChild(newNode);
                current = newNode;
            }

            // 等级往下一级（门→纲→目→科…）
            expectedRank = static_cast<TaxonomicRank>(static_cast<int>(expectedRank) + 1);
        }

        emit taxonomyChanged(); // 告诉界面更新
        return true;
    }




    // 可选：返回 false 提示用户先创建上级分类
    qWarning() << "❌ 无法添加分类：" << fullName
               << "\n原因：没有找到已存在的父级（如'蔷薇属'），请先创建完整路径";
    return false;
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
    std::function<void(const QSharedPointer<TaxonNode>&)> search = [&](const QSharedPointer<TaxonNode>& node)
    {
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
        std::function<void(const QSharedPointer<TaxonNode>&)> buildIndex =[&](const QSharedPointer<TaxonNode>& node)
        {
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
