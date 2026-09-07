#pragma once

#include <QString>
#include <QVector>

// 植物分类等级。等级数值越大的表示越“细”。
// 界 → 门 → 纲 → 目 → 科 → 属 → 种 为线性链；
// 种之下并列挂“亚种 / 变种 / 变型 / 品种”四个同级末级等级。
enum class TaxonRank : int {
    Invalid = 0,
    Kingdom = 1,   // 界
    Phylum,        // 门
    Class,         // 纲
    Order,         // 目
    Family,        // 科
    Genus,         // 属
    Species,       // 种
    Subspecies,    // 亚种
    Variety,       // 变种
    Form,          // 变型
    Cultivar       // 品种
};

namespace TaxonRanks {

// 当前支持的最高（最细）等级
TaxonRank highest();

// 中文显示名，例如 “界 / 门 / 种 / 亚种 / 变种 / 变型 / 品种”
QString displayName(TaxonRank rank);

// 稳定的 JSON key，例如 “kingdom / species / variety”
QString toKey(TaxonRank rank);
TaxonRank fromKey(const QString& key);

bool isValid(TaxonRank rank);

// 线性链的“下一级”：界→门→…→属→种→亚种；
// 亚种/变种/变型/品种都是末级，返回 Invalid。
// “种”的默认下级为“亚种”，新增变种/变型/品种请用 addNode 的显式等级重载。
TaxonRank nextLower(TaxonRank rank);

// 该等级是否可以继续添加下级分类（只有界~种可以）。
bool canHaveChildren(TaxonRank rank);

// 只有“种 / 亚种 / 变种 / 变型 / 品种”允许承载物种资料。
bool canHostPlantInfo(TaxonRank rank);

// 种之下的四个并列子等级（亚种/变种/变型/品种），供 UI 选择。
QVector<TaxonRank> subRanks();

// 是否为“种”之下的并列末级等级（亚种/变种/变型/品种）。
bool isSubrankOfSpecies(TaxonRank rank);

} // namespace TaxonRanks
