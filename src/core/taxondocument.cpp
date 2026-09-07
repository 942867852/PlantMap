#include "taxondocument.h"
#include "dbversion.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>

#include <functional>

namespace {

QString trimCollapse(const QString& input)
{
    QStringList parts = input.trimmed().split(QRegularExpression(QStringLiteral("\\s+")),
                                               Qt::SkipEmptyParts);
    return parts.join(QLatin1Char(' '));
}

} // namespace

// ------------------------- 基础操作 -------------------------

void TaxonomyDocument::clear()
{
    m_nodes.clear();
    m_rootIds.clear();
    m_latinIndex.clear();
    m_nextId = 1;
}

bool TaxonomyDocument::isEmpty() const
{
    return m_nodes.isEmpty();
}

int TaxonomyDocument::nodeCount() const
{
    return m_nodes.size();
}

int TaxonomyDocument::infoCount() const
{
    int count = 0;
    for (auto it = m_nodes.constBegin(); it != m_nodes.constEnd(); ++it) {
        if (it.value()->hasInfo)
            ++count;
    }
    return count;
}

const TaxonNode* TaxonomyDocument::node(int id) const
{
    auto it = m_nodes.constFind(id);
    return it == m_nodes.constEnd() ? nullptr : it.value().data();
}

TaxonNode* TaxonomyDocument::node(int id)
{
    auto it = m_nodes.find(id);
    return it == m_nodes.end() ? nullptr : it.value().data();
}

QVector<int> TaxonomyDocument::roots() const
{
    return m_rootIds;
}

QVector<int> TaxonomyDocument::childIdsOf(int id) const
{
    const TaxonNode* n = node(id);
    return n ? n->childIds : QVector<int>();
}

const TaxonNode* TaxonomyDocument::parent(int id) const
{
    const TaxonNode* n = node(id);
    return n ? node(n->parentId) : nullptr;
}

QString TaxonomyDocument::displayPathOf(int id) const
{
    QStringList path;
    const TaxonNode* current = node(id);
    while (current) {
        path.prepend(current->name);
        current = node(current->parentId);
    }
    return path.join(QStringLiteral(" > "));
}

bool TaxonomyDocument::hasInfo(int id) const
{
    const TaxonNode* n = node(id);
    return n && n->hasInfo;
}

const SpeciesInfo* TaxonomyDocument::infoOf(int id) const
{
    const TaxonNode* n = node(id);
    return n && n->hasInfo ? &n->info : nullptr;
}

int TaxonomyDocument::findNodeByScientificName(const QString& latinName) const
{
    return m_latinIndex.value(normalizedScientificName(latinName), 0);
}

const TaxonNode* TaxonomyDocument::childByName(int parentId, const QString& name) const
{
    const TaxonNode* parentNode = parentId == 0 ? nullptr : node(parentId);
    if (parentId != 0 && !parentNode)
        return nullptr;

    const QVector<int> ids = parentId == 0 ? m_rootIds : parentNode->childIds;
    for (int childId : ids) {
        const TaxonNode* child = node(childId);
        if (child && child->name == name)
            return child;
    }
    return nullptr;
}

// ------------------------- 分类树编辑 -------------------------

int TaxonomyDocument::addNode(int parentId, const QString& name, QString* error)
{
    return addNode(parentId, name, TaxonRank::Invalid, error);
}

