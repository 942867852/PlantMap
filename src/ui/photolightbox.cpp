#include "photolightbox.h"

#include "photoimageutils.h"
#include "speciesinfo.h"  // isSafePhotoFileName

#include <QDir>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

PhotoLightBox::PhotoLightBox(const QStringList& photos, const QString& dataDir,
                             int startIndex, QWidget* parent)
    : QDialog(parent)
    , m_photos(photos)
    , m_dataDir(dataDir)
{
    setWindowTitle(QStringLiteral("照片浏览"));
    setWindowFlags(windowFlags() | Qt::Window);
    resize(960, 720);
    setStyleSheet(QStringLiteral("QDialog { background: #202020; }"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);

    m_imageLabel = new QLabel(this);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setMinimumSize(400, 300);
    layout->addWidget(m_imageLabel, 1);

    m_counterLabel = new QLabel(this);
    m_counterLabel->setAlignment(Qt::AlignCenter);
    m_counterLabel->setStyleSheet(QStringLiteral("color: #cccccc; font-size: 14px;"));
    layout->addWidget(m_counterLabel);

    auto* nav = new QHBoxLayout;
    auto* prev = new QPushButton(QStringLiteral("‹ 上一张"), this);
    auto* next = new QPushButton(QStringLiteral("下一张 ›"), this);
    auto* close = new QPushButton(QStringLiteral("关闭 (Esc)"), this);
    prev->setMinimumHeight(34);
    next->setMinimumHeight(34);
    close->setMinimumHeight(34);
    nav->addWidget(prev);
    nav->addStretch();
    nav->addWidget(close);
    nav->addStretch();
    nav->addWidget(next);
    layout->addLayout(nav);

    connect(prev, &QPushButton::clicked, this, &PhotoLightBox::showPrev);
    connect(next, &QPushButton::clicked, this, &PhotoLightBox::showNext);
    connect(close, &QPushButton::clicked, this, &QDialog::reject);

    m_index = (startIndex >= 0 && startIndex < m_photos.size())
        ? startIndex : 0;
    showCurrent();
}

QString PhotoLightBox::resolvePath(const QString& relativeName) const
{
    if (!isSafePhotoFileName(relativeName))
        return QString();
    return QDir(m_dataDir).filePath(QStringLiteral("photos") + QLatin1Char('/') + relativeName);
}

void PhotoLightBox::showCurrent()
{
    if (m_photos.isEmpty()) {
        m_imageLabel->setText(QStringLiteral("没有照片"));
        m_counterLabel->setText(QString());
        return;
    }
    if (m_index < 0)
        m_index = m_photos.size() - 1;
    if (m_index >= m_photos.size())
        m_index = 0;

    const QPixmap pixmap = PhotoImageUtils::loadPreview(resolvePath(m_photos.at(m_index)));
    if (pixmap.isNull()) {
        m_imageLabel->setText(QStringLiteral("图片文件缺失：\n%1").arg(m_photos.at(m_index)));
    } else {
        m_imageLabel->setPixmap(pixmap);
    }
    m_counterLabel->setText(QStringLiteral("%1 / %2").arg(m_index + 1).arg(m_photos.size()));
}

void PhotoLightBox::showPrev()
{
    --m_index;
    showCurrent();
}

void PhotoLightBox::showNext()
{
    ++m_index;
    showCurrent();
}

void PhotoLightBox::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);
    if (!m_imageLabel || m_photos.isEmpty() || m_index < 0 || m_index >= m_photos.size())
        return;
    const QPixmap pixmap = PhotoImageUtils::loadPreview(resolvePath(m_photos.at(m_index)));
    if (pixmap.isNull())
        return;
    QSize area = m_imageLabel->contentsRect().size();
    if (area.width() <= 0 || area.height() <= 0)
        area = m_imageLabel->size();
    m_imageLabel->setPixmap(pixmap.scaled(area, Qt::KeepAspectRatio,
                                          Qt::SmoothTransformation));
}

void PhotoLightBox::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Left:
        showPrev();
        return;
    case Qt::Key_Right:
        showNext();
        return;
    case Qt::Key_Escape:
        reject();
        return;
    default:
        break;
    }
    QDialog::keyPressEvent(event);
}
