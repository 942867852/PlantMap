#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryFile>

#include "dbversion.h"
#include "taxondocument.h"

namespace {

int failures = 0;

void check(bool condition, const QString& name)
{
    if (condition) {
        qInfo().noquote() << "[PASS]" << name;
    } else {
        ++failures;
        qInfo().noquote() << "[FAIL]" << name;
    }
}

int appendChain(TaxonomyDocument& doc, const QStringList& names, QString* error)
{
    int parentId = 0;
    int lastId = 0;
    for (const QString& name : names) {
        lastId = doc.addNode(parentId, name, error);
        if (lastId <= 0)
            return 0;
        parentId = lastId;
    }
    return lastId;
}

QJsonObject legacyV1Database()
{
    QJsonObject root;
    root[QStringLiteral("app")] = QStringLiteral("PlantMap");
    root[QStringLiteral("version")] = 1;

    QJsonObject kingdom;
    kingdom[QStringLiteral("id")] = 1;
    kingdom[QStringLiteral("rank")] = QStringLiteral("kingdom");
    kingdom[QStringLiteral("name")] = QStringLiteral("植物界");
    // 旧版本故意不写 children，验证 v2→v3 会补全。

    QJsonObject taxonomy;
    taxonomy[QStringLiteral("next_id")] = 5;
    taxonomy[QStringLiteral("roots")] = QJsonArray { kingdom };
    root[QStringLiteral("taxonomy")] = taxonomy;
    return root;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    TaxonomyDocument doc;
    QString error;

    // 1. 逐级建立完整分类链
    QStringList chain = { QStringLiteral("植物界"), QStringLiteral("被子植物门"),
                          QStringLiteral("木兰纲"), QStringLiteral("蔷薇目"),
                          QStringLiteral("蔷薇科"), QStringLiteral("蔷薇属"),
                          QStringLiteral("玫瑰") };
    const int speciesId = appendChain(doc, chain, &error);
    check(speciesId > 0, QStringLiteral("完整链逐级添加：%1").arg(
                             error.isEmpty() ? QStringLiteral("OK") : error));

    const TaxonNode* species = doc.node(speciesId);
    check(species && species->rank == TaxonRank::Species,
          QStringLiteral("玫瑰节点等级应为“种”"));

    // 2. 等级规则：同级下不能重名
    const int duplicateSpecies = doc.addNode(doc.node(speciesId)->parentId,
                                             QStringLiteral("玫瑰"), &error);
    check(duplicateSpecies == 0, QStringLiteral("同一属下拒绝重名“玫瑰”：%1").arg(error));

    // 3. 亚种可以挂在种下面
    const int subspeciesId = doc.addNode(speciesId, QStringLiteral("重瓣玫瑰"), &error);
    check(subspeciesId > 0 && doc.node(subspeciesId)->rank == TaxonRank::Subspecies,
          QStringLiteral("种下可添加亚种"));

    // 4. 种下不能再加普通级别（亚种是末级）
    const int invalidChild = doc.addNode(subspeciesId, QStringLiteral("再下级"), &error);
    check(invalidChild == 0, QStringLiteral("亚种下禁止再添加下级"));

    // 5. 资料挂载等级限制
    SpeciesInfo genusInfo;
    genusInfo.scientificName = QStringLiteral("Rosa spp.");
    check(!doc.setInfo(species->parentId, genusInfo, &error),
          QStringLiteral("“属”节点不允许挂资料"));

    // 6. 拉丁学名唯一：同库第二个相同学名必须失败
    SpeciesInfo validInfo;
    validInfo.scientificName = QStringLiteral("Rosa rugosa");
    validInfo.description = QStringLiteral("海边常见蔷薇");
    check(doc.setInfo(speciesId, validInfo, &error),
          QStringLiteral("第一次保存 Rosa rugosa 成功"));

    SpeciesInfo invalidInfo;
    invalidInfo.scientificName = QStringLiteral("  rosa.rugosa  ");
    check(!doc.setInfo(subspeciesId, invalidInfo, &error),
          QStringLiteral("忽略大小写/标点后重复的学名被拒绝：%1").arg(error));

    // 7. 不同学名可以并存
    SpeciesInfo subspeciesInfo;
    subspeciesInfo.scientificName = QStringLiteral("Rosa rugosa subsp. alba");
    check(doc.setInfo(subspeciesId, subspeciesInfo, &error),
          QStringLiteral("亚种使用独立学名成功"));

    // 8. 空/重复资料校验
    SpeciesInfo emptyInfo;
    check(!doc.setInfo(subspeciesId, emptyInfo, &error),
          QStringLiteral("空拉丁学名被拒绝"));

    // 9. 哈希索引可反查
    check(doc.findNodeByScientificName(QStringLiteral("rosa rugosa")) == speciesId,
          QStringLiteral("哈希索引可查回 Rosa rugosa 所在节点"));

    // 10. JSON 往返一致性
    const QJsonObject snapshot = doc.toJson();
    TaxonomyDocument loaded;
    check(loaded.loadFromJson(snapshot, &error),
          QStringLiteral("JSON 载入成功"));
    check(loaded.nodeCount() == doc.nodeCount(),
          QStringLiteral("节点数一致：%1 == %2")
              .arg(loaded.nodeCount()).arg(doc.nodeCount()));
    check(loaded.infoCount() == 2,
          QStringLiteral("两个物种资料往返后仍在"));
    check(loaded.findNodeByScientificName(QStringLiteral("Rosa rugosa subsp. alba"))
              == subspeciesId,
          QStringLiteral("往返后哈希索引仍然正确"));

    // 11. 级联删除子树
    const int genusId = doc.node(speciesId)->parentId;
    check(doc.removeNode(genusId, &error),
          QStringLiteral("删除“蔷薇属”整棵子树"));
    check(doc.node(speciesId) == nullptr && doc.node(subspeciesId) == nullptr,
          QStringLiteral("种与亚种被级联删除"));
    check(doc.findNodeByScientificName(QStringLiteral("rosa rugosa")) == 0,
          QStringLiteral("删除后拉丁名索引同步清理"));

    // 12. schema 版本识别
    const QJsonObject v1 = legacyV1Database();
    check(DbVersion::schemaVersionOf(v1) == 1,
          QStringLiteral("旧版 version=1 可被识别"));

    // 13. 1 → 2 迁移
    const QJsonObject v2 = DbVersion::migrateV1ToV2(v1);
    check(DbVersion::schemaVersionOf(v2) == 2
              && !v2.contains(QStringLiteral("version")),
          QStringLiteral("1 → 2：version 转为 schema_version"));

    // 14. 2 → 3 迁移
    const QJsonObject v3 = DbVersion::migrateV2ToV3(v2);
    const QJsonObject migratedRoot = v3.value(QStringLiteral("taxonomy"))
                                         .toObject()
                                         .value(QStringLiteral("roots"))
                                         .toArray()
                                         .at(0)
                                         .toObject();
    check(DbVersion::schemaVersionOf(v3) == 3
              && migratedRoot.contains(QStringLiteral("children")),
          QStringLiteral("2 → 3：版本号推进并补齐 children"));

    // 15. 链式迁移 1 → 2 → 3
    QJsonObject chainDb = v1;
    check(DbVersion::migrateToLatest(chainDb, &error)
              && DbVersion::schemaVersionOf(chainDb) == 3,
          QStringLiteral("1 → 3 链式迁移成功：%1").arg(error));

    // 16. loadFromFile 先迁移再解析
    QTemporaryFile file(QDir::temp().filePath(
        QStringLiteral("plantmap_schema_XXXXXX.json")));
    check(file.open(), QStringLiteral("创建临时旧版数据库文件"));
    file.write(QJsonDocument(v1).toJson(QJsonDocument::Compact));
    file.flush();
    const QString fileName = file.fileName();
    file.close();

    TaxonomyDocument legacyDoc;
    check(legacyDoc.loadFromFile(fileName, &error),
          QStringLiteral("旧版文件经迁移后可加载：%1").arg(error));
    check(legacyDoc.nodeCount() == 1,
          QStringLiteral("迁移加载后节点数正确"));

    qInfo() << (failures == 0 ? QStringLiteral("全部自检通过")
                              : QStringLiteral("存在 %1 项失败").arg(failures));
    return failures == 0 ? 0 : 1;
}