int TaxonomyDocument::addNode(int parentId, const QString& name, TaxonRank rank,
                              QString* error)
{
    const QString cleanName = trimCollapse(name);
    if (cleanName.isEmpty()) {
        if (error) *error = QStringLiteral("名称不能为空。");
        return 0;
    }

    TaxonRank resolvedRank = TaxonRank::Kingdom;
    if (parentId == 0) {
        resolvedRank = TaxonRank::Kingdom;
        if (rank != TaxonRank::Invalid && rank != TaxonRank::Kingdom) {
            if (error) *error = QStringLiteral("顶级分类的等级只能是“界”。");
            return 0;
        }
    } else {
        const TaxonNode* parentNode = node(parentId);
        if (!parentNode) {
            if (error) *error = QStringLiteral("父节点不存在。");
            return 0;
        }
        if (!TaxonRanks::canHaveChildren(parentNode->rank)) {
            if (error) {
                *error = QStringLiteral("“%1”已经是末级（%2），不能继续添加下级。")
                             .arg(parentNode->name, TaxonRanks::displayName(parentNode->rank));
            }
            return 0;
        }

        if (rank == TaxonRank::Invalid) {
            resolvedRank = TaxonRanks::nextLower(parentNode->rank);
        } else {
            // 显式指定等级：种之下允许亚种/变种/变型/品种；其余必须等于自动推导。
            if (parentNode->rank == TaxonRank::Species) {
                if (!TaxonRanks::isSubrankOfSpecies(rank)) {
                    if (error) {
                        *error = QStringLiteral("“种”下只能添加 亚种 / 变种 / 变型 / 品种。");
                    }
                    return 0;
                }
            } else if (rank != TaxonRanks::nextLower(parentNode->rank)) {
                if (error) {
                    *error = QStringLiteral("“%1”的下一级应为“%2”，不能指定为“%3”。")
                                 .arg(parentNode->name,
                                      TaxonRanks::displayName(TaxonRanks::nextLower(parentNode->rank)),
                                      TaxonRanks::displayName(rank));
                }
                return 0;
            }
            resolvedRank = rank;
        }
    }

    if (childByName(parentId, cleanName)) {
        if (error) {
            const QString where = parentId == 0
                ? QStringLiteral("顶级分类")
                : QStringLiteral("“%1”下").arg(node(parentId)->name);
            *error = QStringLiteral("在%1已存在同名分类“%2”。").arg(where, cleanName);
        }
        return 0;
    }

    auto newNode = QSharedPointer<TaxonNode>::create();
    newNode->id = m_nextId++;
    newNode->parentId = parentId;
    newNode->rank = resolvedRank;
    newNode->name = cleanName;

    if (parentId == 0) {
        m_rootIds.append(newNode->id);
    } else {
        TaxonNode* parentNode = node(parentId);
        parentNode->childIds.append(newNode->id);
    }
    m_nodes.insert(newNode->id, newNode);
    return newNode->id;
}

void TaxonomyDocument::collectSubtreeIds(int id, QVector<int>& ids) const
{
    ids.append(id);
    const TaxonNode* n = node(id);
    if (!n)
        return;
    for (int childId : n->childIds)
        collectSubtreeIds(childId, ids);
}

bool TaxonomyDocument::removeNode(int id, QString* error)
{
    const TaxonNode* n = node(id);
    if (!n) {
        if (error) *error = QStringLiteral("要删除的节点不存在。");
        return false;
    }

    QVector<int> idsToRemove;
    collectSubtreeIds(id, idsToRemove);

    for (int removeId : idsToRemove) {
        const TaxonNode* doomed = node(removeId);
        if (doomed && doomed->hasInfo)
            removeLatinForNode(removeId);
    }

    if (n->parentId == 0) {
        m_rootIds.removeAll(id);
    } else {
        TaxonNode* parentNode = node(n->parentId);
        if (parentNode)
            parentNode->childIds.removeAll(id);
    }

    for (int removeId : idsToRemove)
        m_nodes.remove(removeId);
    return true;
}

bool TaxonomyDocument::renameNode(int id, const QString& newName, QString* error)
{
    TaxonNode* n = node(id);
    if (!n) {
        if (error) *error = QStringLiteral("节点不存在。");
        return false;
    }
    const QString cleanName = trimCollapse(newName);
    if (cleanName.isEmpty()) {
        if (error) *error = QStringLiteral("名称不能为空。");
        return false;
    }

    const TaxonNode* sibling = childByName(n->parentId, cleanName);
    if (sibling && sibling->id != id) {
        if (error) *error = QStringLiteral("同级已存在同名节点“%1”。").arg(cleanName);
        return false;
    }
    n->name = cleanName;
    return true;
}

