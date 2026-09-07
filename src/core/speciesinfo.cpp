#include "speciesinfo.h"
#include "displayformat.h"

#include <algorithm>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>

namespace {

QJsonArray monthsToJson(const QVector<int>& months)
{
    QJsonArray array;
    for (int month : months)
        array.append(month);
    return array;
}

QVector<int> monthsFromJson(const QJsonArray& array)
{
    QVector<int> result;
    for (const auto& value : array) {
        int month = value.toInt();
        if (month >= 1 && month <= 12 && !result.contains(month))
            result.append(month);
    }
    std::sort(result.begin(), result.end());
    return result;
}

QJsonArray keysToJson(const QSet<QString>& keys)
{
    QStringList sorted = keys.values();
    sorted.sort();
    QJsonArray array;
    for (const QString& key : sorted)
        array.append(key);
    return array;
}

QSet<QString> keysFromJson(const QJsonArray& array)
{
    QSet<QString> result;
    for (const auto& value : array) {
        const QString key = value.toString();
        if (!key.isEmpty())
            result.insert(key);
    }
    return result;
}

QJsonObject stringsToJson(const QMap<QString, QString>& map)
{
    QJsonObject obj;
    QStringList keys = map.keys();
    keys.sort();
    for (const QString& key : keys)
        obj[key] = map.value(key);
    return obj;
}

} // namespace

bool isSafePhotoFileName(const QString& fileName)
{
    if (fileName.isEmpty()
        || fileName == QLatin1String(".")
        || fileName == QLatin1String(".."))
        return false;
    if (fileName.contains(QLatin1Char('/'))
        || fileName.contains(QLatin1Char('\\'))
        || fileName.contains(QLatin1Char(':')))
        return false;
    if (QDir::isAbsolutePath(fileName))
        return false;
    return QFileInfo(fileName).fileName() == fileName;
}

// ------------------------- 枚举与映射 -------------------------

QString lightToKey(LightPreference value)
{
    switch (value) {
    case LightPreference::FullSun:   return QStringLiteral("full_sun");
    case LightPreference::HalfSun:   return QStringLiteral("half_sun");
    case LightPreference::HalfShade: return QStringLiteral("half_shade");
    case LightPreference::Shade:     return QStringLiteral("shade");
    case LightPreference::Unknown:   break;
    }
    return QStringLiteral("unknown");
}

LightPreference lightFromKey(const QString& key)
{
    if (key == QLatin1String("full_sun"))   return LightPreference::FullSun;
    if (key == QLatin1String("half_sun"))   return LightPreference::HalfSun;
    if (key == QLatin1String("half_shade")) return LightPreference::HalfShade;
    if (key == QLatin1String("shade"))      return LightPreference::Shade;
    return LightPreference::Unknown;
}

QString lightLabel(LightPreference value)
{
    switch (value) {
    case LightPreference::FullSun:   return QStringLiteral("全日照（≥6小时直射）");
    case LightPreference::HalfSun:   return QStringLiteral("半日照（3–6小时）");
    case LightPreference::HalfShade: return QStringLiteral("半阴（散射光为主）");
    case LightPreference::Shade:     return QStringLiteral("耐阴 / 喜阴");
    case LightPreference::Unknown:   break;
    }
    return QStringLiteral("未填写");
}

QString waterToKey(WaterPreference value)
{
    switch (value) {
    case WaterPreference::Dry:      return QStringLiteral("dry");
    case WaterPreference::Moderate: return QStringLiteral("moderate");
    case WaterPreference::Moist:    return QStringLiteral("moist");
    case WaterPreference::Aquatic:  return QStringLiteral("aquatic");
    case WaterPreference::Unknown:  break;
    }
    return QStringLiteral("unknown");
}

WaterPreference waterFromKey(const QString& key)
{
    if (key == QLatin1String("dry"))       return WaterPreference::Dry;
    if (key == QLatin1String("moderate"))  return WaterPreference::Moderate;
    if (key == QLatin1String("moist"))     return WaterPreference::Moist;
    if (key == QLatin1String("aquatic"))   return WaterPreference::Aquatic;
    return WaterPreference::Unknown;
}

