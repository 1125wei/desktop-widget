#include "WidgetManager.h"

#include "CardBackend.h"
#include "ConfigStore.h"

#include <QCoreApplication>
#include <QDebug>
#include <QIcon>
#include <QQmlContext>
#include <QQuickView>
#include <QUrl>

namespace {
const int kGapLogical = 14;    // 纵向排列时卡片之间的间距（逻辑像素）
const int kMarginLogical = 40; // 距屏幕右边/上边的留白（逻辑像素）

QUrl cardQmlSource()
{
    const QString dir = qEnvironmentVariable("DW_QML_DIR");
    if (dir.isEmpty())
        return QUrl(QStringLiteral("qrc:/qml/Main.qml"));
    return QUrl::fromLocalFile(dir + QStringLiteral("/Main.qml"));
}
} // namespace

WidgetManager::WidgetManager(ConfigStore* cfg, DesktopLayer::Mode mode, QObject* parent)
    : QObject(parent)
    , m_cfg(cfg)
    , m_mode(mode)
{
}

WidgetManager::~WidgetManager()
{
    for (QQuickView* v : m_views) {
        if (v) {
            DesktopLayer::forget(v->winId());
            delete v;
        }
    }
    m_views.clear();
}

void WidgetManager::start()
{
    if (!m_cfg)
        return;
    m_visible = m_cfg->data().visible;
    for (int i = 0; i < m_cfg->data().cards.size(); ++i)
        createCard(i);

    if (m_mode != DesktopLayer::Float) {
        m_guard.setInterval(3000);
        connect(&m_guard, &QTimer::timeout, this, [this]() {
            if (m_editing)
                return; // 编辑模式中不该干涉
            for (int i = 0; i < m_views.size(); ++i) {
                QQuickView* v = m_views.at(i);
                if (!v)
                    continue;
                if (!DesktopLayer::isOnScreen(v->winId())) {
                    qWarning().noquote() << "[watchdog] 卡片" << i << "跑到屏幕外了，拉回右上角";
                    pullBackToScreen(i);
                    continue;
                }
                if (!DesktopLayer::isOnDesktopLayer(v->winId())) {
                    const DesktopLayer::Info re = DesktopLayer::apply(v->winId(), m_mode);
                    qInfo().noquote() << "[watchdog] 卡片" << i << "重新挂载桌面层:" << re.toLog();
                    repaintCard(i, QStringLiteral("watchdog"));
                }
            }
        });
        m_guard.start();
    }
    dumpState(QStringLiteral("启动完成"));
}

void WidgetManager::createCard(int index)
{
    if (!m_cfg || index < 0 || index >= m_cfg->data().cards.size())
        return;

    auto* backend = new CardBackend(m_cfg, index, this, this);
    connect(this, &WidgetManager::editingChanged, backend, &CardBackend::editingChanged);

    auto* view = new QQuickView();
    view->setFlags(Qt::FramelessWindowHint | Qt::Tool); // 无边框 + 不进任务栏/Alt-Tab
    view->setColor(Qt::transparent);                    // 透明底，只显示卡片本体
    view->setResizeMode(QQuickView::SizeViewToRootObject);
    view->setIcon(QIcon(QStringLiteral(":/assets/tray.ico")));
    view->rootContext()->setContextProperty(QStringLiteral("Card"), backend);
    view->setSource(cardQmlSource());
    if (view->status() != QQuickView::Ready) {
        qWarning() << "卡片 QML 加载失败:" << index << view->errors();
    }

    m_cards.append(backend);
    m_views.append(view);

    view->show();
    backend->attachWindow(view);

    qInfo().noquote() << "[card]" << index << backend->cardName() << "窗口已建"
                      << "逻辑尺寸" << view->size() << "DPR" << view->devicePixelRatio();

    applyLayer(index);
    restorePosition(index, true);

    // 编辑模式中新增卡片 → 新窗口也要进入编辑态
    if (m_editing)
        DesktopLayer::detachForEditing(view->winId());
    view->setVisible(m_visible);

    emit cardsChanged();
}

void WidgetManager::applyLayer(int index)
{
    QQuickView* v = viewAt(index);
    if (!v)
        return;
    const DesktopLayer::Info info = DesktopLayer::apply(v->winId(), m_mode);
    qInfo().noquote() << "[card]" << index << "挂载:" << info.toLog();
}

