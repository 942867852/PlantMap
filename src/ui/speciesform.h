#pragma once

#include <QWidget>
#include <QVector>

#include "taxondocument.h"

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QListWidget;
class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QPushButton;
class QTableWidget;
class QStackedWidget;

// 单个“种 / 亚种”节点的资料编辑表单。
// 负责把 SpeciesInfo 显示成控件，并在“保存”时回写 TaxonomyDocument。
class SpeciesForm : public QWidget
{
    Q_OBJECT

public:
    explicit SpeciesForm(QWidget* parent = nullptr);

    void setDocument(TaxonomyDocument* document);
    void setDataDir(const QString& path);

    // id <= 0 时清空；非种/亚种节点显示提示页
    void showNode(int id);
    int currentNodeId() const { return m_nodeId; }

signals:
    void infoSaved();
    void infoSaveError(const QString& message);

private slots:
    void requestSave();
    void addPhotos();
    void removeSelectedPhotos();
    void openSelectedPhoto();
    void addCustomProperty();
    void removeCustomProperty();

private:
    QWidget* buildFormPage();
    QWidget* buildHintPage();
    void markEdited();

    bool collectFromForm(SpeciesInfo& out, QString& error) const;
    void populateFromInfo(const SpeciesInfo& info);
    void refreshPhotoList(const SpeciesInfo& info);
    QString resolvePhotoPath(const QString& relativeName) const;
    void setCheckSet(const QSet<QString>& keys, const QVector<QCheckBox*>& checks,
                     const QStringList& keyOrder);
    void setMonths(const QVector<int>& months, const QVector<QCheckBox*>& checks);

    TaxonomyDocument* m_document = nullptr;
    QString m_dataDir;
    int m_nodeId = 0;
    bool m_loading = false;

    QStackedWidget* m_stack = nullptr;
    QLabel* m_hintLabel = nullptr;
    QLabel* m_pathLabel = nullptr;

    // 身份与描述
    QLineEdit* m_latinEdit = nullptr;
    QLineEdit* m_aliasEdit = nullptr;
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
