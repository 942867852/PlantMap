#pragma once

#include <QString>

#include "pinyin_first.h"

// 中文名拼音首字母检索工具：把中文按每个汉字的拼音首字母拼成字符串，
// 例如“银杏” → “YX”、“牡丹” → “MD”。非 CJK 汉字（字母/数字/符号）直接跳过，
// 便于用户输入“ys”“md”等首字母进行模糊匹配。
namespace Pinyin {

inline QString initials(const QString& text)
{
    QString result;
    for (const QChar& ch : text) {
        const uint uc = ch.unicode();
        if (uc < 0x4E00 || uc > 0x9FFF)
            continue;
        const uint offset = uc - 0x4E00;
        const char c = kCjkPinyinFirstLetters[offset / 100][offset % 100];
        if (c != ' ')
            result.append(QLatin1Char(c));
    }
    return result;
}

// 判断 query 是否是 text 的“拼音首字母前缀”匹配：
// 例如 initials("银杏")="YX"，query="y" / "yx" 都能匹配。
inline bool matchesInitials(const QString& text, const QString& query)
{
    const QString q = query.trimmed().toUpper();
    if (q.isEmpty())
        return false;
    const QString initialsUpper = initials(text);
    return initialsUpper.startsWith(q) || initialsUpper.contains(q);
}

} // namespace Pinyin
