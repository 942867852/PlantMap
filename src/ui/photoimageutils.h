#pragma once

#include <QPixmap>
#include <QString>

// 照片图片的按需缩放加载：缩略图/预览都不再全量解码原始大图，
// 避免照片目录里存在高分辨率图片时界面变卡、内存暴涨。
//
// loadThumbnail / loadSearchIcon 带全局缓存（按路径缓存缩放后的图），
// 切换植物时重复的照片无需重新解码；缓存有大小上限，超出时清空。
// loadPreview（大图）不缓存，避免占用过多内存。
namespace PhotoImageUtils {

// 以不超过 maxEdge 像素的边长加载图片（保持长宽比），失败返回空 QPixmap。
QPixmap loadScaled(const QString& path, int maxEdge);

// 列表缩略图按 88px 显示，取 2 倍尺寸兼顾高分屏（带缓存）。
QPixmap loadThumbnail(const QString& path);

// 搜索结果小图标按 48px 显示，同样取 2 倍尺寸（带缓存）。
QPixmap loadSearchIcon(const QString& path);

// 大图预览限制在 2048px 内，足够覆盖常见窗口尺寸（不缓存）。
QPixmap loadPreview(const QString& path);

// 清空缩略图缓存（例如导入新数据库、照片目录变化后调用）。
void clearCache();

} // namespace PhotoImageUtils
