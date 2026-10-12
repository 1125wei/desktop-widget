#pragma once

#include <QObject>
#include <QString>

class QQuickView;
class SettingsBackend;
class WidgetManager;

// 设置窗口（QML: qml/SettingsWindow.qml）—— 无边框深色窗口，自己画标题栏和关闭按钮。
// 预览区用 Settings.previewCard（当前选中卡片的后端）+ 同一个 CardSlot 组件渲染，
// 所以预览与桌面组件是**同一份代码**，不存在"预览和实际不一样"。
class SettingsWindow : public QObject {
    Q_OBJECT
public:
    SettingsWindow(SettingsBackend* sb, WidgetManager* mgr, QObject* parent = nullptr);

    void show();          // 首次创建，之后复用（再点"设置…"就 raise）
    bool isOpen() const;
    bool grabTo(const QString& path) const; // 自检用：抓窗口自身渲染结果

private:
    void createView();

    SettingsBackend* m_sb = nullptr;
    WidgetManager* m_mgr = nullptr;
    QQuickView* m_view = nullptr;
};
