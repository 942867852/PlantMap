#pragma once

#include <QDialog>

class TaxonomyDocument;
class QLabel;

/*
 * 统计面板：展示植物库的整体统计信息。
 *   - 总览：分类节点数、物种（承载资料）数
 *   - 等级分布：各分类等级节点数量
 *   - 生长习性分布：乔木/灌木/草本/藤本… 各有多少种
 *   - 省区分布：原生分布省份 Top 榜
 */
class StatsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit StatsDialog(const TaxonomyDocument* document,
                         QWidget* parent = nullptr);

private:
    QWidget* buildOverview();
    QWidget* buildRankDistribution();
    QWidget* buildHabitDistribution();
    QWidget* buildRegionDistribution();

    const TaxonomyDocument* m_document = nullptr;
};
