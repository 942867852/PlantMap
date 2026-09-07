#pragma once

#include <QString>
#include <QVector>

// 与植物属性展示相关的纯格式化函数，放在 core 层便于自检。
namespace DisplayFormat {

// 月份集合的区间文本；空集合返回“未填写”。
QString joinMonths(const QVector<int>& months);

// 数值范围文本；unknown 表示“未填写”的单端哨兵。
QString rangeText(int low, int high, const QString& suffix, int unknown = 0);

// 数值统一保留 1 位小数（例如 pH）。
double roundOneDecimal(double value);

// 输出固定 1 位小数的文本。
QString formatOneDecimal(double value);

} // namespace DisplayFormat
