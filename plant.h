// plant.h
#ifndef PLANT_H
#define PLANT_H

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include "planttraits.h"

class Plant
{
public:
    Plant(){};
    Plant(const QString& name, const QString& desc = "", const QString& imgPath = "");

    QString getName() const;
    QString getDescription() const;
    QString getImagePath() const;
    void setImagePath(const QString& path);

    PlantProperties& properties();
    const PlantProperties& properties() const;

    // JSON
    QJsonObject toJson() const;
    static Plant fromJson(const QJsonObject& obj);

private:
    QString m_name;
    QString m_description;
    QString m_imagePath;  // 图片路径（本地文件或 :/ 资源）
    PlantProperties m_props;
};

Q_DECLARE_METATYPE(Plant)

// 全局 JSON 工具函数
QJsonArray plantsToJson(const QList<Plant>& plants);
QList<Plant> plantsFromJson(const QJsonArray& array);

#endif // PLANT_H
