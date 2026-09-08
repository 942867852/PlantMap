#pragma once

#include <QString>

class TaxonomyDocument;

/*
 * 把植物库导出为 CSV 表格。
 *
 * 设计原则：属性字段以后还会变，所以导出列分两类——
 *   - 核心固定列：等级 / 中文名 / 拉丁学名 / 别名 / 描述，几乎不会变；
 *   - 属性列：从 SpeciesInfo 的枚举字段 + custom 扩展属性表自动推导，
 *     新增属性时导出会自动跟着变，无需改这里。
 *
 * 只导出“种 / 亚种 / 变种 / 变型 / 品种”这类承载资料的节点。
 */
namespace CsvExport {

// 生成 CSV 文本（UTF-8，带 BOM 便于 Excel 正确识别中文）。
// 返回 true 表示成功；exportError 非空时写入失败原因。
bool generate(const TaxonomyDocument& doc, QString* csv, QString* error);

} // namespace CsvExport
