// traitregistry.cpp
#include "traitregistry.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextStream>

TraitRegistry& TraitRegistry::instance()
{
    static TraitRegistry inst;
    return inst;
}

TraitRegistry::TraitRegistry(QObject *parent)
    : QObject(parent)
{
    addTrait({"喜阳",     TraitCategory::Light, "喜欢充足阳光"});
    addTrait({"耐阴",     TraitCategory::Light, "适合阴暗环境"});
    addTrait({"半阴",     TraitCategory::Light, "中等光照"});

    addTrait({"喜湿",     TraitCategory::Water, "需要高湿度"});
    addTrait({"耐旱",     TraitCategory::Water, "抗干旱"});
    addTrait({"中等需水", TraitCategory::Water, "适中浇水"});

    addTrait({"开花",     TraitCategory::Bloom, "会开花"});
    addTrait({"四季开花", TraitCategory::Bloom, "全年开花"});

    addTrait({"耐寒",     TraitCategory::Hardiness, "能过冬"});
    addTrait({"不耐寒",   TraitCategory::Hardiness, "怕冷"});

    addTrait({"观叶",     TraitCategory::Usage, "以叶子为主观赏点"});
    addTrait({"药用",     TraitCategory::Usage, "可入药"});
    addTrait({"食用",     TraitCategory::Usage, "可食用"});

    emit traitsChanged();
}

bool TraitRegistry::addTrait(const TraitItem &item)
{
    if (item.name.isEmpty()) return false;
    m_traitsMap[item.category].insert(item);
    emit traitsChanged();
    return true;
}

bool TraitRegistry::removeTrait(const QString &name, TraitCategory cat)
{
    auto& set = m_traitsMap[cat];
    for (auto it = set.begin(); it != set.end(); ++it) {
        if (it->name == name) {
            set.erase(it);
            emit traitsChanged();
            return true;
        }
    }
    return false;
}

QList<TraitItem> TraitRegistry::getTraitsByCategory(TraitCategory cat) const
{
    QList<TraitItem> list;
    auto it = m_traitsMap.find(cat);
    if (it != m_traitsMap.end())
        list = QList<TraitItem>(it.value().begin(), it.value().end());
    return list;
}

QMap<TraitCategory, QList<TraitItem>> TraitRegistry::getAllTraitsGroupedByCategory() const
{
    QMap<TraitCategory, QList<TraitItem>> result;
    for (auto it = m_traitsMap.begin(); it != m_traitsMap.end(); ++it) {
        result[it.key()] = QList<TraitItem>(it.value().begin(), it.value().end());
    }
    return result;

}

bool TraitRegistry::containsTrait(const QString &name, TraitCategory cat) const
{
    auto traits = getTraitsByCategory(cat);
    return std::any_of(traits.begin(), traits.end(),
                       [&name](const TraitItem& t){ return t.name == name; });
}

TraitItem TraitRegistry::findTrait(const QString &name, TraitCategory cat) const
{
    auto traits = getTraitsByCategory(cat);
    for (const auto& t : traits)
        if (t.name == name) return t;
    return {};
}

QList<TraitItem> TraitRegistry::getTaritItem(TraitCategory cat) const
{
    QList<TraitItem> result;
    for(const auto& it : m_traitsMap[cat])
    {
        result.append(it);
    }
    return result;
}

// -------------------------------
// 导出：保存所有 trait 到 JSON 文件
// -------------------------------
bool TraitRegistry::exportToJson(const QString &filename) const
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "无法打开文件进行写入:" << filename;
        return false;
    }

    QJsonObject rootObj;

    // 遍历每个分类
    for (auto it = m_traitsMap.begin(); it != m_traitsMap.end(); ++it) {
        QString categoryName = categoryToString(it.key());
        QJsonArray traitArray;

        // 将该分类下所有 trait 转为 JSON
        for (const auto& trait : it.value()) {
            traitArray.append(traitItemToJson(trait));
        }

        rootObj[categoryName] = traitArray;
    }

    QJsonDocument doc(rootObj);
    if (file.write(doc.toJson(QJsonDocument::Indented)) == -1) {
        qWarning() << "写入文件失败:" << filename;
        file.close();
        return false;
    }

    file.close();
    return true;
}

// -------------------------------
// 导入：从 JSON 文件加载 trait
// -------------------------------
bool TraitRegistry::importFromJson(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "无法打开文件进行读取:" << filename;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        qWarning() << "JSON 格式错误：根节点不是对象";
        return false;
    }

    QJsonObject rootObj = doc.object();

    // 临时存储，避免中途出错时破坏原数据
    QMap<TraitCategory, QSet<TraitItem>> newTraitsMap;

    for (auto it = rootObj.begin(); it != rootObj.end(); ++it) {
        QString categoryName = it.key();
        TraitCategory cat = stringToCategory(categoryName); // 反向映射

        QJsonArray traitArray = it.value().toArray();
        QSet<TraitItem> traitSet;

        for (const auto& val : traitArray) {
            if (val.isObject()) {
                TraitItem ti = traitItemFromJson(val.toObject());
                if (!ti.name.isEmpty()) {
                    traitSet.insert(ti);
                }
            }
        }

        newTraitsMap[cat] = traitSet;
    }

    // 全部解析成功后再替换旧数据
    m_traitsMap = newTraitsMap;
    emit traitsChanged(); // 告诉 UI 刷新界面

    return true;
}
