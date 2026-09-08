#include "specieseditdialog.h"

#include "speciesform.h"
#include "taxondocument.h"

#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QVBoxLayout>

SpeciesEditDialog::SpeciesEditDialog(TaxonomyDocument* document,
                                     const QString& dataDir,
                                     int nodeId,
                                     QWidget* parent)
    : QDialog(parent)
{
    m_document = document;
    m_dataDir = dataDir;
    m_nodeId = nodeId;

    const TaxonNode* n = document->node(nodeId);
    m_originalHasInfo = n && n->hasInfo;
    if (m_originalHasInfo)
        m_originalInfo = n->info;

    setWindowTitle(QStringLiteral("编辑资料：%1（%2）")
                       .arg(n ? n->name : QString(),
                            n ? TaxonRanks::displayName(n->rank)
                              : QString()));
    resize(760, 780);

    auto* layout = new QVBoxLayout(this);
    m_form = new SpeciesForm(this);
    m_form->setDocument(document);
    m_form->setDataDir(dataDir);
    m_form->showNode(nodeId);
    layout->addWidget(m_form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save
                                             | QDialogButtonBox::Cancel,
                                         this);
    QPushButton* saveButton = buttons->button(QDialogButtonBox::Save);
    saveButton->setText(QStringLiteral("保存并关闭"));
    buttons->button(QDialogButtonBox::Cancel)
        ->setText(QStringLiteral("取消"));
    connect(saveButton, &QPushButton::clicked, this, [this] {
        if (m_form->requestSave())
            accept();
    });
    connect(buttons->button(QDialogButtonBox::Cancel),
            &QPushButton::clicked, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void SpeciesEditDialog::reject()
{
    if (hasDiscardableChanges()) {
        const QMessageBox::StandardButton answer =
            QMessageBox::warning(
                this, QStringLiteral("有未保存的修改"),
                QStringLiteral("当前编辑的内容或照片操作还没有保存。\n\n"
                               "确定要放弃本次所有修改并关闭吗？"
                               "（新添加的照片也会被删除）"),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;

        QString error;
        if (!rollback(&error)) {
            QMessageBox::critical(
                this, QStringLiteral("无法还原"),
                QStringLiteral("还原失败，为避免数据丢失窗口不会关闭。\n\n%1")
                    .arg(error));
            return;
        }
    }

    // 即使当前表单与数据库一致（例如“添加后又移除”了某张照片），
    // 本会话复制进 photos/ 的文件也不能留下，否则会成为孤儿文件。
    //
    // 说明本清理与“保存成功”互斥，不会误删在用的照片：
    //   - 保存成功走 requestSave() -> accept()，此时
    //     sessionCopiedPhotoFiles() 已缩小为“仍被引用”的集合，
    //     但窗口直接关闭，不会再进入本 reject()。
    //   - 只有真正放弃修改时才会走到这里，此刻 session 里残留的
    //     都是“复制了但最终没被资料引用”的文件，删掉是安全的。
    QStringList failedToRemove;
    for (const QString& relativeName
         : m_form->sessionCopiedPhotoFiles()) {
        const QString fullPath = QDir(m_dataDir).filePath(
            QStringLiteral("photos") + QLatin1Char('/') + relativeName);
        if (QFile::exists(fullPath) && !QFile::remove(fullPath))
            failedToRemove.append(relativeName);
    }
    if (!failedToRemove.isEmpty()) {
        QMessageBox::warning(
            this, QStringLiteral("部分照片文件未能删除"),
            QStringLiteral("以下照片文件可能正被占用，无法删除：\n\n%1")
                .arg(failedToRemove.join(QLatin1Char('\n'))));
    }

    // 放弃修改后清除草稿文件，避免下次打开同一植物时再次提示“发现未保存草稿”。
    m_form->discardDraft();

    QDialog::reject();
}

bool SpeciesEditDialog::hasDiscardableChanges() const
{
    if (m_form->hasUnsavedChanges())
        return true;

    const TaxonNode* n = m_document->node(m_nodeId);
    const bool currentHasInfo = n && n->hasInfo;
    if (currentHasInfo != m_originalHasInfo)
        return true;

    const SpeciesInfo currentInfo =
        currentHasInfo ? n->info : SpeciesInfo();
    return !(currentInfo == m_originalInfo);
}

bool SpeciesEditDialog::rollback(QString* error)
{
    if (m_originalHasInfo) {
        return m_document->setInfo(m_nodeId, m_originalInfo, error);
    }
    return m_document->clearInfo(m_nodeId, error);
}
