#include "displayformat.h"

#include <QStringList>
#include <QtGlobal>

namespace DisplayFormat {

QString joinMonths(const QVector<int>& months)
{
    bool present[13] = {};
    for (int month : months) {
        if (month >= 1 && month <= 12)
            present[month] = true;
    }

    bool allMonths = true;
    for (int month = 1; month <= 12; ++month) {
        if (!present[month]) {
            allMonths = false;
            break;
        }
    }
    if (allMonths)
        return QStringLiteral("全年");

    QStringList parts;
    for (int start = 1; start <= 12; ++start) {
        if (!present[start])
            continue;
        const bool previousPresent =
            start == 1 ? present[12] : present[start - 1];
        if (previousPresent)
            continue;

        int end = start;
        while (present[(end % 12) + 1])
            ++end;

        if (end == start) {
            parts.append(QStringLiteral("%1月").arg(start));
        } else if (end > 12) {
            const int endMonth = (end % 12) == 0 ? 12 : (end % 12);
            parts.append(QStringLiteral("%1月-次年%2月")
                             .arg(start).arg(endMonth));
        } else {
            parts.append(QStringLiteral("%1月-%2月")
                             .arg(start).arg(end));
        }
        start = end;
    }

    return parts.isEmpty() ? QStringLiteral("未填写")
                           : parts.join(QStringLiteral("、"));
}

QString rangeText(int low, int high, const QString& suffix, int unknown)
{
    if (low == unknown && high == unknown)
        return QStringLiteral("未填写");
    if (low == unknown)
        return QStringLiteral("最高 %1%2").arg(high).arg(suffix);
    if (high == unknown)
        return QStringLiteral("最低 %1%2").arg(low).arg(suffix);
    return QStringLiteral("%1%2 ~ %3%2").arg(low).arg(suffix).arg(high);
}

double roundOneDecimal(double value)
{
    return qRound(value * 10.0) / 10.0;
}

QString formatOneDecimal(double value)
{
    return QString::number(value, 'f', 1);
}

} // namespace DisplayFormat
