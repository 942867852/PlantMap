// plant.cpp
#include "plant.h"

Plant::Plant(const QString& name, const QString& desc, const QString& imgPath)
    : m_name(name), m_description(desc), m_imagePath(imgPath) {}

QString Plant::getName() const { return m_name; }
QString Plant::getDescription() const { return m_description; }
QString Plant::getImagePath() const { return m_imagePath; }
void Plant::setImagePath(const QString& path) { m_imagePath = path; }

PlantProperties& Plant::properties() { return m_props; }
const PlantProperties& Plant::properties() const { return m_props; }

QJsonObject Plant::toJson() const {
    QJsonObject obj;
    obj["name"] = m_name;
    obj["description"] = m_description;
    obj["image_path"] = m_imagePath;
    obj["properties"] = m_props.toJson();
    return obj;
}

Plant Plant::fromJson(const QJsonObject& obj) {
    Plant plant(
        obj["name"].toString(),
        obj["description"].toString(),
        obj["image_path"].toString()
        );
    if (obj.contains("properties"))
        plant.properties() = PlantProperties::fromJson(obj["properties"].toObject());
    return plant;
}


QJsonArray plantsToJson(const QList<Plant>& plants) {
    QJsonArray array;
    for (const auto& plant : plants) {
        array.append(plant.toJson());
    }
    return array;
}

QList<Plant> plantsFromJson(const QJsonArray& array) {
    QList<Plant> list;
    for (const auto& val : array) {
        QJsonObject obj = val.toObject();
        list.append(Plant::fromJson(obj));
    }
    return list;
}