int TaxonomyDocument::cloneSubtree(int srcId, int dstParentId,
                                   const QString& newRootName, QString* error)
{
    const TaxonNode* src = node(srcId);
    if (!src) {
        if (error) *error = QStringLiteral("要复制的节点不存在。");
        return 0;
    }

    if (dstParentId != 0) {
        const TaxonNode* dstParent = node(dstParentId);
        if (!dstParent) {
            if (error) *error = QStringLiteral("目标父节点不存在。");
            return 0;
        }
        if (!TaxonRanks::canHaveChildren(dstParent->rank)) {
            if (error) {
                *error = QStringLiteral("“%1（%2）”已经是最末级，不能在其下添加副本。")
                             .arg(dstParent->name, TaxonRanks::displayName(dstParent->rank));
            }
            return 0;
        }
    }

    const QString cleanRoot = trimCollapse(newRootName);
    if (cleanRoot.isEmpty()) {
        if (error) *error = QStringLiteral("副本名称不能为空。");
        return 0;
    }
    if (childByName(dstParentId, cleanRoot)) {
        if (error) {
            *error = QStringLiteral("已存在同名节点“%1”，请换一个副本名称。").arg(cleanRoot);
        }
        return 0;
    }

    // 先生成拉丁名后缀方案，避免与现有索引冲突（原件仍在，故原名必冲突）。
    auto uniqueLatin = [&](const QString& base) {
        for (int n = 1; ; ++n) {
            const QString candidate = base + QStringLiteral(" dup%1").arg(n);
            if (findNodeByScientificName(candidate) == 0)
                return candidate;
        }
    };

    // 先序遍历 src 子树，按顺序克隆（父节点先于子节点插入）。
    QVector<int> order;
    std::function<void(int)> collect = [&](int id) {
        order.append(id);
        const TaxonNode* n = node(id);
        for (int childId : n->childIds)
            collect(childId);
    };
    collect(srcId);

    QHash<int, int> newIdOf;
    int newRootId = 0;
    for (int sid : order) {
        const TaxonNode* sn = node(sid);
        if (!sn)
            continue;

        auto cloned = QSharedPointer<TaxonNode>::create();
        cloned->id = m_nextId++;
        cloned->parentId = (sid == srcId) ? dstParentId
                                          : newIdOf.value(sn->parentId, 0);
        cloned->rank = sn->rank;
        cloned->name = (sid == srcId) ? cleanRoot : sn->name;
        cloned->hasInfo = sn->hasInfo;
        if (sn->hasInfo) {
            cloned->info = sn->info;
            if (cloned->info.hasScientificName())
                cloned->info.scientificName =
                    uniqueLatin(cloned->info.scientificName);
        }

        if (cloned->parentId == 0)
            m_rootIds.append(cloned->id);
        else
            node(cloned->parentId)->childIds.append(cloned->id);
        m_nodes.insert(cloned->id, cloned);
        if (cloned->hasInfo && cloned->info.hasScientificName())
            addLatinToIndex(cloned->info.scientificName, cloned->id);

        newIdOf.insert(sid, cloned->id);
        if (sid == srcId)
            newRootId = cloned->id;
    }
    return newRootId;
}

