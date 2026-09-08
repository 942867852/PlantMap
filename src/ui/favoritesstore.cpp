#include "favoritesstore.h"

#include "taxondocument.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

#include <algorithm>

FavoritesStore::FavoritesStore(const QString& dataDir)
    : m_dataDir(dataDir)
{
}

void FavoritesStore::setDocument(TaxonomyDocument* document)
{
    m_document = document;
}

void FavoritesStore::load()
{
    m_favorites.clear();
    const QString path = QDir(m_dataDir).filePath(QStringLiteral("favorites.json"));
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    const QJsonArray arr = doc.array();
    for (const auto& value : arr) {
        const int id = value.toInt();
        const TaxonNode* n = id > 0 && m_document ? m_document->node(id) : nullptr;
        if (n && TaxonRanks::canHostPlantInfo(n->rank))
            m_favorites.insert(id);
    }
}

void FavoritesStore::save() const
{
    QJsonArray arr;
    QList<int> sorted = ids();
    for (int id : sorted)
        arr.append(id);

    const QString path = QDir(m_dataDir).filePath(QStringLiteral("favorites.json"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(QJsonDocument(arr).toJson());
    file.close();
}

bool FavoritesStore::contains(int nodeId) const
{
    return m_favorites.contains(nodeId);
}

bool FavoritesStore::toggle(int nodeId)
{
    if (nodeId <= 0)
        return false;
    if (m_favorites.contains(nodeId))
        m_favorites.remove(nodeId);
    else
        m_favorites.insert(nodeId);
    return m_favorites.contains(nodeId);
}

QList<int> FavoritesStore::ids() const
{
    QList<int> sorted = m_favorites.values();
    std::sort(sorted.begin(), sorted.end());
    return sorted;
}