QString waterLabel(WaterPreference value)
{
    switch (value) {
    case WaterPreference::Dry:      return QStringLiteral("耐旱（少浇水）");
    case WaterPreference::Moderate: return QStringLiteral("中性（见干见湿）");
    case WaterPreference::Moist:    return QStringLiteral("喜湿");
    case WaterPreference::Aquatic:  return QStringLiteral("水生 / 沼泽");
    case WaterPreference::Unknown:  break;
    }
    return QStringLiteral("未填写");
}

QString habitToKey(GrowthHabit value)
{
    switch (value) {
    case GrowthHabit::Tree:      return QStringLiteral("tree");
    case GrowthHabit::Shrub:     return QStringLiteral("shrub");
    case GrowthHabit::Herb:      return QStringLiteral("herb");
    case GrowthHabit::Vine:      return QStringLiteral("vine");
    case GrowthHabit::Aquatic:   return QStringLiteral("aquatic");
    case GrowthHabit::Succulent: return QStringLiteral("succulent");
    case GrowthHabit::Fern:      return QStringLiteral("fern");
    case GrowthHabit::Other:     return QStringLiteral("other");
    case GrowthHabit::Unknown:   break;
    }
    return QStringLiteral("unknown");
}

GrowthHabit habitFromKey(const QString& key)
{
    if (key == QLatin1String("tree"))       return GrowthHabit::Tree;
    if (key == QLatin1String("shrub"))      return GrowthHabit::Shrub;
    if (key == QLatin1String("herb"))       return GrowthHabit::Herb;
    if (key == QLatin1String("vine"))       return GrowthHabit::Vine;
    if (key == QLatin1String("aquatic"))    return GrowthHabit::Aquatic;
    if (key == QLatin1String("succulent"))  return GrowthHabit::Succulent;
    if (key == QLatin1String("fern"))       return GrowthHabit::Fern;
    if (key == QLatin1String("other"))      return GrowthHabit::Other;
    return GrowthHabit::Unknown;
}

QString habitLabel(GrowthHabit value)
{
    switch (value) {
    case GrowthHabit::Tree:      return QStringLiteral("乔木");
    case GrowthHabit::Shrub:     return QStringLiteral("灌木");
    case GrowthHabit::Herb:      return QStringLiteral("草本");
    case GrowthHabit::Vine:      return QStringLiteral("藤本");
    case GrowthHabit::Aquatic:   return QStringLiteral("水生植物");
    case GrowthHabit::Succulent: return QStringLiteral("多肉植物");
    case GrowthHabit::Fern:      return QStringLiteral("蕨类植物");
    case GrowthHabit::Other:     return QStringLiteral("其他");
    case GrowthHabit::Unknown:   break;
    }
    return QStringLiteral("未填写");
}

QString lifecycleToKey(LifeCycle value)
{
    switch (value) {
    case LifeCycle::Annual:    return QStringLiteral("annual");
    case LifeCycle::Biennial:  return QStringLiteral("biennial");
    case LifeCycle::Perennial: return QStringLiteral("perennial");
    case LifeCycle::Unknown:   break;
    }
    return QStringLiteral("unknown");
}

LifeCycle lifecycleFromKey(const QString& key)
{
    if (key == QLatin1String("annual"))     return LifeCycle::Annual;
    if (key == QLatin1String("biennial"))   return LifeCycle::Biennial;
    if (key == QLatin1String("perennial"))  return LifeCycle::Perennial;
    return LifeCycle::Unknown;
}

QString lifecycleLabel(LifeCycle value)
{
    switch (value) {
    case LifeCycle::Annual:    return QStringLiteral("一年生");
    case LifeCycle::Biennial:  return QStringLiteral("二年生");
    case LifeCycle::Perennial: return QStringLiteral("多年生");
    case LifeCycle::Unknown:   break;
    }
    return QStringLiteral("未填写");
}

QString foliageToKey(FoliageType value)
{
    switch (value) {
    case FoliageType::Evergreen:     return QStringLiteral("evergreen");
    case FoliageType::SemiEvergreen: return QStringLiteral("semi_evergreen");
    case FoliageType::Deciduous:     return QStringLiteral("deciduous");
    case FoliageType::Unknown:       break;
    }
    return QStringLiteral("unknown");
}

