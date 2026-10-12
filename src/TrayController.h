#pragma once

#include <QMenu>
#include <QObject>
#include <QSignalBlocker>
#include <QSystemTrayIcon>

class ConfigStore;
class QAction;
class QMenu;
class SettingsWindow;
class WidgetManager;

// 托盘常驻 + 菜单：桌面组件的唯一交互入口
// （挂在图标层之下时鼠标点击会被桌面图标层吃掉，所以一切操作都从这里走）
class TrayController : public QObject {
    Q_OBJECT
public:
    TrayController(WidgetManager* mgr, ConfigStore* cfg, SettingsWindow* settings, QObject* parent = nullptr);

    void setup();                    // 建托盘与菜单
    void setVisibleChecked(bool on); // 同步「显示组件」勾选态

private slots:
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void onToggleVisible(bool on);
    void onToggleEdit(bool on);
    void onSettings();
    void onOpenConfigFolder();
    void onRemount();
    void onResetPosition();
    void onAddCard();
    void onToggleAutoStart(bool on);
    void onQuit();

private:
    bool autoStartEnabled() const;
    void setAutoStart(bool on);

    WidgetManager* m_mgr = nullptr;
    ConfigStore* m_cfg = nullptr;
    SettingsWindow* m_settings = nullptr;
    QSystemTrayIcon* m_tray = nullptr;
    QMenu* m_menu = nullptr;
    QAction* m_actVisible = nullptr;
    QAction* m_actEdit = nullptr;
    QAction* m_actAutoStart = nullptr;
};
