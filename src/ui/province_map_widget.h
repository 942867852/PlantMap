#pragma once

#include <QWidget>
#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVector>

// 中国省级行政区示意图控件：
//   - 全图按固定经纬度纵横比绘制（窗口变化时整体等比缩放并居中，不变形）；
//   - 大陆 + 台湾 + 海南岛 + 南海诸岛（含十段线）都画在同一张图内；
//   - 支持点击省份高亮/取消（多选），用于编辑某植物的原生分布省区；
//   - 也可切换为只读展示模式。
//
// 合规说明：省界数据来自阿里云 DataV（高德底图），十段线来自民政部行政区划数据，
// 仅作程序内示意；正式发布请以自然资源部标准地图为准并标注审图号。
class ProvinceMapWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ProvinceMapWidget(QWidget* parent = nullptr);

    void setSelected(const QStringList& adcodes);
    QStringList selected() const;
    void setEditable(bool editable);
    bool isEditable() const;

    QSize sizeHint() const override;

signals:
    void selectionChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void rebuildPaths();
    QString adcodeAt(const QPointF& pos) const;

    QStringList m_selected;
    bool m_editable = true;
    QString m_hoverAdcode;

    QVector<QPainterPath> m_provincePaths;
    QVector<QString> m_provinceAdcodes;
    QVector<QString> m_provinceNames;
    // 每个省“面积最大的环”的经纬度包围盒（用于字号自适应）与质心（用于省名定位）。
    QVector<QRectF> m_labelRects;
    QVector<QPointF> m_labelCenters;
};
