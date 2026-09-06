#pragma once

#include <QPixmap>
#include <QWidget>

#include "taxondocument.h"

class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QScrollArea;
class QStackedWidget;
class QTableWidget;
class QResizeEvent;

/*
 * 物种资料“查看页”：左侧点击植物后，这里以只读方式展示图片和全部属性。
 * 想修改时点击“编辑资料”，由主窗口打开独立编辑窗口。
 */
class SpeciesViewForm : public QWidget
{
    Q_OBJECT

public:
    explicit SpeciesViewForm(QWidget* parent = nullptr);

    void setDocument(TaxonomyDocument* document);
    void setDataDir(const QString& path);

    // id <= 0 时显示提示；非种/亚种显示说明；种/亚种显示只读资料。
    void showNode(int id);

signals:
    void editRequested(int nodeId);

private slots:
    void onPhotoSelectionChanged(QListWidgetItem* current,
                                 QListWidgetItem* previous);

private:
    QWidget* buildEmptyPage();
    QWidget* buildDetailPage();
    void refreshDetail();
    void refreshPhotoList(const QStringList& photos);
    void setPhotoPreview(const QString& relativeName);
    void updatePhotoPreview();
    void resizeEvent(QResizeEvent* event) override;
    QString resolvePhotoPath(const QString& relativeName) const;
    QLabel* makeValueLabel(QWidget* parent) const;

    TaxonomyDocument* m_document = nullptr;
    QString m_dataDir;
    int m_nodeId = 0;

    QStackedWidget* m_stack = nullptr;
    QScrollArea* m_detailScroll = nullptr;
    QLabel* m_hintLabel = nullptr;
    QLabel* m_pathLabel = nullptr;
    QPushButton* m_editButton = nullptr;

    // 照片展示
    QLabel* m_photoPreview = nullptr;
    QPixmap m_photoPreviewOriginal;
    QListWidget* m_photoList = nullptr;

    // 基本信息
    QLabel* m_nameValue = nullptr;
    QLabel* m_rankValue = nullptr;
    QLabel* m_latinValue = nullptr;
    QLabel* m_aliasValue = nullptr;
    QLabel* m_descriptionValue = nullptr;

    // 环境需求
    QLabel* m_lightValue = nullptr;
    QLabel* m_waterValue = nullptr;
    QLabel* m_temperatureValue = nullptr;
    QLabel* m_humidityValue = nullptr;
    QLabel* m_phValue = nullptr;
    QLabel* m_soilValue = nullptr;
    QLabel* m_zoneValue = nullptr;

    // 生长形态
    QLabel* m_habitValue = nullptr;
    QLabel* m_lifecycleValue = nullptr;
    QLabel* m_foliageValue = nullptr;
    QLabel* m_growthRateValue = nullptr;
    QLabel* m_heightValue = nullptr;
    QLabel* m_spreadValue = nullptr;
    QLabel* m_propagationValue = nullptr;
    QLabel* m_usageValue = nullptr;

    // 物候
    QLabel* m_bloomValue = nullptr;
    QLabel* m_fruitValue = nullptr;

    // 扩展属性
    QTableWidget* m_customTable = nullptr;
};
