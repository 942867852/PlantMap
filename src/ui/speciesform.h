#pragma once

#include <QPixmap>
#include <QWidget>
#include <QVector>

#include "taxondocument.h"

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QListWidget;
class QListWidgetItem;
class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QPushButton;
class QTableWidget;
class QStackedWidget;
class QScrollArea;
class QResizeEvent;
class QTimer;
class ProvinceMapWidget;

// 单个“种 / 亚种”节点的资料编辑表单（在独立编辑窗口中打开）。
// 负责把 SpeciesInfo 显示成控件，并在“保存”时回写 TaxonomyDocument。
class SpeciesForm : public QWidget
{
    Q_OBJECT

public:
    explicit SpeciesForm(QWidget* parent = nullptr);

    void setDocument(TaxonomyDocument* document);
    void setDataDir(const QString& path);
    // 把当前表单内容保存到文档；成功返回 true，失败弹窗提示并返回 false。
    bool requestSave();
    // 与文档中已保存的资料对比，判断当前表单是否有未保存改动。
    bool hasUnsavedChanges() const;

    // id <= 0 时清空；非种/亚种节点显示提示页
    void showNode(int id);
    int currentNodeId() const { return m_nodeId; }
    // 本次编辑会话中新复制到 photos/ 的文件（用于取消/保存时清理孤儿文件）。
    QStringList sessionCopiedPhotoFiles() const;
    // 删除当前节点的草稿文件（取消编辑时调用，避免残留过期草稿）。
    void discardDraft();

signals:
    void infoSaved();
    void infoSaveError(const QString& message);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void addAlias();
    void removeSelectedAlias();
    void addPhotos();
    void removeSelectedPhotos();
    void openSelectedPhoto();
    void addCustomProperty();
    void removeCustomProperty();
    void onPhotoSelectionChanged(QListWidgetItem* current, QListWidgetItem* previous);

private:
    QWidget* buildFormPage();
    QWidget* buildHintPage();
    void setPhotoPreview(const QString& relativeName);
    void updatePhotoPreview();
    void resizeEvent(QResizeEvent* event) override;

    bool collectFromForm(SpeciesInfo& out, QString& error) const;
    void populateFromInfo(const SpeciesInfo& info);
    void refreshPhotoList(const SpeciesInfo& info);
    QListWidgetItem* buildPhotoItem(const QString& relativeName) const;
    QString resolvePhotoPath(const QString& relativeName) const;
    // 把一批本地图片文件复制进照片目录并加入表单列表（拖拽/粘贴/文件对话框共用）。
    void addPhotosFromFiles(const QStringList& files);
    // 草稿：把当前表单内容写入草稿文件 / 删除草稿文件。
    void saveDraft();
    void clearDraft();
    QString draftPath() const;
    void setCheckSet(const QSet<QString>& keys, const QVector<QCheckBox*>& checks,
                     const QStringList& keyOrder);
    void setMonths(const QVector<int>& months, const QVector<QCheckBox*>& checks);

    TaxonomyDocument* m_document = nullptr;
    QString m_dataDir;
    QStringList m_copiedPhotosThisSession;
    // 进入本次编辑会话时资料中已引用的照片（保存时据此清理被移除的文件）。
    QStringList m_photosAtSessionStart;
    int m_nodeId = 0;
    bool m_loading = false;

    QStackedWidget* m_stack = nullptr;
    QScrollArea* m_formScroll = nullptr;
    QLabel* m_hintLabel = nullptr;
    QLabel* m_pathLabel = nullptr;
    QLabel* m_photoPreview = nullptr;
    QPixmap m_photoPreviewOriginal;
    QTimer* m_draftTimer = nullptr;

    // 身份与描述
    QLineEdit* m_latinEdit = nullptr;
    QListWidget* m_aliasList = nullptr;
    QPlainTextEdit* m_descriptionEdit = nullptr;
    QListWidget* m_photoList = nullptr;

    // 环境需求
    QComboBox* m_lightCombo = nullptr;
    QComboBox* m_waterCombo = nullptr;
    QSpinBox* m_tempMin = nullptr;
    QSpinBox* m_tempMax = nullptr;
    QSpinBox* m_humidityMin = nullptr;
    QSpinBox* m_humidityMax = nullptr;
    QDoubleSpinBox* m_phMin = nullptr;
    QDoubleSpinBox* m_phMax = nullptr;
    QSpinBox* m_zoneLow = nullptr;
    QSpinBox* m_zoneHigh = nullptr;
    QVector<QCheckBox*> m_soilChecks;

    // 地理分布
    ProvinceMapWidget* m_mapWidget = nullptr;
    QLineEdit* m_habitatEdit = nullptr;

    // 生长形态
    QComboBox* m_habitCombo = nullptr;
    QComboBox* m_lifecycleCombo = nullptr;
    QComboBox* m_foliageCombo = nullptr;
    QComboBox* m_growthRateCombo = nullptr;
    QSpinBox* m_heightMin = nullptr;
    QSpinBox* m_heightMax = nullptr;
    QSpinBox* m_spreadMin = nullptr;
    QSpinBox* m_spreadMax = nullptr;
    QVector<QCheckBox*> m_propagationChecks;
    QVector<QCheckBox*> m_usageChecks;

    // 物候
    QVector<QCheckBox*> m_bloomChecks;
    QVector<QCheckBox*> m_fruitChecks;

    // 扩展属性
    QTableWidget* m_customTable = nullptr;
};
