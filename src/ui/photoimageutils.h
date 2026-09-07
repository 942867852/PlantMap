#pragma once

#include <QImage>
#include <QImageReader>
#include <QPixmap>
#include <QSize>
#include <QtGlobal>

// 照片图片的按需缩放加载：缩略图/预览都不再全量解码原始大图，
// 避免照片目录里存在高分辨率图片时界面变卡、内存暴涨。
namespace PhotoImageUtils {

// 以不超过 maxEdge 像素的边长加载图片（保持长宽比），失败返回空 QPixmap。
inline QPixmap loadScaled(const QString& path, int maxEdge)
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

// 列表缩略图按 88px 显示，取 2 倍尺寸兼顾高分屏。
inline QPixmap loadThumbnail(const QString& path)
{
    return loadScaled(path, 176);
}

// 搜索结果小图标按 48px 显示，同样取 2 倍尺寸。
inline QPixmap loadSearchIcon(const QString& path)
{
    return loadScaled(path, 96);
}

// 大图预览限制在 2048px 内，足够覆盖常见窗口尺寸。
inline QPixmap loadPreview(const QString& path)
{
    return loadScaled(path, 2048);
}

} // namespace PhotoImageUtils
