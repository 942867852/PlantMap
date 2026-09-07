#include "taxonrank.h"

namespace TaxonRanks {

TaxonRank highest()
{
    return TaxonRank::Cultivar;
}

QString displayName(TaxonRank rank)
{
    switch (rank) {
    case TaxonRank::Kingdom:    return QStringLiteral("界");
    case TaxonRank::Phylum:     return QStringLiteral("门");
    case TaxonRank::Class:      return QStringLiteral("纲");
    case TaxonRank::Order:      return QStringLiteral("目");
    case TaxonRank::Family:     return QStringLiteral("科");
    case TaxonRank::Genus:      return QStringLiteral("属");
    case TaxonRank::Species:    return QStringLiteral("种");
    case TaxonRank::Subspecies: return QStringLiteral("亚种");
    case TaxonRank::Variety:    return QStringLiteral("变种");
    case TaxonRank::Form:       return QStringLiteral("变型");
    case TaxonRank::Cultivar:   return QStringLiteral("品种");
    case TaxonRank::Invalid:    break;
    }
    return QStringLiteral("未知");
}

QString toKey(TaxonRank rank)
{
    switch (rank) {
    case TaxonRank::Kingdom:    return QStringLiteral("kingdom");
    case TaxonRank::Phylum:     return QStringLiteral("phylum");
    case TaxonRank::Class:      return QStringLiteral("class");
    case TaxonRank::Order:      return QStringLiteral("order");
    case TaxonRank::Family:     return QStringLiteral("family");
    case TaxonRank::Genus:      return QStringLiteral("genus");
    case TaxonRank::Species:    return QStringLiteral("species");
    case TaxonRank::Subspecies: return QStringLiteral("subspecies");
    case TaxonRank::Variety:    return QStringLiteral("variety");
    case TaxonRank::Form:       return QStringLiteral("form");
    case TaxonRank::Cultivar:   return QStringLiteral("cultivar");
    case TaxonRank::Invalid:    break;
    }
    return QStringLiteral("invalid");
}

TaxonRank fromKey(const QString& key)
{
    if (key == QLatin1String("kingdom"))    return TaxonRank::Kingdom;
    if (key == QLatin1String("phylum"))     return TaxonRank::Phylum;
    if (key == QLatin1String("class"))      return TaxonRank::Class;
    if (key == QLatin1String("order"))      return TaxonRank::Order;
    if (key == QLatin1String("family"))     return TaxonRank::Family;
    if (key == QLatin1String("genus"))      return TaxonRank::Genus;
    if (key == QLatin1String("species"))    return TaxonRank::Species;
    if (key == QLatin1String("subspecies")) return TaxonRank::Subspecies;
    if (key == QLatin1String("variety"))    return TaxonRank::Variety;
    if (key == QLatin1String("form"))       return TaxonRank::Form;
    if (key == QLatin1String("cultivar"))   return TaxonRank::Cultivar;
    return TaxonRank::Invalid;
}

bool isValid(TaxonRank rank)
{
    const int value = static_cast<int>(rank);
    return value >= static_cast<int>(TaxonRank::Kingdom)
        && value <= static_cast<int>(TaxonRank::Cultivar);
}

TaxonRank nextLower(TaxonRank rank)
{
    switch (rank) {
    case TaxonRank::Kingdom: return TaxonRank::Phylum;
    case TaxonRank::Phylum:  return TaxonRank::Class;
    case TaxonRank::Class:   return TaxonRank::Order;
    case TaxonRank::Order:   return TaxonRank::Family;
    case TaxonRank::Family:  return TaxonRank::Genus;
    case TaxonRank::Genus:   return TaxonRank::Species;
    case TaxonRank::Species: return TaxonRank::Subspecies;  // 默认下级；UI 可改选变种/变型/品种
    default:                 return TaxonRank::Invalid;      // 亚种/变种/变型/品种都是末级
    }
}

bool canHaveChildren(TaxonRank rank)
{
    // 只有线性链上的界~种可以继续添加下级；
    // 亚种/变种/变型/品种都是末级，不能再分。
    const int value = static_cast<int>(rank);
    return value >= static_cast<int>(TaxonRank::Kingdom)
        && value <= static_cast<int>(TaxonRank::Species);
}

bool canHostPlantInfo(TaxonRank rank)
{
    return rank == TaxonRank::Species
        || isSubrankOfSpecies(rank);
}

QVector<TaxonRank> subRanks()
{
    return { TaxonRank::Subspecies, TaxonRank::Variety,
             TaxonRank::Form, TaxonRank::Cultivar };
}

bool isSubrankOfSpecies(TaxonRank rank)
{
    return rank == TaxonRank::Subspecies
        || rank == TaxonRank::Variety
        || rank == TaxonRank::Form
        || rank == TaxonRank::Cultivar;
}

} // namespace TaxonRanks