int TaxonomyDocument::restoreSubtreeFromJson(const QJsonObject& obj,
                                             int parentId, QString* error)
{
    std::function<int(const QJsonObject&, int)> restore =
        [&](const QJsonObject& o, int pid) -> int {
        auto node = QSharedPointer<TaxonNode>::create();
        node->id = m_nextId++;
        node->parentId = pid;
        node->rank = TaxonRanks::fromKey(o.value(QStringLiteral("rank")).toString());
        node->name = o.value(QStringLiteral("name")).toString().trimmed();

        const QJsonObject infoObj = o.value(QStringLiteral("info")).toObject();
        if (!infoObj.isEmpty()) {
            node->info = SpeciesInfo::fromJson(infoObj);
            node->hasInfo = !node->info.isEmpty();
        }

        if (pid == 0)
            m_rootIds.append(node->id);
        else
            this->node(pid)->childIds.append(node->id);
        m_nodes.insert(node->id, node);

        if (node->hasInfo && node->info.hasScientificName()) {
            // 拉丁学名冲突时加后缀，保证恢复不因唯一性失败。
            if (!addLatinToIndex(node->info.scientificName, node->id)) {
                QString base = node->info.scientificName;
                for (int n = 1; ; ++n) {
                    const QString cand = base + QStringLiteral(" dup%1").arg(n);
                    if (addLatinToIndex(cand, node->id)) {
                        node->info.scientificName = cand;
                        break;
                    }
                }
            }
        }

        const QJsonArray children = o.value(QStringLiteral("children")).toArray();
        for (const auto& child : children)
            node->childIds.append(restore(child.toObject(), node->id));
        return node->id;
    };

    if (error) *error = QString();
    return restore(obj, parentId);
}

// ------------------------- 拉丁名索引 -------------------------

QString TaxonomyDocument::normalizedScientificName(const QString& latinName)
{
    QString normalized;
    const QString lowered = latinName.trimmed().toLower();
    for (const QChar& ch : lowered) {
        if (ch.isLetterOrNumber())
            normalized.append(ch);
    }
    return normalized;
}

void TaxonomyDocument::removeLatinForNode(int nodeId)
{
    auto it = m_latinIndex.begin();
    while (it != m_latinIndex.end()) {
        if (it.value() == nodeId)
            it = m_latinIndex.erase(it);
        else
            ++it;
    }
}

bool TaxonomyDocument::addLatinToIndex(const QString& latinName, int nodeId)
{
    const QString key = normalizedScientificName(latinName);
    if (key.isEmpty())
        return false;
    const auto it = m_latinIndex.constFind(key);
    if (it != m_latinIndex.constEnd() && it.value() != nodeId)
        return false;
    m_latinIndex.insert(key, nodeId);
    return true;
}

bool TaxonomyDocument::setInfo(int id, const SpeciesInfo& info, QString* error)
{
    TaxonNode* n = node(id);
    if (!n) {
        if (error) *error = QStringLiteral("节点不存在。");
        return false;
    }
    if (!TaxonRanks::canHostPlantInfo(n->rank)) {
        if (error) {
            *error = QStringLiteral("“%1”等级的资料只允许挂在“种”或“亚种”节点上。")
                         .arg(TaxonRanks::displayName(n->rank));
        }
        return false;
    }
    if (!info.hasScientificName()) {
        if (error) *error = QStringLiteral("拉丁学名不能为空，它是每个物种的唯一标识。");
        return false;
    }

    // 先把旧索引摘除，再检查新学名，避免“自己跟自己冲突”。
    if (n->hasInfo)
        removeLatinForNode(n->id);

    if (!addLatinToIndex(info.scientificName, n->id)) {
        if (error) {
            const int existingId = findNodeByScientificName(info.scientificName);
            if (existingId > 0) {
                *error = QStringLiteral("拉丁学名“%1”已存在，位于：%2")
                             .arg(info.scientificName.trimmed(),
                                  displayPathOf(existingId));
            } else {
                *error = QStringLiteral("拉丁学名“%1”规范化后为空。")
                             .arg(info.scientificName.trimmed());
            }
        }
        // 恢复旧索引
        if (n->hasInfo)
            addLatinToIndex(n->info.scientificName, n->id);
        return false;
    }

    n->info = info;
    n->hasInfo = true;
    return true;
}

