// traitregistry.cpp
#include "traitregistry.h"

TraitRegistry& TraitRegistry::instance()
{
    static TraitRegistry inst;
    return inst;
}

TraitRegistry::TraitRegistry(QObject *parent)
    : QObject(parent)
{
    addTrait({"喜阳",     TraitCategory::Light, "喜欢充足阳光"});
    addTrait({"耐阴",     TraitCategory::Light, "适合阴暗环境"});
    addTrait({"半阴",     TraitCategory::Light, "中等光照"});

    addTrait({"喜湿",     TraitCategory::Water, "需要高湿度"});
    addTrait({"耐旱",     TraitCategory::Water, "抗干旱"});
    addTrait({"中等需水", TraitCategory::Water, "适中浇水"});

    addTrait({"开花",     TraitCategory::Bloom, "会开花"});
    addTrait({"四季开花", TraitCategory::Bloom, "全年开花"});

    addTrait({"耐寒",     TraitCategory::Hardiness, "能过冬"});
    addTrait({"不耐寒",   TraitCategory::Hardiness, "怕冷"});

    addTrait({"观叶",     TraitCategory::Usage, "以叶子为主观赏点"});
    addTrait({"药用",     TraitCategory::Usage, "可入药"});
    addTrait({"食用",     TraitCategory::Usage, "可食用"});

    emit traitsChanged();
}

bool TraitRegistry::addTrait(const TraitItem &item)
{
    if (item.name.isEmpty()) return false;
    m_traitsMap[item.category].insert(item);
    emit traitsChanged();
    return true;
}

bool TraitRegistry::removeTrait(const QString &name, TraitCategory cat)
{
    auto& set = m_traitsMap[cat];
    for (auto it = set.begin(); it != set.end(); ++it) {
        if (it->name == name) {
            set.erase(it);
            emit traitsChanged();
            return true;
        }
    }
    return false;
}

QList<TraitItem> TraitRegistry::getTraitsByCategory(TraitCategory cat) const
{
    QList<TraitItem> list;
    auto it = m_traitsMap.find(cat);
    if (it != m_traitsMap.end())
        list = QList<TraitItem>(it.value().begin(), it.value().end());
    return list;
}

QMap<TraitCategory, QList<TraitItem>> TraitRegistry::getAllTraitsGroupedByCategory() const
{
    QMap<TraitCategory, QList<TraitItem>> result;
    for (auto it = m_traitsMap.begin(); it != m_traitsMap.end(); ++it) {
        result[it.key()] = QList<TraitItem>(it.value().begin(), it.value().end());
    }
    return result;
}

bool TraitRegistry::containsTrait(const QString &name, TraitCategory cat) const
{
    auto traits = getTraitsByCategory(cat);
    return std::any_of(traits.begin(), traits.end(),
                       [&name](const TraitItem& t){ return t.name == name; });
}

TraitItem TraitRegistry::findTrait(const QString &name, TraitCategory cat) const
{
    auto traits = getTraitsByCategory(cat);
    for (const auto& t : traits)
        if (t.name == name) return t;
    return {};
}
