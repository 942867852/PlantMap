#pragma once

#include <QDialog>

#include "speciesinfo.h"

class TaxonomyDocument;
class SpeciesForm;
class QLabel;

/*
 * 资料编辑窗口：把 SpeciesForm 包进一个带“保存并关闭 / 取消”按钮的对话框。
 * 点右上角 X 或“取消”时，若有未保存改动则先询问；确认放弃后回滚并清理
 * 本会话复制进 photos/ 的孤儿文件。
 */
class SpeciesEditDialog : public QDialog
{
    Q_OBJECT

public:
    SpeciesEditDialog(TaxonomyDocument* document,
                      const QString& dataDir,
                      int nodeId,
                      QWidget* parent = nullptr);

protected:
    void reject() override;

private:
    bool hasDiscardableChanges() const;
    bool rollback(QString* error);

    TaxonomyDocument* m_document = nullptr;
    QString m_dataDir;
    int m_nodeId = 0;
    bool m_originalHasInfo = false;
    SpeciesInfo m_originalInfo;
    SpeciesForm* m_form = nullptr;
};