bool TaxonomyDocument::clearInfo(int id, QString* error)
{
    TaxonNode* n = node(id);
    if (!n) {
        if (error) *error = QStringLiteral("节点不存在。");
        return false;
    }
    if (n->hasInfo) {
        removeLatinForNode(id);
        n->info = SpeciesInfo();
        n->hasInfo = false;
    }
    return true;
}

// ------------------------- JSON 序列化 -------------------------

QJsonObject TaxonomyDocument::nodeToJson(const TaxonNode& nodeToWrite) const
{
    QJsonObject obj;
    obj[QStringLiteral("id")] = nodeToWrite.id;
    obj[QStringLiteral("rank")] = TaxonRanks::toKey(nodeToWrite.rank);
    obj[QStringLiteral("name")] = nodeToWrite.name;
    if (nodeToWrite.hasInfo)
        obj[QStringLiteral("info")] = nodeToWrite.info.toJson();

    QJsonArray children;
    for (int childId : nodeToWrite.childIds) {
        const TaxonNode* child = node(childId);
        if (child)
            children.append(nodeToJson(*child));
    }
    obj[QStringLiteral("children")] = children;
    return obj;
}

QJsonObject TaxonomyDocument::toJson() const
{
    QJsonArray roots;
    for (int id : m_rootIds) {
        const TaxonNode* n = node(id);
        if (n)
            roots.append(nodeToJson(*n));
    }

    QJsonObject taxonomy;
    taxonomy[QStringLiteral("next_id")] = m_nextId;
    taxonomy[QStringLiteral("roots")] = roots;

    QJsonObject root;
    root[QStringLiteral("app")] = QStringLiteral("PlantMap");
    root[QStringLiteral("schema_version")] =
        DbVersion::kCurrentSchemaVersion;
    root[QStringLiteral("taxonomy")] = taxonomy;
    return root;
}

QJsonObject TaxonomyDocument::subtreeToJson(int id) const
{
    const TaxonNode* n = node(id);
    return n ? nodeToJson(*n) : QJsonObject();
}

int TaxonomyDocument::parseNodeJson(const QJsonObject& obj, int parentId,
                                    int& nextId, QSet<int>& seenIds, int depth,
                                    QString& error)
{
    // 合法分类最多 8 级（界→亚种）。解析先于层级校验执行，
    // 这里直接拒绝病态深层嵌套，防止手工构造的 JSON 造成栈溢出。
    constexpr int kMaxParseDepth = 16;
    if (depth > kMaxParseDepth) {
        error = QStringLiteral("数据文件层级嵌套过深（超过 %1 层），文件可能已损坏。")
                    .arg(kMaxParseDepth);
        return 0;
    }

    const QString name = obj.value(QStringLiteral("name")).toString().trimmed();
    const TaxonRank rank = TaxonRanks::fromKey(
        obj.value(QStringLiteral("rank")).toString());
    if (name.isEmpty() || !TaxonRanks::isValid(rank)) {
        error = QStringLiteral("JSON 中存在无效分类节点（name/rank 缺失）。");
        return 0;
    }

    int id = obj.value(QStringLiteral("id")).toInt();
    if (id <= 0) {
        // 节点省略或非法 id 时，从 nextId 起寻找一个未被占用的 id。
        // 正常程序保存的文件 id 严格连续且唯一，此分支仅在手工编辑
        // JSON 等异常情况下触发；循环保证兜底 id 不与任何显式 id 冲突。
        while (seenIds.contains(nextId))
            ++nextId;
        id = nextId;
    }
    if (seenIds.contains(id)) {
        error = QStringLiteral("JSON 中节点 id=%1 重复。").arg(id);
        return 0;
    }
    seenIds.insert(id);
    if (id >= nextId)
        nextId = id + 1;

    auto newNode = QSharedPointer<TaxonNode>::create();
    newNode->id = id;
    newNode->parentId = parentId;
    newNode->rank = rank;
    newNode->name = name;

    m_nodes.insert(newNode->id, newNode);

    const QJsonObject infoObj = obj.value(QStringLiteral("info")).toObject();
    if (!infoObj.isEmpty()) {
        newNode->info = SpeciesInfo::fromJson(infoObj);
        newNode->hasInfo = !newNode->info.isEmpty();
    }

    const QJsonArray childrenArray = obj.value(QStringLiteral("children")).toArray();
    for (const auto& value : childrenArray) {
        const int childId = parseNodeJson(value.toObject(), id, nextId, seenIds,
                                          depth + 1, error);
        if (childId == 0)
            return 0;
        newNode->childIds.append(childId);
    }

    if (parentId == 0)
        m_rootIds.append(newNode->id);
    return newNode->id;
}

