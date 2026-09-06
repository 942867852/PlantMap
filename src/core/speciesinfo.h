#pragma once

#include <QString>
#include <QStringList>
#include <QSet>
#include <QMap>
#include <QVector>
#include <QJsonObject>

// 光照需求
enum class LightPreference {
    Unknown = 0,
    FullSun,    // 全日照
    HalfSun,    // 半日照
    HalfShade,  // 半阴
    Shade       // 耐阴/喜阴
};

// 水分需求
enum class WaterPreference {
    Unknown = 0,
    Dry,        // 耐旱
    Moderate,   // 中性/适中
    Moist,      // 喜湿
    Aquatic     // 水生
};

// 生长习性
enum class GrowthHabit {
    Unknown = 0,
    Tree,       // 乔木
    Shrub,      // 灌木
    Herb,       // 草本
    Vine,       // 藤本
    Aquatic,    // 水生
    Succulent,  // 多肉
    Fern,       // 蕨类
    Other
};

// 生命周期
enum class LifeCycle {
    Unknown = 0,
    Annual,     // 一年生
    Biennial,   // 二年生
    Perennial   // 多年生
};

// 叶型
enum class FoliageType {
    Unknown = 0,
    Evergreen,      // 常绿
    SemiEvergreen,  // 半常绿
    Deciduous       // 落叶
};

// 生长速度
enum class GrowthRate {
    Unknown = 0,
    Slow,
    Medium,
    Fast
};

// 枚举 <-> 稳定 key / 中文标签，供 UI 与 JSON 使用
QString lightToKey(LightPreference value);
LightPreference lightFromKey(const QString& key);
QString lightLabel(LightPreference value);

QString waterToKey(WaterPreference value);
WaterPreference waterFromKey(const QString& key);
QString waterLabel(WaterPreference value);

QString habitToKey(GrowthHabit value);
GrowthHabit habitFromKey(const QString& key);
QString habitLabel(GrowthHabit value);

QString lifecycleToKey(LifeCycle value);
LifeCycle lifecycleFromKey(const QString& key);
QString lifecycleLabel(LifeCycle value);

QString foliageToKey(FoliageType value);
FoliageType foliageFromKey(const QString& key);
QString foliageLabel(FoliageType value);

QString growthRateToKey(GrowthRate value);
GrowthRate growthRateFromKey(const QString& key);
QString growthRateLabel(GrowthRate value);

// 预置选项（返回稳定 key），UI 需要显示时调用对应 label 函数
QStringList predefinedSoilTypes();
QString soilTypeLabel(const QString& key);
QStringList predefinedPropagationMethods();
QString propagationLabel(const QString& key);
QStringList predefinedUsageTags();
QString usageTagLabel(const QString& key);

/*
 * 一个“种/亚种”的全部资料。
 *
 * 增加新属性时：
 *   1. 在结构体里加字段；
 *   2. 在 toJson/fromJson 中读写；
 *   3. 在 SpeciesForm 中加一个输入控件。
 * 只想临时记录任意键值时，可直接使用 custom。
 */
struct SpeciesInfo {
    // ---- 身份 ----
    QString scientificName;      // 拉丁学名（全库唯一，规范化后查重）
    QStringList aliases;         // 别名 / 俗名
    QString description;         // 自定义描述
    QStringList photos;          // 相对 <数据目录>/photos/ 的文件名

    // ---- 环境需求 ----
    LightPreference light = LightPreference::Unknown;
    WaterPreference water = WaterPreference::Unknown;
    int temperatureMinC = -100;  // ℃，-100 表示未填写
    int temperatureMaxC = -100;
    int humidityMinPct = 0;      // %，0 表示未填写
    int humidityMaxPct = 0;
    double phMin = 0.0;          // 0 表示未填写
    double phMax = 0.0;
    QSet<QString> soilTypes;     // predefinedSoilTypes() 的子集
    int hardinessZoneLow = 0;    // USDA 耐寒区，0 表示未填写
    int hardinessZoneHigh = 0;

    // ---- 生长形态 ----
    GrowthHabit habit = GrowthHabit::Unknown;
    LifeCycle lifeCycle = LifeCycle::Unknown;
    FoliageType foliage = FoliageType::Unknown;
    GrowthRate growthRate = GrowthRate::Unknown;
    int heightMinCm = 0;         // 0 表示未填写
    int heightMaxCm = 0;
    int spreadMinCm = 0;
    int spreadMaxCm = 0;
    QSet<QString> propagationMethods;  // predefinedPropagationMethods() 的子集
    QSet<QString> usageTags;           // predefinedUsageTags() 的子集

    // ---- 物候（月份 1..12）----
    QVector<int> bloomMonths;
    QVector<int> fruitMonths;

    // ---- 自定义扩展键值 ----
    QMap<QString, QString> custom;

    bool hasScientificName() const;
    bool isEmpty() const;
    bool operator==(const SpeciesInfo& other) const;
    bool operator!=(const SpeciesInfo& other) const
    {
        return !(*this == other);
    }

    QJsonObject toJson() const;
    static SpeciesInfo fromJson(const QJsonObject& obj);
};
