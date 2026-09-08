#include "comparedialog.h"

#include "displayformat.h"
#include "taxondocument.h"

#include <QHeaderView>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace {

QString joinLabels(const QSet<QString>& keys, const QStringList& order,
                   const std::function<QString(const QString&)>& labelFor)
{
    QStringList labels;
    for (const QString& key : order) {
        if (keys.contains(key))
            labels.append(labelFor(key));
    }
    return labels.isEmpty() ? QStringLiteral("未填写") : labels.join(QStringLiteral("、"));
}

} // namespace

CompareDialog::CompareDialog(const TaxonomyDocument* document,
                             const QList<int>& nodeIds,
                             QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("物种对比"));
    resize(760, 560);

    // 收集有效的植物节点。
    QList<const TaxonNode*> plants;
    for (int id : nodeIds) {
        const TaxonNode* n = document->node(id);
        if (n && TaxonRanks::canHostPlantInfo(n->rank))
            plants.append(n);
    }
    if (plants.isEmpty() || plants.size() > 5)
        plants = plants.mid(0, 5);

    auto* layout = new QVBoxLayout(this);

    // 属性行定义：标签 + 取值函数。
    struct Row { QString label; std::function<QString(const TaxonNode*)> getter; };
    QList<Row> rows;
    rows.append({ QStringLiteral("拉丁学名"), [](const TaxonNode* n) {
        return n->hasInfo && !n->info.scientificName.isEmpty()
            ? n->info.scientificName : QStringLiteral("未填写"); } });
    rows.append({ QStringLiteral("生长习性"), [](const TaxonNode* n) {
        return n->hasInfo ? habitLabel(n->info.habit) : QStringLiteral("未填写"); } });
    rows.append({ QStringLiteral("光照"), [](const TaxonNode* n) {
        return n->hasInfo ? lightLabel(n->info.light) : QStringLiteral("未填写"); } });
    rows.append({ QStringLiteral("水分"), [](const TaxonNode* n) {
        return n->hasInfo ? waterLabel(n->info.water) : QStringLiteral("未填写"); } });
    rows.append({ QStringLiteral("温度范围"), [](const TaxonNode* n) {
        return n->hasInfo ? DisplayFormat::rangeText(n->info.temperatureMinC,
                                                     n->info.temperatureMaxC,
                                                     QStringLiteral(" ℃"), -100)
                          : QStringLiteral("未填写"); } });
    rows.append({ QStringLiteral("空气湿度"), [](const TaxonNode* n) {
        return n->hasInfo ? DisplayFormat::rangeText(n->info.humidityMinPct,
                                                     n->info.humidityMaxPct,
                                                     QStringLiteral(" %"))
                          : QStringLiteral("未填写"); } });
    rows.append({ QStringLiteral("土壤 pH"), [](const TaxonNode* n) {
        return n->hasInfo ? DisplayFormat::rangeTextDouble(n->info.phMin,
                                                           n->info.phMax, QString())
                          : QStringLiteral("未填写"); } });
    rows.append({ QStringLiteral("耐寒区"), [](const TaxonNode* n) {
        return n->hasInfo ? DisplayFormat::rangeText(n->info.hardinessZoneLow,
                                                     n->info.hardinessZoneHigh,
                                                     QStringLiteral(" 区"))
                          : QStringLiteral("未填写"); } });
    rows.append({ QStringLiteral("生命周期"), [](const TaxonNode* n) {
        return n->hasInfo ? lifecycleLabel(n->info.lifeCycle) : QStringLiteral("未填写"); } });
    rows.append({ QStringLiteral("叶型"), [](const TaxonNode* n) {
        return n->hasInfo ? foliageLabel(n->info.foliage) : QStringLiteral("未填写"); } });
    rows.append({ QStringLiteral("生长速度"), [](const TaxonNode* n) {
        return n->hasInfo ? growthRateLabel(n->info.growthRate) : QStringLiteral("未填写"); } });
    rows.append({ QStringLiteral("株高"), [](const TaxonNode* n) {
        return n->hasInfo ? DisplayFormat::rangeText(n->info.heightMinCm,
                                                     n->info.heightMaxCm,
                                                     QStringLiteral(" cm"))
                          : QStringLiteral("未填写"); } });
    rows.append({ QStringLiteral("花期"), [](const TaxonNode* n) {
        return n->hasInfo ? DisplayFormat::joinMonths(n->info.bloomMonths)
                          : QStringLiteral("未填写"); } });
    rows.append({ QStringLiteral("繁殖方式"), [](const TaxonNode* n) {
        return n->hasInfo ? joinLabels(n->info.propagationMethods,
                                       predefinedPropagationMethods(),
                                       &propagationLabel)
                          : QStringLiteral("未填写"); } });
    rows.append({ QStringLiteral("观赏/用途"), [](const TaxonNode* n) {
        return n->hasInfo ? joinLabels(n->info.usageTags, predefinedUsageTags(),
                                       &usageTagLabel)
                          : QStringLiteral("未填写"); } });
    rows.append({ QStringLiteral("生境"), [](const TaxonNode* n) {
        return n->hasInfo && !n->info.habitat.isEmpty()
            ? n->info.habitat : QStringLiteral("未填写"); } });

    auto* table = new QTableWidget(rows.size(), plants.size() + 1, this);
    table->setHorizontalHeaderLabels(
        [&]() {
            QStringList headers;
            headers << QStringLiteral("属性");
            for (const TaxonNode* n : plants)
                headers << n->name;
            return headers;
        }());

    for (int r = 0; r < rows.size(); ++r) {
        table->setItem(r, 0, new QTableWidgetItem(rows[r].label));
        for (int c = 0; c < plants.size(); ++c) {
            auto* item = new QTableWidgetItem(rows[r].getter(plants[c]));
            item->setToolTip(rows[r].getter(plants[c]));
            table->setItem(r, c + 1, item);
        }
    }

    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table->horizontalHeader()->setStretchLastSection(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setWordWrap(true);
    layout->addWidget(table);
}
