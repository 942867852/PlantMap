#include "comparedialog.h"

#include "displayformat.h"
#include "taxondocument.h"

#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
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
                             QList<int>* compareList,
                             QWidget* parent)
    : QDialog(parent)
    , m_document(document)
    , m_compareList(compareList)
{
    setWindowTitle(QStringLiteral("物种对比"));
    resize(760, 560);

    rebuildTable();
}

void CompareDialog::rebuildTable()
{
    // 销毁旧内容（同步 delete，确保删除标签/表格立即消失，无残留）。
    QLayout* oldLayout = layout();
    if (oldLayout) {
        QLayoutItem* item;
        while ((item = oldLayout->takeAt(0)) != nullptr) {
            if (item->widget())
                item->widget()->deleteLater();
            delete item;
        }
    } else {
        oldLayout = new QVBoxLayout(this);
    }

    auto* layout = qobject_cast<QVBoxLayout*>(oldLayout);

    // 收集有效的植物节点。
    QList<const TaxonNode*> plants;
    for (int id : *m_compareList) {
        const TaxonNode* n = m_document->node(id);
        if (n && TaxonRanks::canHostPlantInfo(n->rank))
            plants.append(n);
    }

    // 顶部：每个植物一个带“移除”的标签按钮（删除按植物 id 定位，杜绝下标错位）。
    auto* chipRow = new QHBoxLayout;
    chipRow->setSpacing(6);
    auto* hintLabel = new QLabel(QStringLiteral("对比中的植物（点击 × 移除）："), this);
    hintLabel->setStyleSheet(QStringLiteral("color: #666;"));
    chipRow->addWidget(hintLabel);
    chipRow->addStretch();
    layout->addLayout(chipRow);

    auto* chips = new QHBoxLayout;
    chips->setSpacing(8);
    for (const TaxonNode* n : plants) {
        auto* chip = new QPushButton(
            QStringLiteral("%1  ×").arg(n->name), this);
        chip->setToolTip(QStringLiteral("点击把这个植物从对比列表中移除"));
        chip->setStyleSheet(QStringLiteral(
            "QPushButton { padding: 4px 10px; border-radius: 12px;"
            " border: 1px solid #b0b0b0; background: #eef3f8; }"
            "QPushButton:hover { background: #f5c4b3; }"));
        const int plantId = n->id;
        connect(chip, &QPushButton::clicked, this, [this, plantId]() {
            if (m_compareList->contains(plantId)) {
                m_compareList->removeAll(plantId);
                rebuildTable();
            }
        });
        chips->addWidget(chip);
    }
    chips->addStretch();
    layout->addLayout(chips);

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

    if (plants.isEmpty()) {
        // 对比列表已清空：显示提示，不创建空表格。
        auto* emptyLabel = new QLabel(
            QStringLiteral("对比列表已清空。\n\n"
                           "在植物详情页点击“＋ 加入对比”，"
                           "把要比较的植物加入列表后，再点“对比”查看。"),
            this);
        emptyLabel->setAlignment(Qt::AlignCenter);
        emptyLabel->setStyleSheet(QStringLiteral("color: #888; font-size: 14px;"));
        layout->addWidget(emptyLabel, 1);

        auto* closeButton = new QPushButton(QStringLiteral("关闭"), this);
        closeButton->setMinimumHeight(30);
        connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
        layout->addWidget(closeButton);
        return;
    }

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
    layout->addWidget(table, 1);

    // 底部提示。
    auto* hint = new QLabel(
        QStringLiteral("点击上方植物标签的 × 即可从对比中移除。"
                       "在植物详情页点“＋ 加入对比”可继续添加。"),
        this);
    hint->setStyleSheet(QStringLiteral("color: #888;"));
    layout->addWidget(hint);
}
