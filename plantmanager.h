// plantmanager.h
#ifndef PLANTMANAGER_H
#define PLANTMANAGER_H

#include <QObject>
#include <QList>
#include "plant.h"
#include <optional>

class PlantManager : public QObject
{
    Q_OBJECT

public:
    explicit PlantManager(QObject* parent = nullptr);
    void addPlant(const Plant& plant);
    void updatePlant(const Plant& plant);
    void removePlant(const QString& name);
    void clear();

    QList<Plant> getAllPlants() const;

    QList<Plant> findPlantsByBloomPeriod(const QDate& date) const;
    QList<Plant> findPlantsByLightLevel(PlantProperties::LightLevel level) const;
    QList<Plant> findPlantsByTemperature(int temp) const;
    QList<Plant> findPlantsByTraitCategoryAndName(TraitCategory cat, const QString& name) const;

    QList<Plant> advancedSearch(
        const QDate* bloomDate = nullptr,
        std::optional<PlantProperties::LightLevel> light = std::nullopt,
        std::optional<int> temp = std::nullopt,
        const QSet<TraitItem>* requiredTraits = nullptr,
        bool matchAllTraits = true
        ) const;

    // JSON 文件操作（示例）
    bool saveToFile(const QString& filename);
    bool loadFromFile(const QString& filename);

signals:
    void plantsChanged();

private:
    QList<Plant> m_plants;
};

#endif // PLANTMANAGER_H
