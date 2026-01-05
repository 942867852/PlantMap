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

    void setLatinName(QString latinName);   // 设置拉丁文名
    void setName(QString name) {m_name = name;}     // 设置品种名，若无品种名，填入学名
    void setAliasName(QString aliasName) {m_alias.append(aliasName);}   // 设置别名/俗名

    QString getName() const;
    QStringList getAliasName() const;
    QString getDescription () const;
    QStringList getImagePath() const;
    void setImagePath(const QString& path);

    PlantProperties& properties(); // 返回植物属性
    const PlantProperties& properties() const;

    // JSON
    QJsonObject toJson() const;
    static Plant fromJson(const QJsonObject& obj);

private:
    QStringList m_alias;//植物别名
    QString m_name;//植物品种名
    QString m_latinName;
    QString m_description;
    QStringList m_imagePath;  // 图片路径（本地文件或 :/ 资源）图片命名使用拉丁名
    PlantProperties m_props;
};

Q_DECLARE_METATYPE(Plant)

// 全局 JSON 工具函数
QJsonArray plantsToJson(const QList<Plant>& plants);
QList<Plant> plantsFromJson(const QJsonArray& array);

#endif // PLANT_H