bool TaxonomyDocument::validateLoadedHierarchy(QString& error) const
{
    QSet<QString> rootNames;
    for (int id : m_rootIds) {
        const TaxonNode* n = node(id);
        if (!n || n->rank != TaxonRank::Kingdom) {
            error = QStringLiteral("数据文件中顶级分类不是“界”（等级：%1）。")
                        .arg(n ? TaxonRanks::displayName(n->rank)
                               : QStringLiteral("缺失"));
            return false;
        }
        if (rootNames.contains(n->name)) {
            error = QStringLiteral("数据文件中存在重复的顶级分类“%1”。")
                        .arg(n->name);
            return false;
        }
        rootNames.insert(n->name);
    }

    std::function<bool(int)> validateNode = [&](int id) -> bool {
        const TaxonNode* n = node(id);
        if (!n) {
            error = QStringLiteral("数据文件中存在悬空节点引用（id=%1）。").arg(id);
            return false;
        }

        if (n->parentId != 0) {
            const TaxonNode* parentNode = node(n->parentId);
            bool rankOk = false;
            if (parentNode && parentNode->rank == TaxonRank::Species) {
                // 种之下允许并列的 亚种/变种/变型/品种。
                rankOk = TaxonRanks::isSubrankOfSpecies(n->rank);
            } else if (parentNode) {
                rankOk = n->rank == TaxonRanks::nextLower(parentNode->rank);
            }
            if (!rankOk) {
                error = QStringLiteral(
                            "数据文件层级错误：“%1（%2）”下的节点“%3（%4）”等级不合法。")
                            .arg(parentNode ? parentNode->name : QString(),
                                 parentNode ? TaxonRanks::displayName(parentNode->rank)
                                            : QStringLiteral("?"),
                                 n->name,
                                 TaxonRanks::displayName(n->rank));
                return false;
            }
        }

        if (n->hasInfo && !TaxonRanks::canHostPlantInfo(n->rank)) {
            error = QStringLiteral("数据文件中“%1（%2）”不允许挂载物种资料。")
                        .arg(n->name, TaxonRanks::displayName(n->rank));
            return false;
        }

        QSet<QString> childNames;
        for (int childId : n->childIds) {
            const TaxonNode* child = node(childId);
            if (!child) {
                error = QStringLiteral("数据文件中节点“%1”存在悬空子节点（id=%2）。")
                            .arg(n->name).arg(childId);
                return false;
            }
            if (childNames.contains(child->name)) {
                error = QStringLiteral("数据文件中“%1”下存在同名分类“%2”。")
                            .arg(n->name, child->name);
                return false;
            }
            childNames.insert(child->name);
        }

        for (int childId : n->childIds) {
            if (!validateNode(childId))
                return false;
        }
        return true;
    };

    for (int id : m_rootIds) {
        if (!validateNode(id))
            return false;
    }
    return true;
}

