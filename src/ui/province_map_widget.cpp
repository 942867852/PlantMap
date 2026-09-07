#include "province_map_widget.h"

#include "china_map_data.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QTransform>

#include <algorithm>

namespace {

constexpr double kLonScale = 0.82;  // cos(35°)，修正高纬度横向比例

// 全图经纬度范围：东至乌苏里江、西至帕米尔、北至漠河、南至曾母暗沙（含南海诸岛）。
constexpr double kMinLon = 73.0, kMaxLon = 135.5;
constexpr double kMinLat = 3.0, kMaxLat = 54.0;

// 经纬度 → 屏幕：统一比例因子（保持正确纵横比），整体等比缩放并居中，不拉伸变形。
QTransform mapTransform(const QRectF& rect, double minLon, double maxLon,
                        double minLat, double maxLat)
{
    const double geoW = (maxLon - minLon) * kLonScale;
    const double geoH = (maxLat - minLat);
    const double scale = std::min(rect.width() / geoW, rect.height() / geoH);
    const double drawW = geoW * scale;
    const double drawH = geoH * scale;
    const double ox = rect.left() + (rect.width() - drawW) / 2.0;
    const double oy = rect.top() + (rect.height() - drawH) / 2.0;
    QTransform t;
    t.translate(ox, oy + drawH);
    t.scale(scale * kLonScale, -scale);
    t.translate(-minLon, -minLat);
    return t;
}

// 省名简称：去掉“省/市/自治区/特别行政区”等后缀，避免小省被长名字遮挡。
QString provinceShortName(const QString& name)
{
    static const QStringList suffixes = {
        QStringLiteral("特别行政区"),
        QStringLiteral("维吾尔自治区"),
        QStringLiteral("壮族自治区"),
        QStringLiteral("回族自治区"),
        QStringLiteral("自治区"),
        QStringLiteral("省"),
        QStringLiteral("市"),
    };
    for (const QString& suffix : suffixes) {
        if (name.endsWith(suffix))
            return name.left(name.size() - suffix.size());
    }
    return name;
}

// 多边形质心（shoelace 公式），比包围盒中心更能代表不规则省的位置。
QPointF polygonCentroid(const QPolygonF& poly)
{
    const int n = poly.size();
    if (n == 0)
        return QPointF();
    double area = 0.0, cx = 0.0, cy = 0.0;
    for (int i = 0; i < n; ++i) {
        const QPointF& p0 = poly.at(i);
        const QPointF& p1 = poly.at((i + 1) % n);
        const double cross = p0.x() * p1.y() - p1.x() * p0.y();
        area += cross;
        cx += (p0.x() + p1.x()) * cross;
        cy += (p0.y() + p1.y()) * cross;
    }
    area *= 0.5;
    if (qAbs(area) < 1e-9)
        return poly.boundingRect().center();
    return QPointF(cx / (6.0 * area), cy / (6.0 * area));
}

} // namespace

ProvinceMapWidget::ProvinceMapWidget(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumSize(300, 300);

    // 用经纬度坐标（x=经度, y=纬度）构建省界路径，尺寸无关，构建一次即可。
    const QVector<ChinaProvinceShape> shapes = chinaProvinceShapes();
    for (const ChinaProvinceShape& shape : shapes) {
        QPainterPath path;
        path.setFillRule(Qt::OddEvenFill);
        QRectF labelRect;
        QPointF labelCenter;
        double maxArea = -1.0;
        for (const QVector<QPoint>& ring : shape.rings) {
            QPolygonF poly;
            for (const QPoint& p : ring)
                poly << QPointF(p.x() / 100.0, p.y() / 100.0);
            if (poly.isEmpty())
                continue;
            path.addPolygon(poly);
            path.closeSubpath();
            const QRectF br = poly.boundingRect();
            const double area = br.width() * br.height();
            if (area > maxArea) {
                maxArea = area;
                labelRect = br;
                labelCenter = polygonCentroid(poly);
            }
        }
        m_provincePaths.append(path);
        m_provinceAdcodes.append(shape.adcode);
        m_provinceNames.append(shape.name);
        m_labelRects.append(labelRect);
        m_labelCenters.append(labelCenter);
    }
}

QSize ProvinceMapWidget::sizeHint() const
{
    return QSize(460, 460);
}

void ProvinceMapWidget::setSelected(const QStringList& adcodes)
{
    m_selected = adcodes;
    update();
}

QStringList ProvinceMapWidget::selected() const
{
    return m_selected;
}

void ProvinceMapWidget::setEditable(bool editable)
{
    m_editable = editable;
    if (!editable) {
        m_hoverAdcode.clear();
        setCursor(Qt::ArrowCursor);
    }
    update();
}

bool ProvinceMapWidget::isEditable() const
{
    return m_editable;
}