FoliageType foliageFromKey(const QString& key)
{
    if (key == QLatin1String("evergreen"))      return FoliageType::Evergreen;
    if (key == QLatin1String("semi_evergreen")) return FoliageType::SemiEvergreen;
    if (key == QLatin1String("deciduous"))      return FoliageType::Deciduous;
    return FoliageType::Unknown;
}

QString foliageLabel(FoliageType value)
{
    switch (value) {
    case FoliageType::Evergreen:     return QStringLiteral("常绿");
    case FoliageType::SemiEvergreen: return QStringLiteral("半常绿");
    case FoliageType::Deciduous:     return QStringLiteral("落叶");
    case FoliageType::Unknown:       break;
    }
    return QStringLiteral("未填写");
}

QString growthRateToKey(GrowthRate value)
{
    switch (value) {
    case GrowthRate::Slow:   return QStringLiteral("slow");
    case GrowthRate::Medium: return QStringLiteral("medium");
    case GrowthRate::Fast:   return QStringLiteral("fast");
    case GrowthRate::Unknown: break;
    }
    return QStringLiteral("unknown");
}

GrowthRate growthRateFromKey(const QString& key)
{
    if (key == QLatin1String("slow"))   return GrowthRate::Slow;
    if (key == QLatin1String("medium")) return GrowthRate::Medium;
    if (key == QLatin1String("fast"))   return GrowthRate::Fast;
    return GrowthRate::Unknown;
}

QString growthRateLabel(GrowthRate value)
{
    switch (value) {
    case GrowthRate::Slow:   return QStringLiteral("慢");
    case GrowthRate::Medium: return QStringLiteral("中等");
    case GrowthRate::Fast:   return QStringLiteral("快");
    case GrowthRate::Unknown: break;
    }
    return QStringLiteral("未填写");
}

// ------------------------- 预置多选选项 -------------------------

QStringList predefinedSoilTypes()
{
    return { QStringLiteral("sand"),   QStringLiteral("loam"),
             QStringLiteral("clay"),   QStringLiteral("humus"),
             QStringLiteral("gravel") };
}

QString soilTypeLabel(const QString& key)
{
    if (key == QLatin1String("sand"))    return QStringLiteral("沙质土（透水强）");
    if (key == QLatin1String("loam"))    return QStringLiteral("壤土（最适宜）");
    if (key == QLatin1String("clay"))    return QStringLiteral("黏土（保水强）");
    if (key == QLatin1String("humus"))   return QStringLiteral("腐殖质土");
    if (key == QLatin1String("gravel"))  return QStringLiteral("砾石土 / 排水良好");
    return key;
}

QStringList predefinedPropagationMethods()
{
    return { QStringLiteral("seed"),    QStringLiteral("cutting"),
             QStringLiteral("division"), QStringLiteral("layering"),
             QStringLiteral("grafting"), QStringLiteral("tissue") };
}

QString propagationLabel(const QString& key)
{
    if (key == QLatin1String("seed"))     return QStringLiteral("播种");
    if (key == QLatin1String("cutting"))  return QStringLiteral("扦插");
    if (key == QLatin1String("division")) return QStringLiteral("分株");
    if (key == QLatin1String("layering")) return QStringLiteral("压条");
    if (key == QLatin1String("grafting")) return QStringLiteral("嫁接");
    if (key == QLatin1String("tissue"))   return QStringLiteral("组织培养");
    return key;
}

QStringList predefinedUsageTags()
{
    return { QStringLiteral("flower"),  QStringLiteral("foliage"),
             QStringLiteral("fruit"),   QStringLiteral("shape"),
             QStringLiteral("aroma"),   QStringLiteral("medicine"),
             QStringLiteral("food"),    QStringLiteral("honey"),
             QStringLiteral("purify") };
}

QString usageTagLabel(const QString& key)
{
    if (key == QLatin1String("flower"))  return QStringLiteral("观花");
    if (key == QLatin1String("foliage")) return QStringLiteral("观叶");
    if (key == QLatin1String("fruit"))   return QStringLiteral("观果");
    if (key == QLatin1String("shape"))   return QStringLiteral("观形");
    if (key == QLatin1String("aroma"))   return QStringLiteral("芳香");
    if (key == QLatin1String("medicine"))return QStringLiteral("药用");
    if (key == QLatin1String("food"))    return QStringLiteral("食用");
    if (key == QLatin1String("honey"))   return QStringLiteral("蜜源");
    if (key == QLatin1String("purify"))  return QStringLiteral("净化空气");
    return key;
}

