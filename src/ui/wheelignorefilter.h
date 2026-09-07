#pragma once

#include <QEvent>
#include <QObject>

// 拦截鼠标滚轮事件，避免下拉框 / 数字输入框在悬停时被滚动误改。
class WheelIgnoreFilter : public QObject
{
public:
    explicit WheelIgnoreFilter(QObject* parent)
        : QObject(parent)
    {
    }

    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::Wheel)
            return true;
        return QObject::eventFilter(watched, event);
    }
};
