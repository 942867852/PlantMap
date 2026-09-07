#include "dbversion.h"

#include <QJsonArray>
#include <QJsonValue>

namespace {

const QString kAppKey = QStringLiteral("app");
const QString kVersionKey = QStringLiteral("version");
const QString kSchemaVersionKey = QStringLiteral("schema_version");
const QString kTaxonomyKey = QStringLiteral("taxonomy");
const QString kNextIdKey = QStringLiteral("next_id");
const QString kRootsKey = QStringLiteral("roots");
const QString kChildrenKey = QStringLiteral("children");
const QString kInfoKey = QStringLiteral("info");
const QString kNativeRegionsKey = QStringLiteral("native_regions");
const QString kHabitatKey = QStringLiteral("habitat");

void ensureNodeChildren(QJsonObject& node)
{
    if (!node.contains(kChildrenKey)
        || !node.value(kChildrenKey).isArray()) {
        node.insert(kChildrenKey, QJsonArray());
        return;
    }

    QJsonArray children = node.value(kChildrenKey).toArray();
    for (int i = 0; i < children.size(); ++i) {
        QJsonObject child = children.at(i).toObject();
        if (!child.isEmpty())
            ensureNodeChildren(child);
        children.replace(i, child);
    }
    node.insert(kChildrenKey, children);
}

void normalizeTaxonomy(QJsonObject& db)
{
    QJsonObject taxonomy = db.value(kTaxonomyKey).toObject();
    if (taxonomy.isEmpty())
        return;

    if (!taxonomy.contains(kNextIdKey)
        || taxonomy.value(kNextIdKey).toInt() < 1) {
        taxonomy.insert(kNextIdKey, 1);
    }

    const QJsonArray oldRoots = taxonomy.value(kRootsKey).toArray();
    QJsonArray roots;
    for (int i = 0; i < oldRoots.size(); ++i) {
        QJsonObject node = oldRoots.at(i).toObject();
        if (!node.isEmpty())
            ensureNodeChildren(node);
        roots.append(node);
    }
    taxonomy.insert(kRootsKey, roots);
    db.insert(kTaxonomyKey, taxonomy);
}

// v4 新增字段：为所有节点的 info 补上 native_regions / habitat 默认值，
// 使迁移结果与“直接按 v4 格式保存”一致。旧文件缺这些字段时等价于“未填写”。
void ensureV4InfoFields(QJsonObject& node)
{
    if (node.contains(kInfoKey)) {
        QJsonObject info = node.value(kInfoKey).toObject();
        if (!info.contains(kNativeRegionsKey))
            info.insert(kNativeRegionsKey, QJsonArray());
        if (!info.contains(kHabitatKey))
            info.insert(kHabitatKey, QString());
        node.insert(kInfoKey, info);
    }

    const QJsonArray children = node.value(kChildrenKey).toArray();
    QJsonArray updated;
    for (const auto& value : children) {
        QJsonObject child = value.toObject();
        if (!child.isEmpty())
            ensureV4InfoFields(child);
        updated.append(child);
    }
    node.insert(kChildrenKey, updated);
}

} // namespace

namespace DbVersion {

int schemaVersionOf(const QJsonObject& db)
{
    const QJsonValue schema = db.value(kSchemaVersionKey);
    if (schema.isDouble())
        return schema.toInt();
    const QJsonValue legacy = db.value(kVersionKey);
    if (legacy.isDouble())
        return legacy.toInt();
    return 0;
}

QJsonObject migrateV1ToV2(const QJsonObject& db)
{
    QJsonObject upgraded = db;
    upgraded.remove(kVersionKey);
    upgraded.insert(kSchemaVersionKey, 2);
    return upgraded;
}

QJsonObject migrateV2ToV3(const QJsonObject& db)
{
    QJsonObject upgraded = db;
    upgraded.remove(kVersionKey);
    upgraded.insert(kSchemaVersionKey, 3);
    normalizeTaxonomy(upgraded);
    return upgraded;
}

QJsonObject migrateV3ToV4(const QJsonObject& db)
{
    QJsonObject upgraded = db;
    upgraded.remove(kVersionKey);
    upgraded.insert(kSchemaVersionKey, 4);

    // 为所有节点的 info 补上 v4 新增的 native_regions / habitat 默认值。
    QJsonObject taxonomy = upgraded.value(kTaxonomyKey).toObject();
    const QJsonArray oldRoots = taxonomy.value(kRootsKey).toArray();
    QJsonArray roots;
    for (const auto& value : oldRoots) {
        QJsonObject node = value.toObject();
        if (!node.isEmpty())
            ensureV4InfoFields(node);
        roots.append(node);
    }
    taxonomy.insert(kRootsKey, roots);
    upgraded.insert(kTaxonomyKey, taxonomy);
    return upgraded;
}

bool migrateToLatest(QJsonObject& db, QString* error)
{
    // 先确认是 PlantMap 数据文件再动手迁移，避免无意义地改写外来 JSON。
    const QString appName = db.value(kAppKey).toString();
    if (appName != QLatin1String("PlantMap")) {
        if (error) {
            *error = QStringLiteral("不是 PlantMap 格式的数据文件（app 不匹配）。");
        }
        return false;
    }

    const int startVersion = schemaVersionOf(db);
    if (startVersion <= 0) {
        if (error) {
            *error = QStringLiteral(
                "无法识别数据库格式版本（缺少 version/schema_version 字段）。");
        }
        return false;
    }
    if (startVersion > kCurrentSchemaVersion) {
        if (error) {
            *error = QStringLiteral(
                "数据库格式版本（%1）高于当前软件支持版本（%2），请升级软件。")
                         .arg(startVersion)
                         .arg(kCurrentSchemaVersion);
        }
        return false;
    }

    int version = startVersion;
    while (version < kCurrentSchemaVersion) {
        QJsonObject upgraded;
        if (version == 1) {
            upgraded = migrateV1ToV2(db);
        } else if (version == 2) {
            upgraded = migrateV2ToV3(db);
        } else if (version == 3) {
            upgraded = migrateV3ToV4(db);
        } else {
            if (error) {
                *error = QStringLiteral(
                    "缺少从版本 %1 开始的迁移规则。").arg(version);
            }
            return false;
        }

        const int nextVersion = schemaVersionOf(upgraded);
        if (nextVersion <= version) {
            if (error) {
                *error = QStringLiteral(
                    "迁移规则没有正确推进版本号（%1 → %2）。")
                             .arg(version)
                             .arg(nextVersion);
            }
            return false;
        }
        db = upgraded;
        version = nextVersion;
    }
    return true;
}

} // namespace DbVersion
