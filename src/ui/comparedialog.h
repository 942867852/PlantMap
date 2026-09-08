#pragma once

#include <QDialog>
#include <QList>

class TaxonomyDocument;

/*
 * 物种对比：把多个“种/亚种/变种/变型/品种”的关键属性并排成表格，
 * 行是属性名，列是植物。支持 2~5 个植物对比。
 */
class CompareDialog : public QDialog
{
    Q_OBJECT

public:
    CompareDialog(const TaxonomyDocument* document,
                  const QList<int>& nodeIds,
                  QWidget* parent = nullptr);
};
