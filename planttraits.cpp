// planttraits.cpp
#include "planttraits.h"
#include <QJsonObject>
#include <QJsonArray>

QString categoryToString(TraitCategory cat) {
    switch (cat) {
    case TraitCategory::Light:      return QObject::tr("光照");
    case TraitCategory::Water:      return QObject::tr("水分");
    case TraitCategory::Bloom:      return QObject::tr("开花");
    case TraitCategory::Hardiness:  return QObject::tr("耐寒性");
    case TraitCategory::Usage:      return QObject::tr("用途");
    default:                        return QObject::tr("未知");
    }
}

TraitCategory stringToCategory(const QString &str) {
    if (str == QObject::tr("光照")) return TraitCategory::Light;
    if (str == QObject::tr("水分")) return TraitCategory::Water;
    if (str == QObject::tr("开花")) return TraitCategory::Bloom;
    if (str == QObject::tr("耐寒性")) return TraitCategory::Hardiness;
    if (str == QObject::tr("用途")) return TraitCategory::Usage;
    return TraitCategory::Usage;
}

bool TraitItem::operator==(const TraitItem &other) const {
    return name == other.name && category == other.category && description == other.description;
}

bool TraitItem::operator<(const TraitItem &other) const {
    if (category != other.category)
        return static_cast<int>(category) < static_cast<int>(other.category);
    if (name != other.name)
        return name < other.name;
    return description < other.description;
}

uint qHash(const TraitItem &key, uint seed) {
    return ::qHash(key.name, seed) ^ (::qHash(static_cast<int>(key.category)) << 16);
}

QJsonObject traitItemToJson(const TraitItem& item) {
    QJsonObject obj;
    obj["name"] = item.name;
    obj["category_str"] = categoryToString(item.category);
    obj["description"] = item.description;
    return obj;
}

TraitItem traitItemFromJson(const QJsonObject& obj) {
    TraitItem item;
    item.name = obj["name"].toString();
    item.description = obj["description"].toString();
    item.category = stringToCategory(obj["category_str"].toString());
    return item;
}

QString PlantProperties::lightRangeToString() const {
    auto toStr = [](LightLevel l) {
        switch (l) {
        case LightLevel::Low:   return QObject::tr("弱光");
        case LightLevel::Medium:return QObject::tr("中等");
        case LightLevel::High:  return QObject::tr("强光");
        default:                return QObject::tr("未知");
        }
    };
    return toStr(lightRange.first) + " ~ " + toStr(lightRange.second);
}

bool PlantProperties::isSuitableTemperature(int temp) const {
    return temp >= temperatureRange.first && temp <= temperatureRange.second;
}

bool PlantProperties::isSuitablePH(float ph) const
{
    return ph >= pH.first && ph <= pH.second;
}

void PlantProperties::clear() {
    bloomStart = bloomEnd = QDate();
    lightRange = {Low, Low};
    temperatureRange = {0, 0};
    traits.clear();
}

QJsonObject PlantProperties::toJson() const {
    QJsonObject obj;

    if (hasBloomPeriod()) {
        obj["bloom_start"] = bloomStart.toString(Qt::ISODate);
        obj["bloom_end"] = bloomEnd.toString(Qt::ISODate);
    }

    obj["light_min"] = static_cast<int>(lightRange.first);
    obj["light_max"] = static_cast<int>(lightRange.second);

    obj["temp_min"] = temperatureRange.first;
    obj["temp_max"] = temperatureRange.second;

    QJsonArray traitArray;
    for (const auto& t : traits) {
        traitArray.append(traitItemToJson(t));
    }
    obj["traits"] = traitArray;

    return obj;
}

PlantProperties PlantProperties::fromJson(const QJsonObject &obj) {
    PlantProperties props;

    if (obj.contains("bloom_start") && obj.contains("bloom_end")) {
        props.bloomStart = QDate::fromString(obj["bloom_start"].toString(), Qt::ISODate);
        props.bloomEnd = QDate::fromString(obj["bloom_end"].toString(), Qt::ISODate);
    }

    props.lightRange.first = static_cast<LightLevel>(obj["light_min"].toInt(1));
    props.lightRange.second = static_cast<LightLevel>(obj["light_max"].toInt(1));

    props.temperatureRange.first = obj["temp_min"].toInt(0);
    props.temperatureRange.second = obj["temp_max"].toInt(0);

    QJsonArray traitArray = obj["traits"].toArray();
    for (const auto& val : traitArray) {
        QJsonObject tObj = val.toObject();
        TraitItem ti = traitItemFromJson(tObj);
        if (!ti.name.isEmpty()) {
            props.traits.insert(ti);
        }
    }

    return props;
}
