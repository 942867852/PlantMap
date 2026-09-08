#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryFile>

#include "csvexport.h"
#include "dbversion.h"
#include "displayformat.h"
#include "pinyin.h"
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

QJsonObject makeChainNode(int id, const QString& rank, const QString& name,
                          const QJsonArray& children = QJsonArray())
{
    QJsonObject node;
    node[QStringLiteral("id")] = id;
    node[QStringLiteral("rank")] = rank;
    node[QStringLiteral("name")] = name;
    node[QStringLiteral("children")] = children;
    return node;
}

QJsonObject wrappedDb(const QJsonArray& roots)
{
    QJsonObject taxonomy;
    taxonomy[QStringLiteral("next_id")] = 100;
    taxonomy[QStringLiteral("roots")] = roots;
    QJsonObject db;
    db[QStringLiteral("app")] = QStringLiteral("PlantMap");
    db[QStringLiteral("schema_version")] = DbVersion::kCurrentSchemaVersion;
    db[QStringLiteral("taxonomy")] = taxonomy;
    return db;
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

    // 15. 链式迁移 1 → 2 → 3 → 4
    QJsonObject chainDb = v1;
    check(DbVersion::migrateToLatest(chainDb, &error)
              && DbVersion::schemaVersionOf(chainDb) == 4,
          QStringLiteral("1 → 4 链式迁移成功：%1").arg(error));

    // 15b. 3 → 4 迁移：为 info 补齐 v4 新增字段（native_regions / habitat）
    QJsonObject v3Root = makeChainNode(1, QStringLiteral("kingdom"), QStringLiteral("植物界"));
    QJsonObject v3Info;
    v3Info[QStringLiteral("scientific_name")] = QStringLiteral("Rosa rugosa");
    v3Root[QStringLiteral("info")] = v3Info;
    QJsonObject v3Db;
    v3Db[QStringLiteral("app")] = QStringLiteral("PlantMap");
    v3Db[QStringLiteral("schema_version")] = 3;
    QJsonObject v3Tax;
    v3Tax[QStringLiteral("next_id")] = 2;
    v3Tax[QStringLiteral("roots")] = QJsonArray { v3Root };
    v3Db[QStringLiteral("taxonomy")] = v3Tax;

    const QJsonObject v4 = DbVersion::migrateV3ToV4(v3Db);
    check(DbVersion::schemaVersionOf(v4) == 4,
          QStringLiteral("3 → 4：版本号推进到 4"));
    const QJsonObject migratedInfo = v4.value(QStringLiteral("taxonomy"))
                                         .toObject()
                                         .value(QStringLiteral("roots"))
                                         .toArray().at(0).toObject()
                                         .value(QStringLiteral("info"))
                                         .toObject();
    check(migratedInfo.contains(QStringLiteral("native_regions"))
              && migratedInfo.value(QStringLiteral("native_regions")).toArray().isEmpty(),
          QStringLiteral("3 → 4：info 补上空的 native_regions"));
    check(migratedInfo.contains(QStringLiteral("habitat"))
              && migratedInfo.value(QStringLiteral("habitat")).toString().isEmpty(),
          QStringLiteral("3 → 4：info 补上空的 habitat"));

    // 15c. 迁移后的合法 v4 数据能被当前版本加载
    QJsonObject v3Sp2 = makeChainNode(7, QStringLiteral("species"), QStringLiteral("玫瑰"));
    QJsonObject v3Info2;
    v3Info2[QStringLiteral("scientific_name")] = QStringLiteral("Rosa rugosa");
    v3Sp2[QStringLiteral("info")] = v3Info2;
    QJsonObject v3Db2;
    v3Db2[QStringLiteral("app")] = QStringLiteral("PlantMap");
    v3Db2[QStringLiteral("schema_version")] = 3;
    QJsonObject v3Tax2;
    v3Tax2[QStringLiteral("next_id")] = 8;
    v3Tax2[QStringLiteral("roots")] = QJsonArray {
        makeChainNode(1, QStringLiteral("kingdom"), QStringLiteral("植物界"), {
            makeChainNode(2, QStringLiteral("phylum"), QStringLiteral("被子植物门"), {
                makeChainNode(3, QStringLiteral("class"), QStringLiteral("木兰纲"), {
                    makeChainNode(4, QStringLiteral("order"), QStringLiteral("蔷薇目"), {
                        makeChainNode(5, QStringLiteral("family"), QStringLiteral("蔷薇科"), {
                            makeChainNode(6, QStringLiteral("genus"), QStringLiteral("蔷薇属"),
                                          { v3Sp2 })
                        })
                    })
                })
            })
        })
    };
    v3Db2[QStringLiteral("taxonomy")] = v3Tax2;
    const QJsonObject v4b = DbVersion::migrateV3ToV4(v3Db2);
    TaxonomyDocument v4Doc;
    check(v4Doc.loadFromJson(v4b, &error),
          QStringLiteral("迁移后的合法 v4 数据可加载：%1").arg(error));
    check(v4Doc.infoCount() == 1 && v4Doc.node(7)
              && v4Doc.node(7)->info.scientificName == QStringLiteral("Rosa rugosa"),
          QStringLiteral("迁移后物种资料与拉丁学名保留"));

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

    // 17. 月份区间显示（防止回归）
    check(DisplayFormat::joinMonths(QVector<int>{}) == QStringLiteral("未填写"),
          QStringLiteral("空月份显示“未填写”"));
    check(DisplayFormat::joinMonths(QVector<int>{3}) == QStringLiteral("3月"),
          QStringLiteral("单月显示“3月”"));
    check(DisplayFormat::joinMonths(QVector<int>{2, 3, 4})
              == QStringLiteral("2月-4月"),
          QStringLiteral("连续月份显示“2月-4月”"));
    check(DisplayFormat::joinMonths(QVector<int>{12, 1, 2})
              == QStringLiteral("12月-次年2月"),
          QStringLiteral("跨年月份显示“12月-次年2月”"));
    QVector<int> allMonths;
    for (int month = 1; month <= 12; ++month)
        allMonths.append(month);
    check(DisplayFormat::joinMonths(allMonths) == QStringLiteral("全年"),
          QStringLiteral("全选月份显示“全年”"));
    check(DisplayFormat::joinMonths(QVector<int>{2, 4})
              == QStringLiteral("2月、4月"),
          QStringLiteral("不连续月份逐个显示"));

    // 18. 数值范围单边显示（防止回归）
    check(DisplayFormat::rangeText(-100, -100, QStringLiteral(" ℃"), -100)
              == QStringLiteral("未填写"),
          QStringLiteral("两端未填时不显示温度"));
    check(DisplayFormat::rangeText(-100, 30, QStringLiteral(" ℃"), -100)
              == QStringLiteral("最高 30 ℃"),
          QStringLiteral("只填高端显示“最高 30 ℃”"));
    check(DisplayFormat::rangeText(15, -100, QStringLiteral(" ℃"), -100)
              == QStringLiteral("最低 15 ℃"),
          QStringLiteral("只填低端显示“最低 15 ℃”"));
    check(DisplayFormat::rangeText(15, 30, QStringLiteral(" ℃"), -100)
              == QStringLiteral("15 ℃ ~ 30 ℃"),
          QStringLiteral("两端都有显示完整范围"));

    // 19. 数值一位小数（防止 pH 长浮点数回归）
    check(qAbs(DisplayFormat::roundOneDecimal(5.300000000001) - 5.3)
              < 0.000001,
          QStringLiteral("pH 舍入到 1 位小数"));
    check(DisplayFormat::formatOneDecimal(7.1000001)
              == QStringLiteral("7.1"),
          QStringLiteral("pH 文本固定 1 位小数"));

    // 20. 照片文件名安全校验
    check(isSafePhotoFileName(QStringLiteral("node1_photo.jpg")),
          QStringLiteral("普通照片文件名合法"));
    check(!isSafePhotoFileName(QStringLiteral("../photo.jpg")),
          QStringLiteral("拒绝含 .. 的照片文件名"));
    check(!isSafePhotoFileName(QStringLiteral("dir/photo.jpg")),
          QStringLiteral("拒绝含路径分隔符的照片文件名"));
    check(!isSafePhotoFileName(QStringLiteral("C:/photo.jpg")),
          QStringLiteral("拒绝绝对路径的照片文件名"));

    // 21. 载入时校验层级，拒绝顶层非“界”的文件
    QJsonObject invalidRoot;
    invalidRoot[QStringLiteral("app")] = QStringLiteral("PlantMap");
    invalidRoot[QStringLiteral("schema_version")] = DbVersion::kCurrentSchemaVersion;
    QJsonObject invalidTaxonomy;
    invalidTaxonomy[QStringLiteral("next_id")] = 2;
    QJsonObject invalidNode;
    invalidNode[QStringLiteral("id")] = 1;
    invalidNode[QStringLiteral("rank")] = QStringLiteral("species");
    invalidNode[QStringLiteral("name")] = QStringLiteral("非法顶级");
    invalidNode[QStringLiteral("children")] = QJsonArray();
    invalidTaxonomy[QStringLiteral("roots")] =
        QJsonArray { invalidNode };
    invalidRoot[QStringLiteral("taxonomy")] = invalidTaxonomy;
    TaxonomyDocument invalidDoc;
    QString invalidError;
    check(!invalidDoc.loadFromJson(invalidRoot, &invalidError),
          QStringLiteral("层级校验拒绝顶层非“界”的分类文件"));

    // 22. fromJson 数值钳制（防止手编 JSON 越界值导致编辑页假性“未保存”）
    QJsonObject weird;
    weird[QStringLiteral("humidity_min_pct")] = -20;
    weird[QStringLiteral("humidity_max_pct")] = 500;
    weird[QStringLiteral("hardiness_zone_low")] = 99;
    weird[QStringLiteral("temperature_min_c")] = -150;
    weird[QStringLiteral("height_min_cm")] = -5;
    weird[QStringLiteral("ph_max")] = 20.0;
    const SpeciesInfo clamped = SpeciesInfo::fromJson(weird);
    check(clamped.humidityMinPct == 0 && clamped.humidityMaxPct == 100,
          QStringLiteral("湿度越界值被钳制到 0-100"));
    check(clamped.hardinessZoneLow == 13,
          QStringLiteral("耐寒区越界值被钳制到 13"));
    check(clamped.temperatureMinC == -100,
          QStringLiteral("温度越界值被钳制到 -100"));
    check(clamped.heightMinCm == 0,
          QStringLiteral("株高负值被钳制到 0"));
    check(qAbs(clamped.phMax - 14.0) < 0.000001,
          QStringLiteral("pH 越界值被钳制到 14"));

    // 23. 病态深层嵌套在解析阶段直接拒绝（防栈溢出）
    QJsonObject deepNode = makeChainNode(20, QStringLiteral("subspecies"),
                                         QStringLiteral("深层"));
    for (int i = 19; i >= 1; --i) {
        deepNode = makeChainNode(i, QStringLiteral("kingdom"),
                                 QStringLiteral("n%1").arg(i),
                                 { deepNode });
    }
    TaxonomyDocument deepDoc;
    QString deepError;
    check(!deepDoc.loadFromJson(wrappedDb({ deepNode }), &deepError)
              && deepError.contains(QStringLiteral("嵌套过深")),
          QStringLiteral("拒绝超深嵌套的 JSON（解析阶段直接报错）"));

    // 24. 规范化后为空的拉丁名给出明确错误，而不是误报“重复”
    QJsonObject badSpecies = makeChainNode(7, QStringLiteral("species"),
                                           QStringLiteral("怪植物"));
    QJsonObject badInfo;
    badInfo[QStringLiteral("scientific_name")] = QStringLiteral("!!!");
    badSpecies[QStringLiteral("info")] = badInfo;
    const QJsonObject badDb = wrappedDb({
        makeChainNode(1, QStringLiteral("kingdom"), QStringLiteral("植物界"), {
            makeChainNode(2, QStringLiteral("phylum"), QStringLiteral("被子植物门"), {
                makeChainNode(3, QStringLiteral("class"), QStringLiteral("木兰纲"), {
                    makeChainNode(4, QStringLiteral("order"), QStringLiteral("蔷薇目"), {
                        makeChainNode(5, QStringLiteral("family"), QStringLiteral("蔷薇科"), {
                            makeChainNode(6, QStringLiteral("genus"), QStringLiteral("蔷薇属"), {
                                badSpecies }) }) }) }) }) }) });
    TaxonomyDocument badDoc;
    QString badError;
    check(!badDoc.loadFromJson(badDb, &badError)
              && badError.contains(QStringLiteral("规范化后为空")),
          QStringLiteral("规范化后为空的拉丁名给出明确错误：%1").arg(badError));

    // 25. 变种/变型/品种等级（种下并列末级）
    TaxonomyDocument rankDoc;
    QString rankError;
    const int speciesId2 = appendChain(rankDoc,
        { QStringLiteral("植物界"), QStringLiteral("被子植物门"),
          QStringLiteral("木兰纲"), QStringLiteral("蔷薇目"),
          QStringLiteral("蔷薇科"), QStringLiteral("蔷薇属"),
          QStringLiteral("玫瑰") }, &rankError);
    const int varietyId = rankDoc.addNode(speciesId2, QStringLiteral("白玫瑰"),
                                          TaxonRank::Variety, &rankError);
    check(varietyId > 0 && rankDoc.node(varietyId)->rank == TaxonRank::Variety,
          QStringLiteral("种下可添加“变种”"));
    const int formId = rankDoc.addNode(speciesId2, QStringLiteral("重瓣型"),
                                       TaxonRank::Form, &rankError);
    check(formId > 0 && rankDoc.node(formId)->rank == TaxonRank::Form,
          QStringLiteral("种下可添加“变型”"));
    const int cultivarId = rankDoc.addNode(speciesId2, QStringLiteral("丰花"),
                                           TaxonRank::Cultivar, &rankError);
    check(cultivarId > 0 && rankDoc.node(cultivarId)->rank == TaxonRank::Cultivar,
          QStringLiteral("种下可添加“品种”"));
    check(rankDoc.addNode(varietyId, QStringLiteral("再下级"), &rankError) == 0,
          QStringLiteral("变种下禁止再添加下级"));
    check(rankDoc.addNode(speciesId2, QStringLiteral("错等级"),
                          TaxonRank::Family, &rankError) == 0,
          QStringLiteral("种下指定非法等级被拒绝"));

    // 26. 含变种/品种的库 JSON 往返
    const QJsonObject rankSnap = rankDoc.toJson();
    TaxonomyDocument rankLoaded;
    check(rankLoaded.loadFromJson(rankSnap, &rankError),
          QStringLiteral("含变种/品种的库 JSON 往返成功"));

    // 27. A1 地理分布字段（原生分布省区 + 生境）
    QJsonObject regionJson;
    regionJson[QStringLiteral("native_regions")] =
        QJsonArray { QStringLiteral("410000"), QStringLiteral("410000"),
                     QStringLiteral("41000"), QStringLiteral("河南省") };
    regionJson[QStringLiteral("habitat")] = QStringLiteral("山地林缘");
    const SpeciesInfo regionInfo = SpeciesInfo::fromJson(regionJson);
    check(regionInfo.nativeRegions == QStringList { QStringLiteral("410000") },
          QStringLiteral("原生分布只保留合法 6 位 adcode（去重、过滤非法）"));
    check(regionInfo.habitat == QStringLiteral("山地林缘"),
          QStringLiteral("生境文本读取正确"));
    check(regionInfo.toJson().value(QStringLiteral("native_regions")).toArray().at(0).toString()
              == QStringLiteral("410000"),
          QStringLiteral("原生分布 toJson 写回"));

    // 28. 拼音首字母检索
    check(Pinyin::initials(QStringLiteral("银杏")) == QStringLiteral("YX"),
          QStringLiteral("拼音首字母：银杏→YX"));
    check(Pinyin::initials(QStringLiteral("牡丹")) == QStringLiteral("MD"),
          QStringLiteral("拼音首字母：牡丹→MD"));
    check(Pinyin::matchesInitials(QStringLiteral("银杏"), QStringLiteral("yx")),
          QStringLiteral("首字母 yx 可匹配银杏"));

    // 29. cloneSubtree 复制子树 + 拉丁名后缀
    TaxonomyDocument cloneDoc;
    const int srcSp = appendChain(cloneDoc,
        { QStringLiteral("植物界"), QStringLiteral("被子植物门"),
          QStringLiteral("木兰纲"), QStringLiteral("蔷薇目"),
          QStringLiteral("蔷薇科"), QStringLiteral("蔷薇属"),
          QStringLiteral("玫瑰") }, &rankError);
    SpeciesInfo srcInfo;
    srcInfo.scientificName = QStringLiteral("Rosa rugosa");
    cloneDoc.setInfo(srcSp, srcInfo, &rankError);
    const int genusId2 = cloneDoc.node(srcSp)->parentId;
    const int cloneId = cloneDoc.cloneSubtree(srcSp, genusId2,
                                              QStringLiteral("玫瑰副本"), &rankError);
    check(cloneId > 0 && cloneDoc.node(cloneId)->name == QStringLiteral("玫瑰副本"),
          QStringLiteral("复制子树成功且副本名正确"));
    check(cloneDoc.node(cloneId)->info.scientificName != QStringLiteral("Rosa rugosa")
              && cloneDoc.node(cloneId)->info.scientificName.contains(QStringLiteral("dup")),
          QStringLiteral("副本拉丁名自动加后缀避免冲突"));

    // 30. restoreSubtreeFromJson 恢复被删子树（撤销删除用）
    const QJsonObject subJson = cloneDoc.subtreeToJson(cloneId);
    cloneDoc.removeNode(cloneId, &rankError);
    const int restoredId = cloneDoc.restoreSubtreeFromJson(subJson, genusId2, &rankError);
    check(restoredId > 0 && cloneDoc.node(restoredId) != nullptr,
          QStringLiteral("restoreSubtreeFromJson 恢复被删子树"));

    // 31. CSV 导出：字段自动推导 + 含 custom 扩展属性 + 逗号转义
    TaxonomyDocument csvDoc;
    const int csvSp = appendChain(csvDoc,
        { QStringLiteral("植物界"), QStringLiteral("被子植物门"),
          QStringLiteral("木兰纲"), QStringLiteral("蔷薇目"),
          QStringLiteral("蔷薇科"), QStringLiteral("蔷薇属"),
          QStringLiteral("玫瑰") }, &rankError);
    SpeciesInfo csvInfo;
    csvInfo.scientificName = QStringLiteral("Rosa rugosa");
    csvInfo.habit = GrowthHabit::Shrub;
    csvInfo.custom.insert(QStringLiteral("耐盐碱性"), QStringLiteral("强"));
    csvInfo.custom.insert(QStringLiteral("备注"), QStringLiteral("含逗号,的字段"));
    csvDoc.setInfo(csvSp, csvInfo, &rankError);

    QString csvText;
    QString csvError;
    check(CsvExport::generate(csvDoc, &csvText, &csvError),
          QStringLiteral("CSV 导出成功"));
    check(csvText.contains(QStringLiteral("等级"))
              && csvText.contains(QStringLiteral("拉丁学名"))
              && csvText.contains(QStringLiteral("耐盐碱性"))
              && csvText.contains(QStringLiteral("备注")),
          QStringLiteral("CSV 含固定列 + 自动推导的 custom 列"));
    check(csvText.contains(QStringLiteral("Rosa rugosa"))
              && csvText.contains(QStringLiteral("灌木"))
              && csvText.contains(QStringLiteral("\"含逗号,的字段\"")),
          QStringLiteral("CSV 内容正确且逗号字段被转义"));

    qInfo() << (failures == 0 ? QStringLiteral("全部自检通过")
                              : QStringLiteral("存在 %1 项失败").arg(failures));
    return failures == 0 ? 0 : 1;
}
