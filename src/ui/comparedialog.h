#pragma once

#include <QDialog>
#include <QList>

class TaxonomyDocument;

/*
 * 物种对比：把对比列表中的“种/亚种/变种/变型/品种”关键属性并排成表格，
 * 行是属性名，列是植物。支持在对话框内移除某个植物（即时刷新）。
 *
 * compareList 是指向主窗口对比列表的可变指针：删除某列时会同步修改该列表，
 * 对话框关闭后主窗口据此更新“加入对比”按钮状态。
 */
class CompareDialog : public QDialog
{
    Q_OBJECT

public:
    CompareDialog(const TaxonomyDocument* document,
                  QList<int>* compareList,
                  QWidget* parent = nullptr);

private:
    void rebuildTable();

    const TaxonomyDocument* m_document = nullptr;
    QList<int>* m_compareList = nullptr;
};
