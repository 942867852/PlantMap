// traitregistry.h - unchanged
#ifndef TRAITREGISTRY_H
#define TRAITREGISTRY_H

#include <QObject>
#include <QMap>
#include "planttraits.h"

class TraitRegistry : public QObject
{
    Q_OBJECT

public:
    //单例函数入口
    static TraitRegistry& instance();
    QList<TraitItem> getTraitsByCategory(TraitCategory cat) const;
    QMap<TraitCategory, QList<TraitItem>> getAllTraitsGroupedByCategory() const;
    bool addTrait(const TraitItem& item);
    bool removeTrait(const QString& name, TraitCategory cat);
    bool containsTrait(const QString& name, TraitCategory cat) const;
    TraitItem findTrait(const QString& name, TraitCategory cat) const;

signals:
    void traitsChanged();

private:
    explicit TraitRegistry(QObject* parent = nullptr);
    QMap<TraitCategory, QSet<TraitItem>> m_traitsMap;
};

inline TraitRegistry& Traits() { return TraitRegistry::instance(); }

#endif // TRAITREGISTRY_H
