#pragma once

#include <QDialog>
#include <QStringList>

class QLabel;

// 全屏照片灯箱：接收一组照片（photos/ 下的相对文件名），
// 支持左右翻页（按钮 / 方向键）与 Esc 关闭，图片按窗口等比缩放。
class PhotoLightBox : public QDialog
{
    Q_OBJECT

public:
    PhotoLightBox(const QStringList& photos, const QString& dataDir,
                  int startIndex = 0, QWidget* parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void showCurrent();
    void showPrev();
    void showNext();
    QString resolvePath(const QString& relativeName) const;

    QStringList m_photos;
    QString m_dataDir;
    int m_index = 0;
    QLabel* m_imageLabel = nullptr;
    QLabel* m_counterLabel = nullptr;
};