bool TaxonomyDocument::loadFromJson(const QJsonObject& root, QString* error)
{
    const QString appName = root.value(QStringLiteral("app")).toString();
    const int schemaVersion = DbVersion::schemaVersionOf(root);
    if (appName != QLatin1String("PlantMap")
        || schemaVersion != DbVersion::kCurrentSchemaVersion) {
        if (error) {
            *error = QStringLiteral(
                "不是 PlantMap 格式的数据文件"
                "（app/schema_version 不匹配，当前需要版本 %1）。")
                         .arg(DbVersion::kCurrentSchemaVersion);
        }
        return false;
    }

    const QJsonObject taxonomy = root.value(QStringLiteral("taxonomy")).toObject();
    if (taxonomy.isEmpty()) {
        if (error) *error = QStringLiteral("数据文件中缺少 taxonomy 字段。");
        return false;
    }

    clear();
    m_nextId = taxonomy.value(QStringLiteral("next_id")).toInt(1);
    if (m_nextId < 1)
        m_nextId = 1;

    QSet<int> seenIds;
    QString loadError;
    const QJsonArray rootsArray = taxonomy.value(QStringLiteral("roots")).toArray();
    for (const auto& value : rootsArray) {
        const int id = parseNodeJson(value.toObject(), 0, m_nextId, seenIds, 0,
                                     loadError);
        if (id == 0) {
            if (error) *error = loadError;
            clear();
            return false;
        }
    }

    QString hierarchyError;
    if (!validateLoadedHierarchy(hierarchyError)) {
        if (error) *error = hierarchyError;
        clear();
        return false;
    }

    // 重建拉丁学名索引；发现非法/重复学名时加载失败，提示用户手工清理数据文件。
    // 注意区分两种失败原因：“规范化后为空”与“与其他节点重复”，避免误导。
    QStringList latinErrors;
    for (auto it = m_nodes.constBegin(); it != m_nodes.constEnd(); ++it) {
        const TaxonNode* n = it.value().data();
        if (!n->hasInfo || !n->info.hasScientificName())
            continue;
        if (normalizedScientificName(n->info.scientificName).isEmpty()) {
            latinErrors.append(QStringLiteral("“%1”规范化后为空（位于 %2）")
                                   .arg(n->info.scientificName, displayPathOf(n->id)));
            continue;
        }
        if (!addLatinToIndex(n->info.scientificName, n->id)) {
            latinErrors.append(QStringLiteral("“%1”与其他节点重复（位于 %2）")
                                   .arg(n->info.scientificName, displayPathOf(n->id)));
        }
    }
    if (!latinErrors.isEmpty()) {
        if (error) {
            *error = QStringLiteral("数据文件中存在非法或重复的拉丁学名：%1")
                         .arg(latinErrors.join(QStringLiteral("；")));
        }
        clear();
        return false;
    }
    return true;
}

bool TaxonomyDocument::saveToFile(const QString& fileName, QString* error) const
{
    const QFileInfo info(fileName);
    if (!info.absoluteDir().exists() && !QDir().mkpath(info.absolutePath())) {
        if (error) *error = QStringLiteral("无法创建数据目录：%1").arg(info.absolutePath());
        return false;
    }

    QSaveFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = QStringLiteral("无法打开数据文件写入：%1").arg(fileName);
        return false;
    }
    file.write(QJsonDocument(toJson()).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error) *error = QStringLiteral("写入数据文件失败：%1").arg(file.errorString());
        return false;
    }
    return true;
}

bool TaxonomyDocument::loadFromFile(const QString& fileName, QString* error)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = QStringLiteral("无法打开数据文件：%1").arg(file.errorString());
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!doc.isObject()) {
        if (error) *error = QStringLiteral("数据文件不是有效 JSON。");
        return false;
    }

    // 先按 schema 版本链式迁移，再交给 loadFromJson 解析。
    QJsonObject root = doc.object();
    if (!DbVersion::migrateToLatest(root, error))
        return false;
    return loadFromJson(root, error);
}