QString ProvinceMapWidget::adcodeAt(const QPointF& pos) const
{
    const QRectF rect = this->rect();
    const QTransform t = mapTransform(rect, kMinLon, kMaxLon, kMinLat, kMaxLat);
    bool ok = false;
    const QPointF lonLat = t.inverted(&ok).map(pos);
    if (!ok)
        return QString();
    for (int i = m_provincePaths.size() - 1; i >= 0; --i) {
        if (m_provincePaths.at(i).contains(lonLat))
            return m_provinceAdcodes.at(i);
    }
    return QString();
}

void ProvinceMapWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QColor fillDefault(0xea, 0xef, 0xf5);
    const QColor strokeDefault(0x8a, 0x94, 0xa6);
    const QColor fillSelected(0xf5, 0x8a, 0x4a);
    const QColor fillHover(0xc9, 0xd8, 0xe8);
    const QColor dashColor(0x2b, 0x4b, 0x6f);

    const QRectF r = rect();
    const QTransform t = mapTransform(r, kMinLon, kMaxLon, kMinLat, kMaxLat);

    // 各省（含台湾、海南岛、港澳，及海南省自带的南海诸岛小环）
    painter.save();
    painter.setTransform(t, true);
    painter.setPen(QPen(strokeDefault, 0.15));
    for (int i = 0; i < m_provincePaths.size(); ++i) {
        const QString& adcode = m_provinceAdcodes.at(i);
        QColor fill = fillDefault;
        if (m_selected.contains(adcode))
            fill = fillSelected;
        else if (m_hoverAdcode == adcode && m_editable)
            fill = fillHover;
        painter.setBrush(fill);
        painter.drawPath(m_provincePaths.at(i));
    }

    // 南海诸岛主要岛礁（增强可视性）
    painter.setPen(QPen(QColor(0x7a, 0x84, 0x96), 0.15));
    painter.setBrush(QColor(0xc9, 0xd5, 0xe0));
    for (const QVector<QPoint>& ring : southChinaSeaIslands()) {
        QPolygonF poly;
        for (const QPoint& p : ring)
            poly << QPointF(p.x() / 100.0, p.y() / 100.0);
        if (!poly.isEmpty())
            painter.drawPolygon(poly);
    }

    // 十段线（南海断续线）
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(dashColor, 0.2, Qt::DashLine));
    for (const QVector<QPoint>& seg : nineDashLines()) {
        QPolygonF poly;
        for (const QPoint& p : seg)
            poly << QPointF(p.x() / 100.0, p.y() / 100.0);
        if (poly.size() >= 2)
            painter.drawPolyline(poly);
    }
    painter.restore();

    // 省名标注：定位在“面积最大的环”的质心上，字号随省大小自适应。
    painter.setPen(QColor(0x40, 0x40, 0x40));
    for (int i = 0; i < m_provincePaths.size(); ++i) {
        const QRectF br = t.mapRect(m_labelRects.at(i));
        if (br.width() < 10 || br.height() < 8)
            continue;  // 港澳等极小区域不标，避免文字溢出
        const QPointF center = t.map(m_labelCenters.at(i));
        const QString label = provinceShortName(m_provinceNames.at(i));
        int fontSize = 7;
        if (br.width() < 40 || br.height() < 30)
            fontSize = 6;
        if (br.width() < 26 || br.height() < 20)
            fontSize = 5;
        painter.setFont(QFont(painter.font().family(), fontSize));
        painter.drawText(QRectF(center.x() - 60, center.y() - 12, 120, 24),
                         Qt::AlignCenter, label);
    }

    // 南海诸岛文字
    const QPointF nanhaiPos = t.map(QPointF(113.0, 13.0));
    painter.setPen(QPen(QColor(0x2b, 0x4b, 0x6f), 0.8));
    painter.setFont(QFont(painter.font().family(), 7));
    painter.drawText(QRectF(nanhaiPos.x() - 40, nanhaiPos.y() - 8, 80, 16),
                     Qt::AlignCenter, QStringLiteral("南海诸岛"));
}

void ProvinceMapWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_editable) {
        const QString adcode = adcodeAt(event->position());
        if (adcode != m_hoverAdcode) {
            m_hoverAdcode = adcode;
            setCursor(adcode.isEmpty() ? Qt::ArrowCursor : Qt::PointingHandCursor);
            update();
        }
    }
    QWidget::mouseMoveEvent(event);
}

void ProvinceMapWidget::leaveEvent(QEvent* event)
{
    m_hoverAdcode.clear();
    update();
    QWidget::leaveEvent(event);
}

void ProvinceMapWidget::mousePressEvent(QMouseEvent* event)
{
    if (!m_editable || event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    const QString adcode = adcodeAt(event->position());
    if (adcode.isEmpty())
        return;

    if (m_selected.contains(adcode))
        m_selected.removeAll(adcode);
    else
        m_selected.append(adcode);
    update();
    emit selectionChanged();
}
