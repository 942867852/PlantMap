#pragma once

#include <QString>
#include <QVector>

// 与植物属性展示相关的纯格式化函数，放在 core 层便于自检。
namespace DisplayFormat {

// 月份集合的区间文本；空集合返回“未填写”。
QString joinMonths(const QVector<int>& months);

// 数值范围文本；unknown 表示“未填写”的单端哨兵。
QString rangeText(int low, int high, const QString& suffix, int unknown = 0);

// 浮点数值范围文本（固定 1 位小数），用于 pH 等；0.0 视为“未填写”。
// 输出形如 “未填写” / “最高 8.5” / “最低 6.0” / “6.0 ~ 8.5”。
QString rangeTextDouble(double low, double high, const QString& suffix);

// 数值统一保留 1 位小数（例如 pH）。
double roundOneDecimal(double value);

// 输出固定 1 位小数的文本。
QString formatOneDecimal(double value);

} // namespace DisplayFormat
