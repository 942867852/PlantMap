#include "photoimageutils.h"

#include <QHash>
#include <QImage>
#include <QImageReader>

namespace {

// 缩略图/小图标缓存：path -> QPixmap。上限 512 张，超出时整体清空，
// 避免长时间运行内存无限增长。
QHash<QString, QPixmap>& thumbnailCache()
{
    static QHash<QString, QPixmap> cache;
    return cache;
}

constexpr int kCacheLimit = 512;

QPixmap cachedLoad(const QString& path, int maxEdge)
{
    QHash<QString, QPixmap>& cache = thumbnailCache();
    auto it = cache.find(path);
    if (it != cache.end())
        return it.value();

    const QPixmap pixmap = PhotoImageUtils::loadScaled(path, maxEdge);
    if (cache.size() >= kCacheLimit)
        cache.clear();
    if (!pixmap.isNull())
        cache.insert(path, pixmap);
    return pixmap;
}

} // namespace

namespace PhotoImageUtils {

QPixmap loadScaled(const QString& path, int maxEdge)
{
    QImageReader reader(path);
    const QSize original = reader.size();
    if (original.isValid()
        && qMax(original.width(), original.height()) > maxEdge) {
        reader.setScaledSize(
            original.scaled(QSize(maxEdge, maxEdge), Qt::KeepAspectRatio));
    }
    const QImage image = reader.read();
    return image.isNull() ? QPixmap() : QPixmap::fromImage(image);
}

QPixmap loadThumbnail(const QString& path)
{
    return cachedLoad(path, 176);
}

QPixmap loadSearchIcon(const QString& path)
{
    return cachedLoad(path, 96);
}

QPixmap loadPreview(const QString& path)
{
    return loadScaled(path, 2048);
}

void clearCache()
{
    thumbnailCache().clear();
}

} // namespace PhotoImageUtils
