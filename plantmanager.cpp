// plantmanager.cpp
#include "plantmanager.h"
#include <QFile>
#include <QJsonDocument>
#include <QTextStream>

PlantManager::PlantManager(QObject *parent)
    : QObject(parent)
{}

void PlantManager::addPlant(const Plant &plant)
{
    m_plants.append(plant);
    emit plantsChanged();
}

void PlantManager::updatePlant(const Plant &plant)
{
    for (auto& p : m_plants) {
        if (p.getName() == plant.getName()) {
            p = plant;
            emit plantsChanged();
            return;
        }
    }
    addPlant(plant);
}

void PlantManager::removePlant(const QString &name)
{
    m_plants.erase(
        std::remove_if(m_plants.begin(), m_plants.end(),
                       [&name](const Plant& p){ return p.getName() == name; }),
        m_plants.end()
        );
    emit plantsChanged();
}

void PlantManager::clear()
{
    m_plants.clear();
    emit plantsChanged();
}

QList<Plant> PlantManager::getAllPlants() const { return m_plants; }

QList<Plant> PlantManager::findPlantsByBloomPeriod(const QDate &date) const
{
    QList<Plant> result;
    for (const auto& plant : m_plants) {
        const auto& props = plant.properties();
        if (props.hasBloomPeriod() &&
            date >= props.bloomStart &&
            date <= props.bloomEnd) {
            result.append(plant);
        }
    }
    return result;
}
QList<Plant> PlantManager::findPlantsByLightLevel(PlantProperties::LightLevel level) const
{
    QList<Plant> result;
    for (const auto& plant : m_plants) {
        auto range = plant.properties().lightRange;
        if (level >= range.first && level <= range.second) {
            result.append(plant);
        }
    }
    return result;
}
QList<Plant> PlantManager::findPlantsByTemperature(int temp) const
{
    QList<Plant> result;
    for (const auto& plant : m_plants) {
        if (plant.properties().isSuitableTemperature(temp)) {
            result.append(plant);
        }
    }
    return result;
}
QList<Plant> PlantManager::findPlantsByTraitCategoryAndName(TraitCategory cat, const QString &name) const
{
    QList<Plant> result;
    for (const auto& plant : m_plants) {
        const auto& traits = plant.properties().traits;
        for (const auto& trait : traits) {
            if (trait.category == cat && trait.name == name) {
                result.append(plant);
                break; // 找到一个即可，避免重复添加
            }
        }
    }
    return result;
}

QList<Plant> PlantManager::advancedSearch(
    const QDate *bloomDate,
    std::optional<PlantProperties::LightLevel> light,
    std::optional<int> temp,
    const QSet<TraitItem> *requiredTraits,
    bool matchAllTraits) const
{
    // ... 同上（略）
    QList<Plant> result;

    for (const auto& plant : m_plants) {
        bool match = true;

        if (bloomDate && (!plant.properties().hasBloomPeriod() ||
                          *bloomDate < plant.properties().bloomStart ||
                          *bloomDate > plant.properties().bloomEnd)) {
            match = false;
        }

        if (match && light.has_value()) {
            auto range = plant.properties().lightRange;
            if (light.value() < range.first || light.value() > range.second)
                match = false;
        }

        if (match && temp.has_value()) {
            if (!plant.properties().isSuitableTemperature(temp.value()))
                match = false;
        }

        if (match && requiredTraits) {
            if (matchAllTraits) {
                for (const auto& t : *requiredTraits) {
                    if (!plant.properties().traits.contains(t)) {
                        match = false;
                        break;
                    }
                }
            } else {
                match = false;
                for (const auto& t : *requiredTraits) {
                    if (plant.properties().traits.contains(t)) {
                        match = true;
                        break;
                    }
                }
            }
        }

        if (match) result.append(plant);
    }

    return result;
}

bool PlantManager::saveToFile(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QJsonDocument doc(plantsToJson(m_plants));
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

bool PlantManager::loadFromFile(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) return false;

    m_plants = plantsFromJson(doc.array());
    emit plantsChanged();
    file.close();
    return true;
}