void WidgetManager::restorePosition(int index, bool useConfig)
{
    QQuickView* v = viewAt(index);
    CardBackend* b = cardAt(index);
    if (!v || !b || !m_cfg || index >= m_cfg->data().cards.size())
        return;

    const QRect cur = DesktopLayer::windowScreenRect(v->winId());
    // ⚠️ 必须用实际物理矩形（GetWindowRect）算钳制量，Qt 逻辑尺寸在 150% 缩放下会算错
    const double dpr = v->devicePixelRatio();
    const int margin = qRound(kMarginLogical * dpr);

    const ConfigStore::CardData& c = m_cfg->data().cards.at(index);
    if (useConfig && c.hasPos) {
        const QRect want = DesktopLayer::sanitizeRect(QRect(c.x, c.y, cur.width(), cur.height()));
        DesktopLayer::moveToScreenTopLeft(v->winId(), want.topLeft());
        qInfo().noquote() << "[card]" << index << "还原位置:" << c.x << c.y << "→ 钳制后" << want.topLeft();
    } else {
        placeBelowPrevious(index);
        qInfo().noquote() << "[card]" << index << "初始摆位 →"
                          << DesktopLayer::windowScreenRect(v->winId()).topLeft();
    }
    b->savePosition(); // 兜底：按真实物理矩形再钳一次并落盘
}

// 排在上一张卡片正下方（第一张则贴右上角）；越界由 sanitizeRect 兜底
void WidgetManager::placeBelowPrevious(int index)
{
    QQuickView* v = viewAt(index);
    if (!v)
        return;
    const QRect cur = DesktopLayer::windowScreenRect(v->winId());
    const double dpr = v->devicePixelRatio();
    const int margin = qRound(kMarginLogical * dpr);
    const QRect wa = DesktopLayer::workAreaPhysical(v->winId());

    QPoint target(wa.x() + wa.width() - cur.width() - margin, wa.y() + margin);
    if (index > 0) {
        if (QQuickView* pv = viewAt(index - 1)) {
            const QRect pr = DesktopLayer::windowScreenRect(pv->winId());
            target = QPoint(pr.x(), pr.y() + pr.height() + qRound(kGapLogical * dpr));
        }
    }
    const QRect want = DesktopLayer::sanitizeRect(QRect(target, cur.size()));
    DesktopLayer::moveToScreenTopLeft(v->winId(), want.topLeft());
}

void WidgetManager::repaintCard(int index, const QString& tag)
{
    QQuickView* v = viewAt(index);
    if (!v)
        return;
    const WId wid = v->winId();
    // ⚠️ 二次挂载（编辑模式退出）后 Qt 不会自动重绘：换父窗口是 OS 层面的事，
    //    DWM 丢了旧画面、Qt 却毫不知情 → 桌面上就是空白。最可靠的是"隐藏再显示"。
    v->hide();
    QTimer::singleShot(30, this, [this, index, wid, tag]() {
        QQuickView* vv = viewAt(index);
        if (!vv)
            return;
        vv->show();
        vv->requestUpdate();
        DesktopLayer::forceRepaint(wid);
        qInfo().noquote() << "[repaint]" << tag << "card" << index
                          << "isExposed =" << vv->isExposed();
    });
}

void WidgetManager::setEditing(bool on)
{
    if (m_editing == on)
        return;
    m_editing = on;

    if (on)
        setVisibleAll(true); // 编辑时必须可见，否则找不到卡片

    for (int i = 0; i < m_views.size(); ++i) {
        QQuickView* v = m_views.at(i);
        if (!v)
            continue;
        const WId wid = v->winId();
        if (on) {
            DesktopLayer::detachForEditing(wid);
            qInfo().noquote() << "[edit] 卡片" << i << "进入编辑模式:"
                              << DesktopLayer::describe(wid, QStringLiteral("edit")).toLog();
        } else {
            applyLayer(i);
            DesktopLayer::dumpChildZOrder(wid, QStringLiteral("退出编辑后"));
            repaintCard(i, QStringLiteral("退出编辑"));
        }
    }

    if (!on) {
        saveAllPositions();
        dumpState(QStringLiteral("退出编辑后"));
    }

    emit editingChanged();
}

