#include "statsdialog.h"

#include "taxondocument.h"
#include "taxonrank.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMap>
#include <QTabWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace {

// 遍历所有承载资料的节点，收集统计。
struct Stats {
    int totalNodes = 0;
    int plantNodes = 0;
    QMap<QString, int> rankCount;    // 等级 -> 数量
    QMap<QString, int> habitCount;   // 习性标签 -> 数量
    QMap<QString, int> regionCount;  // 省区 -> 数量
};

Stats collect(const TaxonomyDocument* doc)
{
    Stats s;
    std::function<void(int)> visit = [&](int parentId) {
        for (int childId : doc->childIdsOf(parentId)) {
            const TaxonNode* n = doc->node(childId);
            if (!n) {
                visit(childId);
                continue;
            }
            ++s.totalNodes;
            s.rankCount[TaxonRanks::displayName(n->rank)]++;

            if (TaxonRanks::canHostPlantInfo(n->rank) && n->hasInfo) {
                ++s.plantNodes;
                s.habitCount[habitLabel(n->info.habit)]++;
                for (const QString& region : n->info.nativeRegions)
                    s.regionCount[region]++;
            }
            visit(childId);
        }
    };
    for (int rootId : doc->roots())
        visit(rootId);
    return s;
}

// 把 map 转成按值降序的 (标签, 数量) 列表。
QList<QPair<QString, int>> sortByValue(const QMap<QString, int>& map)
{
    QList<QPair<QString, int>> list;
    for (auto it = map.constBegin(); it != map.constEnd(); ++it)
        list.append(qMakePair(it.key(), it.value()));
    std::sort(list.begin(), list.end(),
              [](const QPair<QString, int>& a, const QPair<QString, int>& b) {
                  return a.second > b.second;
              });
    return list;
}

// 生成一个简单的“标签 + 数量 + 条形”行。
QLabel* makeBarRow(const QString& label, int value, int max, QWidget* parent)
{
    const int barLen = max > 0 ? (value * 40 / max) : 0;  // 最多 40 个字符宽的条形
    QString bar;
    for (int i = 0; i < barLen; ++i)
        bar.append(QChar(0x2588));  // 实心方块
    auto* row = new QLabel(QStringLiteral("%1  %2  %3")
                               .arg(label, -12)
                               .arg(value, 4)
                               .arg(bar),
                           parent);
    row->setTextInteractionFlags(Qt::TextSelectableByMouse);
    row->setStyleSheet(QStringLiteral("font-family: 'Consolas','Microsoft YaHei';"
                                      " font-size: 13px;"));
    return row;
}

} // namespace

StatsDialog::StatsDialog(const TaxonomyDocument* document, QWidget* parent)
    : QDialog(parent)
    , m_document(document)
{
    setWindowTitle(QStringLiteral("统计面板"));
    resize(560, 560);

    auto* layout = new QVBoxLayout(this);
    auto* tabs = new QTabWidget(this);
    tabs->addTab(buildOverview(), QStringLiteral("总览"));
    tabs->addTab(buildRankDistribution(), QStringLiteral("等级分布"));
    tabs->addTab(buildHabitDistribution(), QStringLiteral("习性分布"));
    tabs->addTab(buildRegionDistribution(), QStringLiteral("省区分布"));
    layout->addWidget(tabs);
}

QWidget* StatsDialog::buildOverview()
{
    const Stats s = collect(m_document);
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);

    auto* box = new QGroupBox(QStringLiteral("总览"), page);
    auto* form = new QVBoxLayout(box);
    form->addWidget(new QLabel(QStringLiteral("分类节点总数：%1").arg(s.totalNodes), box));
    form->addWidget(new QLabel(QStringLiteral("植物（承载资料）数量：%1").arg(s.plantNodes), box));
    layout->addWidget(box);
    layout->addStretch();
    return page;
}

QWidget* StatsDialog::buildRankDistribution()
{
    const Stats s = collect(m_document);
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);

    const QList<QPair<QString, int>> list = sortByValue(s.rankCount);
    const int max = list.isEmpty() ? 0 : list.first().second;
    for (const auto& pair : list)
        layout->addWidget(makeBarRow(pair.first, pair.second, max, page));
    layout->addStretch();
    return page;
}

QWidget* StatsDialog::buildHabitDistribution()
{
    const Stats s = collect(m_document);
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);

    const QList<QPair<QString, int>> list = sortByValue(s.habitCount);
    const int max = list.isEmpty() ? 0 : list.first().second;
    for (const auto& pair : list)
        layout->addWidget(makeBarRow(pair.first, pair.second, max, page));
    layout->addStretch();
    return page;
}

QWidget* StatsDialog::buildRegionDistribution()
{
    const Stats s = collect(m_document);
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);

    const QList<QPair<QString, int>> list = sortByValue(s.regionCount);
    const int max = list.isEmpty() ? 0 : list.first().second;
    // 只显示前 20 名，避免省份太多刷屏。
    int shown = 0;
    for (const auto& pair : list) {
        if (shown++ >= 20)
            break;
        layout->addWidget(makeBarRow(pair.first, pair.second, max, page));
    }
    if (list.isEmpty())
        layout->addWidget(new QLabel(QStringLiteral("暂无原生分布数据。"), page));
    layout->addStretch();
    return page;
}
