#pragma once

#include <QJsonObject>
#include <QString>

/*
 * 数据库 schema 版本与链式迁移。
 *
 * 每个迁移函数只负责把一个版本升级到“紧邻的下一个版本”，例如：
 *   1 → 2：旧字段 version 统一为 schema_version
 *   2 → 3：补齐节点 children 等结构保证
 * migrateToLatest() 会按顺序执行所有步骤，直到当前最新版本。
 */
namespace DbVersion {

constexpr int kCurrentSchemaVersion = 3;

// 读取文件声明的格式版本；兼容旧字段 version 与新字段 schema_version。
// 无法识别时返回 0。
int schemaVersionOf(const QJsonObject& db);

QJsonObject migrateV1ToV2(const QJsonObject& db);
QJsonObject migrateV2ToV3(const QJsonObject& db);

// 把 db 原地升级到当前版本；成功返回 true。
bool migrateToLatest(QJsonObject& db, QString* error = nullptr);

} // namespace DbVersion