void WidgetManager::reloadCard(int index)
{
    if (CardBackend* b = cardAt(index))
        b->reload();
}

void WidgetManager::reloadAll()
{
    for (CardBackend* b : m_cards)
        b->reload();
    qInfo().noquote() << "[config] 已实时应用（卡片数" << m_cards.size() << "）";
}

void WidgetManager::saveAllPositions()
{
    for (CardBackend* b : m_cards)
        b->savePosition();
}

void WidgetManager::resetAllPositions()
{
    for (int i = 0; i < m_cards.size(); ++i) {
        if (!viewAt(i))
            continue;
        placeBelowPrevious(i);
        if (CardBackend* b = cardAt(i))
            b->savePosition();
    }
    setVisibleAll(true);
    qInfo().noquote() << "[reset] 全部卡片已回到右上角（纵向排开）";
}

void WidgetManager::pullBackToScreen(int index)
{
    QQuickView* v = viewAt(index);
    if (!v)
        return;
    DesktopLayer::placeTopRight(v->winId(), qRound(kMarginLogical * v->devicePixelRatio()));
    if (CardBackend* b = cardAt(index))
        b->savePosition();
}

void WidgetManager::remountAll()
{
    if (m_editing)
        return;
    for (int i = 0; i < m_views.size(); ++i) {
        applyLayer(i);
        repaintCard(i, QStringLiteral("重新挂载"));
    }
}

void WidgetManager::setVisibleAll(bool on)
{
    m_visible = on;
    for (QQuickView* v : m_views)
        if (v)
            v->setVisible(on);
    if (m_cfg) {
        m_cfg->mutableData().visible = on;
        m_cfg->save();
    }
}

int WidgetManager::addCard(const QString& type)
{
    if (!m_cfg)
        return -1;
    if (m_editing)
        setEditing(false);

    ConfigStore::CardData c = ConfigStore::defaultCard(type);
    // 尺寸跟随现有卡片，视觉统一
    if (!m_cfg->data().cards.isEmpty())
        c.cardWidth = m_cfg->data().cards.first().cardWidth;
    m_cfg->mutableData().cards.append(c);
    const int idx = m_cfg->data().cards.size() - 1;

    qInfo().noquote() << "[card] 新增" << type << "→ 索引" << idx;
    createCard(idx);
    m_cfg->save();
    emit cardsChanged();
    return idx;
}

void WidgetManager::removeCard(int index)
{
    if (!m_cfg || index < 0 || index >= m_cards.size())
        return;
    if (m_cards.size() <= 1) {
        qWarning().noquote() << "[card] 至少保留一张卡片，拒绝删除";
        return;
    }
    if (m_editing)
        setEditing(false);

    QQuickView* v = m_views.takeAt(index);
    CardBackend* b = m_cards.takeAt(index);
    if (v) {
        DesktopLayer::forget(v->winId());
        v->hide();
        v->deleteLater();
    }
    if (b)
        b->deleteLater();

    m_cfg->mutableData().cards.removeAt(index);
    for (int i = 0; i < m_cards.size(); ++i)
        m_cards[i]->setIndex(i); // ⚠️ 删除后所有索引都要重排，否则读写到别的卡
    m_cfg->save();
    qInfo().noquote() << "[card] 已删除索引" << index << "，剩余" << m_cards.size() << "张";
    emit cardsChanged();
}

void WidgetManager::quitApp()
{
    saveAllPositions();
    QCoreApplication::quit();
}

void WidgetManager::dumpState(const QString& tag) const
{
    qInfo().noquote() << "===== 卡片状态[" << tag << "]=====";
    for (int i = 0; i < m_views.size(); ++i) {
        QQuickView* v = m_views.at(i);
        CardBackend* b = m_cards.at(i);
        qInfo().noquote() << QStringLiteral("  [%1] %2 (%3) %4")
                                 .arg(i)
                                 .arg(b ? b->cardName() : QStringLiteral("?"),
                                      b ? b->type() : QStringLiteral("?"),
                                      v ? DesktopLayer::describe(v->winId(), QStringLiteral("card")).toLog()
                                        : QStringLiteral("<no window>"));
    }
    qInfo().noquote() << "  编辑模式 =" << m_editing << " 可见 =" << m_visible;
}
