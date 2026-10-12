#include "TrayController.h"

#include "AutoStart.h"
#include "ConfigStore.h"
#include "SettingsWindow.h"
#include "WidgetManager.h"

#include <QAction>
#include <QCoreApplication>
#include <QCursor>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QMenu>
#include <QMessageBox>
#include <QUrl>

TrayController::TrayController(WidgetManager* mgr, ConfigStore* cfg, SettingsWindow* settings,
                               QObject* parent)
    : QObject(parent)
    , m_mgr(mgr)
    , m_cfg(cfg)
    , m_settings(settings)
{
}

void TrayController::setup()
{
    m_tray = new QSystemTrayIcon(this);
    m_tray->setIcon(QIcon(QStringLiteral(":/assets/tray.ico")));
    m_tray->setToolTip(QStringLiteral("桌面组件"));

    m_menu = new QMenu();

    m_actVisible = m_menu->addAction(QStringLiteral("显示组件"));
    m_actVisible->setCheckable(true);
    m_actVisible->setChecked(true);
    connect(m_actVisible, &QAction::toggled, this, &TrayController::onToggleVisible);

    m_menu->addSeparator();

    m_actEdit = m_menu->addAction(QStringLiteral("编辑模式（可拖动）"));
    m_actEdit->setCheckable(true);
    connect(m_actEdit, &QAction::toggled, this, &TrayController::onToggleEdit);

    QAction* actRemount = m_menu->addAction(QStringLiteral("重新挂到桌面层"));
    connect(actRemount, &QAction::triggered, this, &TrayController::onRemount);

    QAction* actReset = m_menu->addAction(QStringLiteral("重置位置（回到右上角）"));
    connect(actReset, &QAction::triggered, this, &TrayController::onResetPosition);

    QMenu* addMenu = m_menu->addMenu(QStringLiteral("新增组件"));
    QAction* actAddCount = addMenu->addAction(QStringLiteral("下班倒计时"));
    connect(actAddCount, &QAction::triggered, this, [this]() {
        if (m_mgr)
            m_mgr->addCard(QStringLiteral("countdown"));
    });
    QAction* actAddAnniv = addMenu->addAction(QStringLiteral("纪念日"));
    connect(actAddAnniv, &QAction::triggered, this, [this]() {
        if (m_mgr)
            m_mgr->addCard(QStringLiteral("anniversary"));
    });

    QAction* actSettings = m_menu->addAction(QStringLiteral("设置…"));
    connect(actSettings, &QAction::triggered, this, &TrayController::onSettings);

    m_menu->addSeparator();

    m_actAutoStart = m_menu->addAction(QStringLiteral("开机自启"));
    m_actAutoStart->setCheckable(true);
    m_actAutoStart->setChecked(autoStartEnabled());
    connect(m_actAutoStart, &QAction::toggled, this, &TrayController::onToggleAutoStart);

    QAction* actFolder = m_menu->addAction(QStringLiteral("打开配置文件目录"));
    connect(actFolder, &QAction::triggered, this, &TrayController::onOpenConfigFolder);

    m_menu->addSeparator();

    QAction* actQuit = m_menu->addAction(QStringLiteral("退出"));
    connect(actQuit, &QAction::triggered, this, &TrayController::onQuit);

    m_tray->setContextMenu(m_menu);
    connect(m_tray, &QSystemTrayIcon::activated, this, &TrayController::onTrayActivated);
    m_tray->show();

    qInfo().noquote() << "托盘已就绪 (isSystemTrayAvailable ="
                      << QSystemTrayIcon::isSystemTrayAvailable() << ") 开机自启 ="
                      << autoStartEnabled();
}

void TrayController::setVisibleChecked(bool on)
{
    if (m_actVisible)
        m_actVisible->setChecked(on);
}

void TrayController::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    // Windows 用户习惯单击托盘图标唤出菜单
    if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
        if (m_menu)
            m_menu->popup(QCursor::pos());
    }
}

void TrayController::onToggleVisible(bool on)
{
    if (m_mgr)
        m_mgr->setVisibleAll(on);
    if (m_actEdit && !on)
        m_actEdit->setChecked(false);
    qInfo().noquote() << "[tray] 组件可见性 =" << on;
}

void TrayController::onToggleEdit(bool on)
{
    if (m_mgr)
        m_mgr->setEditing(on);
    if (m_actVisible && on)
        m_actVisible->setChecked(true); // 编辑时必须可见
}

void TrayController::onSettings()
{
    if (!m_settings)
        return;
    m_settings->show();
    qInfo().noquote() << "[tray] 打开设置界面";
}

void TrayController::onOpenConfigFolder()
{
    if (!m_cfg)
        return;
    const QFileInfo fi(m_cfg->path());
    QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absolutePath()));
    qInfo().noquote() << "[tray] 打开配置目录:" << fi.absolutePath();
}

void TrayController::onRemount()
{
    if (m_mgr)
        m_mgr->remountAll();
}

void TrayController::onResetPosition()
{
    if (m_mgr)
        m_mgr->resetAllPositions();
    if (m_actVisible)
        m_actVisible->setChecked(true);
}

void TrayController::onAddCard()
{
    // 兼容旧的单入口：默认加纪念日卡
    if (m_mgr)
        m_mgr->addCard(QStringLiteral("anniversary"));
}

bool TrayController::autoStartEnabled() const
{
    return AutoStart::enabled();
}

void TrayController::setAutoStart(bool on)
{
    QString after;
    const bool ok = AutoStart::setEnabled(on, &after);
    qInfo().noquote() << "[tray] 开机自启 =" << on << (ok ? QStringLiteral("[成功]") : QStringLiteral("[失败]"))
                      << "注册表值 =" << after;
}

void TrayController::onToggleAutoStart(bool on)
{
    setAutoStart(on);
    // 写入失败（极少数被组策略锁定的机器）→ 把勾选态改回真实状态
    if (m_actAutoStart && m_actAutoStart->isChecked() != autoStartEnabled()) {
        const QSignalBlocker b(m_actAutoStart);
        m_actAutoStart->setChecked(autoStartEnabled());
    }
}

void TrayController::onQuit()
{
    // 退出是"组件彻底消失"的操作，必须二次确认（曾经被误点导致用户以为组件丢了）
    QMessageBox box(QMessageBox::Question, QStringLiteral("退出桌面组件"),
                    QStringLiteral("退出后组件会从桌面消失（已保存的位置和设置不会丢，重新启动即可恢复）。\n\n"
                                   "确定要退出吗？"),
                    QMessageBox::Yes | QMessageBox::No, nullptr);
    box.setWindowFlag(Qt::WindowStaysOnTopHint, true);
    box.setButtonText(QMessageBox::Yes, QStringLiteral("退出"));
    box.setButtonText(QMessageBox::No, QStringLiteral("取消"));
    box.setDefaultButton(QMessageBox::No);
    if (box.exec() != QMessageBox::Yes) {
        qInfo().noquote() << "[tray] 退出已取消";
        return;
    }
    qInfo().noquote() << "[tray] 用户确认退出";
    if (m_mgr)
        m_mgr->quitApp();
    else
        QCoreApplication::quit();
}
