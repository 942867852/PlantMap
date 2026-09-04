#pragma once

#include <QString>

// 植物分类等级。等级数值越大的表示越“细”。
// 当前最低一级为“亚种”；以后要支持“变种/变型/品种”时在此扩展，
// 并同步 taxonrank.cpp 中的映射和 nextLower()。
enum class TaxonRank : int {
    Invalid = 0,
    Kingdom = 1,   // 界
    Phylum,        // 门
    Class,         // 纲
    Order,         // 目
    Family,        // 科
    Genus,         // 属
    Species,       // 种
    Subspecies     // 亚种
};

namespace TaxonRanks {

// 当前支持的最高等级
TaxonRank highest();

// 中文显示名，例如 “界 / 门 / 种 / 亚种”
QString displayName(TaxonRank rank);

// 稳定的 JSON key，例如 “kingdom / species”
QString toKey(TaxonRank rank);
TaxonRank fromKey(const QString& key);

bool isValid(TaxonRank rank);

// 指定节点的“下一级”等级；亚种没有下一级时返回 Invalid
TaxonRank nextLower(TaxonRank rank);

// 该等级是否可以继续添加下级分类
bool canHaveChildren(TaxonRank rank);

// 只有“种 / 亚种”允许承载物种资料
bool canHostPlantInfo(TaxonRank rank);

} // namespace TaxonRanks