// ------------------------- SpeciesInfo -------------------------

bool SpeciesInfo::hasScientificName() const
{
    return !scientificName.trimmed().isEmpty();
}

bool SpeciesInfo::isEmpty() const
{
    return !hasScientificName()
        && aliases.isEmpty()
        && description.isEmpty()
        && photos.isEmpty()
        && light == LightPreference::Unknown
        && water == WaterPreference::Unknown
        && temperatureMinC == -100
        && temperatureMaxC == -100
        && humidityMinPct == 0
        && humidityMaxPct == 0
        && phMin == 0.0
        && phMax == 0.0
        && soilTypes.isEmpty()
        && hardinessZoneLow == 0
        && hardinessZoneHigh == 0
        && habit == GrowthHabit::Unknown
        && lifeCycle == LifeCycle::Unknown
        && foliage == FoliageType::Unknown
        && growthRate == GrowthRate::Unknown
        && heightMinCm == 0
        && heightMaxCm == 0
        && spreadMinCm == 0
        && spreadMaxCm == 0
        && propagationMethods.isEmpty()
        && usageTags.isEmpty()
        && bloomMonths.isEmpty()
        && fruitMonths.isEmpty()
        && custom.isEmpty();
}

bool SpeciesInfo::operator==(const SpeciesInfo& other) const
{
    return scientificName == other.scientificName
        && aliases == other.aliases
        && description == other.description
        && photos == other.photos
        && light == other.light
        && water == other.water
        && temperatureMinC == other.temperatureMinC
        && temperatureMaxC == other.temperatureMaxC
        && humidityMinPct == other.humidityMinPct
        && humidityMaxPct == other.humidityMaxPct
        && qRound(phMin * 10.0) == qRound(other.phMin * 10.0)
        && qRound(phMax * 10.0) == qRound(other.phMax * 10.0)
        && soilTypes == other.soilTypes
        && hardinessZoneLow == other.hardinessZoneLow
        && hardinessZoneHigh == other.hardinessZoneHigh
        && habit == other.habit
        && lifeCycle == other.lifeCycle
        && foliage == other.foliage
        && growthRate == other.growthRate
        && heightMinCm == other.heightMinCm
        && heightMaxCm == other.heightMaxCm
        && spreadMinCm == other.spreadMinCm
        && spreadMaxCm == other.spreadMaxCm
        && propagationMethods == other.propagationMethods
        && usageTags == other.usageTags
        && bloomMonths == other.bloomMonths
        && fruitMonths == other.fruitMonths
        && custom == other.custom;
}

QJsonObject SpeciesInfo::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("scientific_name")] = scientificName;
    obj[QStringLiteral("aliases")] = QJsonArray::fromStringList(aliases);
    obj[QStringLiteral("description")] = description;
    obj[QStringLiteral("photos")] = QJsonArray::fromStringList(photos);

    obj[QStringLiteral("light")] = lightToKey(light);
    obj[QStringLiteral("water")] = waterToKey(water);
    obj[QStringLiteral("temperature_min_c")] = temperatureMinC;
    obj[QStringLiteral("temperature_max_c")] = temperatureMaxC;
    obj[QStringLiteral("humidity_min_pct")] = humidityMinPct;
    obj[QStringLiteral("humidity_max_pct")] = humidityMaxPct;
    obj[QStringLiteral("ph_min")] = DisplayFormat::roundOneDecimal(phMin);
    obj[QStringLiteral("ph_max")] = DisplayFormat::roundOneDecimal(phMax);
    obj[QStringLiteral("soil_types")] = keysToJson(soilTypes);
    obj[QStringLiteral("hardiness_zone_low")] = hardinessZoneLow;
    obj[QStringLiteral("hardiness_zone_high")] = hardinessZoneHigh;

    obj[QStringLiteral("habit")] = habitToKey(habit);
    obj[QStringLiteral("life_cycle")] = lifecycleToKey(lifeCycle);
    obj[QStringLiteral("foliage")] = foliageToKey(foliage);
    obj[QStringLiteral("growth_rate")] = growthRateToKey(growthRate);
    obj[QStringLiteral("height_min_cm")] = heightMinCm;
    obj[QStringLiteral("height_max_cm")] = heightMaxCm;
    obj[QStringLiteral("spread_min_cm")] = spreadMinCm;
    obj[QStringLiteral("spread_max_cm")] = spreadMaxCm;
    obj[QStringLiteral("propagation_methods")] = keysToJson(propagationMethods);
    obj[QStringLiteral("usage_tags")] = keysToJson(usageTags);

    obj[QStringLiteral("bloom_months")] = monthsToJson(bloomMonths);
    obj[QStringLiteral("fruit_months")] = monthsToJson(fruitMonths);
    obj[QStringLiteral("custom")] = stringsToJson(custom);
    return obj;
}

