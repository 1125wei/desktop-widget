#include "SettingsWindow.h"

#include "SettingsBackend.h"
#include "WidgetManager.h"

#include <QCursor>
#include <QDebug>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlContext>
#include <QQuickView>
#include <QScreen>

SettingsWindow::SettingsWindow(SettingsBackend* sb, WidgetManager* mgr, QObject* parent)
    : QObject(parent)
    , m_sb(sb)
    , m_mgr(mgr)
{
    // QML 里点"完成"/右上角 ✕ → 隐藏窗口（不退出程序）
    connect(m_sb, &SettingsBackend::closeRequested, this, [this]() {
        if (m_view)
            m_view->hide();
    });
}

void SettingsWindow::createView()
{
    m_view = new QQuickView();
    m_view->setFlags(Qt::Window | Qt::FramelessWindowHint); // 自绘标题栏，深色一体
    m_view->setColor(QColor(QStringLiteral("#131315")));
    m_view->setResizeMode(QQuickView::SizeViewToRootObject);
    m_view->setTitle(QStringLiteral("桌面组件 · 设置"));
    m_view->setIcon(QIcon(QStringLiteral(":/assets/tray.ico")));
    m_view->rootContext()->setContextProperty(QStringLiteral("Settings"), m_sb);
    // 预览卡片直接取 Settings.previewCard，不再需要额外的 Backend 上下文属性

    const QString dir = qEnvironmentVariable("DW_QML_DIR");
    const QUrl source = dir.isEmpty() ? QUrl(QStringLiteral("qrc:/qml/SettingsWindow.qml"))
                                      : QUrl::fromLocalFile(dir + QStringLiteral("/SettingsWindow.qml"));
    m_view->setSource(source);
    if (m_view->status() != QQuickView::Ready) {
        qWarning() << "设置界面 QML 加载失败:" << m_view->errors();
        return;
    }
    qInfo().noquote() << "设置界面已创建:" << source.toString();
}

bool SettingsWindow::isOpen() const
{
    return m_view && m_view->isVisible();
}

bool SettingsWindow::grabTo(const QString& path) const
{
    if (!m_view || !m_view->isVisible())
        return false;
    const QImage img = m_view->grabWindow();
    if (img.isNull())
        return false;
    return img.save(path);
}

void SettingsWindow::show()
{
    if (!m_view)
        createView();
    if (!m_view)
        return;

    // 居中到鼠标所在显示器
    QScreen* scr = QGuiApplication::screenAt(QCursor::pos());
    if (!scr)
        scr = QGuiApplication::primaryScreen();
    if (scr) {
        const QRect g = scr->availableGeometry();
        m_view->setPosition(g.center() - QPoint(m_view->width() / 2, m_view->height() / 2));
    }
    m_view->show();
    m_view->raise();
    m_view->requestActivate();
}
