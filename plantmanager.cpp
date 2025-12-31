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
    for (auto& p : m_plants)
    {
        if (p.getName() == plant.getName())
        {
            qDebug() << "重复添加";
            return;
        }
    }
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

// -------------------------------
// 导出植物到 CSV 文件
// -------------------------------
bool PlantManager::exportPlantsToCsv(const QString &filename) const
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "无法打开文件进行写入:" << filename;
        return false;
    }

    QTextStream out(&file);

    // 写入表头
    out << "名称,描述,图片路径,花期开始,花期结束,光照最小,光照最大,温度最小,温度最大,分类节点,标签\n";

    // 写入每行数据
    for (const auto& plant : m_plants) {
        const auto& props = plant.properties();

        // 基本信息
        QString name = plant.getName().replace("\"", "\"\"");
        QString description = plant.getDescription().replace("\"", "\"\"");
        QString imagePath = plant.getImagePath().replace("\"", "\"\"");

        // 花期
        QString bloomStart = props.hasBloomPeriod() ? props.bloomStart.toString(Qt::ISODate) : "";
        QString bloomEnd = props.hasBloomPeriod() ? props.bloomEnd.toString(Qt::ISODate) : "";

        // 光照
        QString lightMin = QString::number(static_cast<int>(props.lightRange.first));
        QString lightMax = QString::number(static_cast<int>(props.lightRange.second));

        // 温度
        QString tempMin = QString::number(props.temperatureRange.first);
        QString tempMax = QString::number(props.temperatureRange.second);

        // 分类节点（路径）
        QString taxonomyPath = "";
        if (props.taxonomyNode) {
            taxonomyPath = props.taxonomyNode->getFullPath().join(" > ");
        }

        // 标签（用逗号分隔）
        QStringList traitNames;
        for (const auto& trait : props.traits) {
            traitNames.append(trait.name.replace("\"", "\"\""));
        }
        QString traits = traitNames.join(",");

        // 写入一行
        out << QString("\"%1\",\"%2\",\"%3\",\"%4\",\"%5\",\"%6\",\"%7\",\"%8\",\"%9\",\"%10\",\"%11\"\n")
                   .arg(name)
                   .arg(description)
                   .arg(imagePath)
                   .arg(bloomStart)
                   .arg(bloomEnd)
                   .arg(lightMin)
                   .arg(lightMax)
                   .arg(tempMin)
                   .arg(tempMax)
                   .arg(taxonomyPath)
                   .arg(traits);
    }

    file.close();
    return true;
}

// -------------------------------
// 从 CSV 文件导入植物
// -------------------------------
bool PlantManager::importPlantsFromCsv(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "无法打开文件进行读取:" << filename;
        return false;
    }

    QTextStream in(&file);
    QString headerLine = in.readLine(); // 跳过表头

    int lineNumber = 1;
    while (!in.atEnd()) {
        lineNumber++;
        QString line = in.readLine();
        if (line.trimmed().isEmpty()) continue;

        // 解析 CSV 行
        QStringList fields = parseCsvLine(line);
        if (fields.size() < 11) {
            qWarning() << "第" << lineNumber << "行格式错误";
            continue;
        }

        try {
            // 解析各个字段
            QString name = fields[0].trimmed().remove('"');
            QString description = fields[1].trimmed().remove('"');
            QString imagePath = fields[2].trimmed().remove('"');

            // 花期
            QDate bloomStart, bloomEnd;
            if (!fields[3].trimmed().isEmpty()) {
                bloomStart = QDate::fromString(fields[3].trimmed().remove('"'), Qt::ISODate);
            }
            if (!fields[4].trimmed().isEmpty()) {
                bloomEnd = QDate::fromString(fields[4].trimmed().remove('"'), Qt::ISODate);
            }

            // 光照
            PlantProperties::LightLevel lightMin = static_cast<PlantProperties::LightLevel>(
                fields[5].trimmed().toInt());
            PlantProperties::LightLevel lightMax = static_cast<PlantProperties::LightLevel>(
                fields[6].trimmed().toInt());

            // 温度
            int tempMin = fields[7].trimmed().toInt();
            int tempMax = fields[8].trimmed().toInt();

            // 分类节点
            QString taxonomyPath = fields[9].trimmed().remove('"');

            // 标签
            QString traitsStr = fields[10].trimmed().remove('"');
            QStringList traitNames = traitsStr.split(",", Qt::SkipEmptyParts);

            // 创建植物对象
            Plant plant(name, description, imagePath);

            // 设置属性
            auto& props = plant.properties();
            props.bloomStart = bloomStart;
            props.bloomEnd = bloomEnd;
            props.lightRange = {lightMin, lightMax};
            props.temperatureRange = {tempMin, tempMax};

            // 设置分类节点（如果存在）
            if (!taxonomyPath.isEmpty()) {
                // 这里需要从路径中查找节点，但需要额外逻辑
                // 简化版本：不自动设置分类节点
            }

            // 添加标签
            for (const QString& traitName : traitNames) {
                // 这里需要从 TraitRegistry 中查找标签
                // 简化版本：暂时跳过标签设置
            }

            // 添加到管理器
            addPlant(plant);

        } catch (...) {
            qWarning() << "第" << lineNumber << "行解析错误";
        }
    }

    file.close();
    emit plantsChanged();
    return true;
}

// -------------------------------
// 解析 CSV 行（处理引号和逗号）
// -------------------------------
QStringList PlantManager::parseCsvLine(const QString &line) const
{
    QStringList fields;
    QString field;
    bool inQuotes = false;

    for (int i = 0; i < line.length(); ++i) {
        QChar ch = line[i];

        if (ch == '"') {
            if (inQuotes && i + 1 < line.length() && line[i + 1] == '"') {
                // 转义的引号
                field += '"';
                ++i;
            } else {
                // 引号开始/结束
                inQuotes = !inQuotes;
            }
        } else if (ch == ',' && !inQuotes) {
            // 逗号分隔符
            fields.append(field);
            field.clear();
        } else {
            field += ch;
        }
    }

    // 添加最后一个字段
    fields.append(field);
    return fields;
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
