// planttraits.h
#ifndef PLANTTRAITS_H
#define PLANTTRAITS_H

#include <QString>
#include <QDate>
#include <QSet>
#include <QMap>
#include <QMetaType>
#include <QJsonObject>
#include "taxonomy.h"

//植物习性大类枚举
enum class TraitCategory {
    Light,      // 光照
    Water,      // 水分
    Bloom,      // 开花相关
    Hardiness,  // 耐寒性
    Usage       // 用途
};

QString categoryToString(TraitCategory cat);
TraitCategory stringToCategory(const QString& str);

/*
植物具体习性
1.命名
2.习性枚举
3.描述
*/
struct TraitItem {
    QString name;
    TraitCategory category;
    QString description;

    bool operator==(const TraitItem& other) const;
    bool operator<(const TraitItem& other) const; // for QSet sorting
};

uint qHash(const TraitItem& key, uint seed = 0);

// Convert to/from JSON
QJsonObject traitItemToJson(const TraitItem& item);
TraitItem traitItemFromJson(const QJsonObject& obj);

class PlantProperties {
public:
    QDate bloomStart;
    QDate bloomEnd;
    bool hasBloomPeriod() const { return bloomStart.isValid() && bloomEnd.isValid(); }

    enum LightLevel { Low = 1, Medium = 2, High = 3 };
    QPair<LightLevel, LightLevel> lightRange{Low, Low};
    QString lightRangeToString() const;

    QPair<int, int> temperatureRange{0, 0};
    bool isSuitableTemperature(int temp) const;

    QPair<float, float> pH{6,8};
    bool isSuitablePH(float ph) const;

    QSet<TraitItem> traits;
    void clear();

    // JSON
    QJsonObject toJson() const;
    static PlantProperties fromJson(const QJsonObject& obj);

    // 🌿 分类学信息
    QSharedPointer<TaxonNode> taxonomyNode; // 指向该植物的“种”或“变种”节点

    // 快捷访问方法
    QString getFamily() const { return taxonomyNode ? taxonomyNode->getAncestorName(TaxonomicRank::Order) : ""; }
    QString getGenus() const { return taxonomyNode ? taxonomyNode->getAncestorName(TaxonomicRank::Family) : ""; }
    QString getSpecies() const { return taxonomyNode ? taxonomyNode->getName() : ""; }


};

Q_DECLARE_METATYPE(PlantProperties)
Q_DECLARE_METATYPE(TraitItem)

#endif // PLANTTRAITS_H
