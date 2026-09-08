#include "csvexport.h"

#include "displayformat.h"
#include "speciesinfo.h"
#include "taxondocument.h"
#include "taxonrank.h"

#include <QStringList>
#include <functional>

namespace {

// CSV 单元格转义：含逗号/引号/换行的字段用双引号包裹，内部引号翻倍。
QString csvEscape(const QString& field)
{
    if (!field.contains(QLatin1Char(','))
        && !field.contains(QLatin1Char('"'))
        && !field.contains(QLatin1Char('\n'))
        && !field.contains(QLatin1Char('\r'))) {
        return field;
    }
    QString escaped = field;
    escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QStringLiteral("\"%1\"").arg(escaped);
}

// 把多选 key 集合按预定义顺序转成中文标签、用“、”连接。
QString joinedLabels(const QSet<QString>& keys, const QStringList& order,
                     const std::function<QString(const QString&)>& labelFor)
{
    QStringList labels;
    for (const QString& key : order) {
        if (keys.contains(key))
            labels.append(labelFor(key));
    }
    return labels.join(QStringLiteral("、"));
}

} // namespace

namespace CsvExport {

bool generate(const TaxonomyDocument& doc, QString* csv, QString* error)
{
    if (!csv) {
        if (error) *error = QStringLiteral("输出参数无效。");
        return false;
    }

    // 第一遍：遍历所有承载资料的节点，收集“固定列”与“custom 扩展属性列”。
    // 扩展属性列按首次出现顺序收集，保证列顺序稳定。
    QStringList customKeys;
    QList<const TaxonNode*> plants;

    std::function<void(int)> visit = [&](int parentId) {
        for (int childId : doc.childIdsOf(parentId)) {
            const TaxonNode* n = doc.node(childId);
            if (!n) {
                visit(childId);
                continue;
            }
            if (TaxonRanks::canHostPlantInfo(n->rank) && n->hasInfo) {
                plants.append(n);
                for (auto it = n->info.custom.constBegin();
                     it != n->info.custom.constEnd(); ++it) {
                    if (!customKeys.contains(it.key()))
                        customKeys.append(it.key());
                }
            }
            visit(childId);
        }
    };
    for (int rootId : doc.roots())
        visit(rootId);

    if (plants.isEmpty()) {
        if (error) *error = QStringLiteral("没有可导出的植物资料。");
        return false;
    }

    // 固定列。
    QStringList headers;
    headers << QStringLiteral("等级")
            << QStringLiteral("中文名")
            << QStringLiteral("拉丁学名")
            << QStringLiteral("别名")
            << QStringLiteral("光照")
            << QStringLiteral("水分")
            << QStringLiteral("温度范围(℃)")
            << QStringLiteral("空气湿度(%)")
            << QStringLiteral("土壤pH")
            << QStringLiteral("土壤类型")
            << QStringLiteral("耐寒区(USDA)")
            << QStringLiteral("生长习性")
            << QStringLiteral("生命周期")
            << QStringLiteral("叶型")
            << QStringLiteral("生长速度")
            << QStringLiteral("株高(cm)")
            << QStringLiteral("冠幅(cm)")
            << QStringLiteral("繁殖方式")
            << QStringLiteral("观赏/用途")
            << QStringLiteral("花期")
            << QStringLiteral("果期")
            << QStringLiteral("原生分布")
            << QStringLiteral("生境")
            << QStringLiteral("描述");
    for (const QString& key : customKeys)
        headers << key;

    QStringList lines;
    lines << headers.join(QLatin1Char(','));

    // 第二遍：为每个节点生成一行。
    for (const TaxonNode* n : plants) {
        const SpeciesInfo& info = n->info;
        QStringList row;
        row << csvEscape(TaxonRanks::displayName(n->rank))
            << csvEscape(n->name)
            << csvEscape(info.scientificName)
            << csvEscape(info.aliases.join(QStringLiteral("、")))

            << csvEscape(lightLabel(info.light))
            << csvEscape(waterLabel(info.water))
            << csvEscape(DisplayFormat::rangeText(info.temperatureMinC,
                                                  info.temperatureMaxC,
                                                  QString(), -100))
            << csvEscape(DisplayFormat::rangeText(info.humidityMinPct,
                                                  info.humidityMaxPct,
                                                  QString()))
            << csvEscape(DisplayFormat::rangeTextDouble(info.phMin, info.phMax,
                                                        QString()))
            << csvEscape(joinedLabels(info.soilTypes, predefinedSoilTypes(),
                                      &soilTypeLabel))
            << csvEscape(DisplayFormat::rangeText(info.hardinessZoneLow,
                                                  info.hardinessZoneHigh,
                                                  QString()))
            << csvEscape(habitLabel(info.habit))
            << csvEscape(lifecycleLabel(info.lifeCycle))
            << csvEscape(foliageLabel(info.foliage))
            << csvEscape(growthRateLabel(info.growthRate))
            << csvEscape(DisplayFormat::rangeText(info.heightMinCm,
                                                  info.heightMaxCm, QString()))
            << csvEscape(DisplayFormat::rangeText(info.spreadMinCm,
                                                  info.spreadMaxCm, QString()))
            << csvEscape(joinedLabels(info.propagationMethods,
                                      predefinedPropagationMethods(),
                                      &propagationLabel))
            << csvEscape(joinedLabels(info.usageTags, predefinedUsageTags(),
                                      &usageTagLabel))
            << csvEscape(DisplayFormat::joinMonths(info.bloomMonths))
            << csvEscape(DisplayFormat::joinMonths(info.fruitMonths))
            << csvEscape(info.nativeRegions.join(QStringLiteral("、")))
            << csvEscape(info.habitat)
            << csvEscape(info.description);

        for (const QString& key : customKeys)
            row << csvEscape(info.custom.value(key));

        lines << row.join(QLatin1Char(','));
    }

    // UTF-8 BOM，方便 Excel 直接双击打开不乱码。
    csv->clear();
    csv->append(QChar(0xFEFF));
    csv->append(lines.join(QLatin1Char('\n')));
    if (error) error->clear();
    return true;
}

} // namespace CsvExport
