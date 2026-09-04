#include "taxonrank.h"

namespace TaxonRanks {

TaxonRank highest()
{
    return TaxonRank::Subspecies;
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
    return TaxonRank::Invalid;
}

bool isValid(TaxonRank rank)
{
    const int value = static_cast<int>(rank);
    return value >= static_cast<int>(TaxonRank::Kingdom)
        && value <= static_cast<int>(TaxonRank::Subspecies);
}

TaxonRank nextLower(TaxonRank rank)
{
    if (!isValid(rank))
        return TaxonRank::Invalid;
    const int value = static_cast<int>(rank) + 1;
    return isValid(static_cast<TaxonRank>(value))
        ? static_cast<TaxonRank>(value)
        : TaxonRank::Invalid;
}

bool canHaveChildren(TaxonRank rank)
{
    return nextLower(rank) != TaxonRank::Invalid;
}

bool canHostPlantInfo(TaxonRank rank)
{
    return rank == TaxonRank::Species || rank == TaxonRank::Subspecies;
}

} // namespace TaxonRanks
