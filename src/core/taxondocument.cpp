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
    const QString cleanName = trimCollapse(name);
    if (cleanName.isEmpty()) {
        if (error) *error = QStringLiteral("名称不能为空。");
        return 0;
    }

    TaxonRank rank = TaxonRank::Kingdom;
    if (parentId == 0) {
        rank = TaxonRank::Kingdom;
    } else {
        const TaxonNode* parentNode = node(parentId);
        if (!parentNode) {
            if (error) *error = QStringLiteral("父节点不存在。");
            return 0;
        }
        if (!TaxonRanks::canHaveChildren(parentNode->rank)) {
            if (error) {
                *error = QStringLiteral("“%1”已经是最细等级（%2），不能继续添加下级。")
                             .arg(parentNode->name, TaxonRanks::displayName(parentNode->rank));
            }
            return 0;
        }
        rank = TaxonRanks::nextLower(parentNode->rank);
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
    newNode->rank = rank;
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

int TaxonomyDocument::parseNodeJson(const QJsonObject& obj, int parentId,
                                    int& nextId, QSet<int>& seenIds, QString& error)
{
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
        const int childId = parseNodeJson(value.toObject(), id, nextId, seenIds, error);
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
            const TaxonRank expected =
                parentNode ? TaxonRanks::nextLower(parentNode->rank)
                           : TaxonRank::Invalid;
            if (n->rank != expected) {
                error = QStringLiteral(
                            "数据文件层级错误：“%1（%2）”的下一级应为%3，"
                            "实际保存的是%4。")
                            .arg(parentNode ? parentNode->name : QString(),
                                 parentNode ? TaxonRanks::displayName(parentNode->rank)
                                            : QStringLiteral("?"),
                                 TaxonRanks::displayName(expected),
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
        const int id = parseNodeJson(value.toObject(), 0, m_nextId, seenIds, loadError);
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

    // 重建拉丁学名索引；发现重复时加载失败，提示用户手工清理数据文件。
    QStringList latinErrors;
    for (auto it = m_nodes.constBegin(); it != m_nodes.constEnd(); ++it) {
        const TaxonNode* n = it.value().data();
        if (!n->hasInfo || !n->info.hasScientificName())
            continue;
        if (!addLatinToIndex(n->info.scientificName, n->id)) {
            latinErrors.append(QStringLiteral("“%1”（位于 %2）")
                                   .arg(n->info.scientificName, displayPathOf(n->id)));
        }
    }
    if (!latinErrors.isEmpty()) {
        if (error) {
            *error = QStringLiteral("数据文件中存在重复拉丁学名：%1")
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