SpeciesInfo SpeciesInfo::fromJson(const QJsonObject& obj)
{
    SpeciesInfo info;
    info.scientificName = obj.value(QStringLiteral("scientific_name")).toString().trimmed();
    const QJsonArray aliasesArray = obj.value(QStringLiteral("aliases")).toArray();
    for (const auto& value : aliasesArray) {
        const QString alias = value.toString().trimmed();
        if (!alias.isEmpty() && !info.aliases.contains(alias))
            info.aliases.append(alias);
    }
    info.description = obj.value(QStringLiteral("description")).toString();

    QJsonArray photosArray = obj.value(QStringLiteral("photos")).toArray();
    for (const auto& value : photosArray) {
        const QString fileName = value.toString();
        if (isSafePhotoFileName(fileName))
            info.photos.append(fileName);
    }

    info.light = lightFromKey(obj.value(QStringLiteral("light")).toString());
    info.water = waterFromKey(obj.value(QStringLiteral("water")).toString());
    info.temperatureMinC = obj.value(QStringLiteral("temperature_min_c")).toInt(info.temperatureMinC);
    info.temperatureMaxC = obj.value(QStringLiteral("temperature_max_c")).toInt(info.temperatureMaxC);
    info.humidityMinPct = obj.value(QStringLiteral("humidity_min_pct")).toInt(info.humidityMinPct);
    info.humidityMaxPct = obj.value(QStringLiteral("humidity_max_pct")).toInt(info.humidityMaxPct);
    info.phMin = DisplayFormat::roundOneDecimal(
        obj.value(QStringLiteral("ph_min")).toDouble(info.phMin));
    info.phMax = DisplayFormat::roundOneDecimal(
        obj.value(QStringLiteral("ph_max")).toDouble(info.phMax));
    info.soilTypes = keysFromJson(obj.value(QStringLiteral("soil_types")).toArray());
    info.hardinessZoneLow = obj.value(QStringLiteral("hardiness_zone_low")).toInt();
    info.hardinessZoneHigh = obj.value(QStringLiteral("hardiness_zone_high")).toInt();

    info.habit = habitFromKey(obj.value(QStringLiteral("habit")).toString());
    info.lifeCycle = lifecycleFromKey(obj.value(QStringLiteral("life_cycle")).toString());
    info.foliage = foliageFromKey(obj.value(QStringLiteral("foliage")).toString());
    info.growthRate = growthRateFromKey(obj.value(QStringLiteral("growth_rate")).toString());
    info.heightMinCm = obj.value(QStringLiteral("height_min_cm")).toInt();
    info.heightMaxCm = obj.value(QStringLiteral("height_max_cm")).toInt();
    info.spreadMinCm = obj.value(QStringLiteral("spread_min_cm")).toInt();
    info.spreadMaxCm = obj.value(QStringLiteral("spread_max_cm")).toInt();
    info.propagationMethods = keysFromJson(obj.value(QStringLiteral("propagation_methods")).toArray());
    info.usageTags = keysFromJson(obj.value(QStringLiteral("usage_tags")).toArray());

    info.bloomMonths = monthsFromJson(obj.value(QStringLiteral("bloom_months")).toArray());
    info.fruitMonths = monthsFromJson(obj.value(QStringLiteral("fruit_months")).toArray());

    const QJsonObject customObj = obj.value(QStringLiteral("custom")).toObject();
    for (auto it = customObj.begin(); it != customObj.end(); ++it)
        info.custom.insert(it.key(), it.value().toString());

    return info;
}
