#include "CardBackend.h"

#include "Festivals.h"
#include "WidgetManager.h"

#include <QDebug>
#include <QQuickWindow>
#include <QUrl>

namespace {
QVariantMap makeMiniCard(const QString& label, const QString& value, const QString& unit)
{
    QVariantMap m;
    m.insert(QStringLiteral("label"), label);
    m.insert(QStringLiteral("value"), value);
    m.insert(QStringLiteral("unit"), unit);
    return m;
}

// 取"同月同日"：源日期是 2/29 而当年不是闰年时退到 2/28
QDate sameDayOf(const QDate& source, int year, int month)
{
    const QDate probe(year, month, 1);
    if (!probe.isValid())
        return QDate();
    return QDate(year, month, qMin(source.day(), probe.daysInMonth()));
}
} // namespace

// ==================== 纪念日口径（唯一实现，卡片与设置界面预览共用）====================
AnnivMath::Result AnnivMath::compute(const QString& dateText, const QString& mode, const QString& repeat,
                                    const QDate& today)
{
    Result r;
    const QDate start = QDate::fromString(dateText.trimmed(), QStringLiteral("yyyy-MM-dd"));
    if (!start.isValid())
        return r; // valid = false
    r.valid = true;

    if (mode == QLatin1String("countdown")) {
        // 倒计时模式：算"还有多少天"。不重复 = 就那一天；每年/每月 = 下一次同月同日
        QDate target = start;
        if (repeat == QLatin1String("yearly")) {
            QDate cand = sameDayOf(start, today.year(), start.month());
            if (!cand.isValid() || cand < today)
                cand = sameDayOf(start, today.year() + 1, start.month());
            target = cand;
        } else if (repeat == QLatin1String("monthly")) {
            QDate cand = sameDayOf(start, today.year(), today.month());
            if (!cand.isValid() || cand < today) {
                const QDate nm = today.addMonths(1);
                cand = sameDayOf(start, nm.year(), nm.month());
            }
            target = cand;
        }
        r.target = target;
        r.elapsed = false;
        const int d = today.daysTo(target);
        if (d >= 0) {
            r.days = d;
        } else {
            r.days = 0;
            r.expired = true; // 日期已过又不重复 → 只能显示 0（设置界面会给提示）
        }
    } else {
        // 纪念日模式：算"已经多少天"，从起始日累加
        r.target = start;
        r.elapsed = true;
        const int d = start.daysTo(today);
        r.days = d >= 0 ? d : 0; // 起始日还在未来 → 0（提示改用倒计时模式）
    }
    return r;
}

AnnivMath::Result AnnivMath::computeLunar(const QString& eventId, int lunarMonth, int lunarDay,
                                         const QDate& today)
{
    Result r;
    const Festivals& f = Festivals::instance();
    const QDate target = (eventId == QLatin1String("custom"))
                             ? f.nextLunarDate(lunarMonth, lunarDay, today)
                             : f.nextLunarEvent(eventId, today);
    if (!target.isValid())
        return r; // valid = false（数据超出 2025–2045）
    r.valid = true;
    r.target = target;
    r.elapsed = false;
    const int d = today.daysTo(target);
    r.days = d >= 0 ? d : 0;
    if (d < 0)
        r.expired = true;
    return r;
}

CardBackend::CardBackend(ConfigStore* cfg, int index, WidgetManager* mgr, QObject* parent)
    : QObject(parent)
    , m_mgr(mgr)
    , m_cfg(cfg)
    , m_index(index)
{
    applyConfig();
    recompute();
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, &CardBackend::recompute);
    m_timer.start();
}

bool CardBackend::editing() const
{
    return m_mgr && m_mgr->editing();
}

QUrl CardBackend::bgImage() const
{
    if (m_bgImage.isEmpty())
        return QUrl();
    return QUrl::fromLocalFile(m_bgImage);
}

void CardBackend::applyConfig()
{
    if (!m_cfg || m_index < 0 || m_index >= m_cfg->data().cards.size())
        return;
    const ConfigStore::CardData& c = m_cfg->data().cards.at(m_index);

    m_type = c.type;
    m_cardName = c.cardName;
    m_cardWidth = qBound(240.0, double(c.cardWidth), 760.0);
    m_cardColor = QColor(c.cardColor).isValid() ? QColor(c.cardColor) : QColor(QStringLiteral("#FFFFFF"));
    m_fontColor = QColor(c.fontColor).isValid() ? QColor(c.fontColor) : QColor(QStringLiteral("#3A3A3A"));
    m_miniCardColor = QColor(c.miniCardColor).isValid() ? QColor(c.miniCardColor)
                                                        : QColor(QStringLiteral("#F2F2F2"));
    m_fontFamily = c.fontFamily.isEmpty() ? QStringLiteral("Microsoft YaHei UI") : c.fontFamily;

    m_titleText = c.title;
    m_workDays = c.workDays;
    m_workStart = QTime::fromString(c.workStart, QStringLiteral("HH:mm"));
    m_workEnd = QTime::fromString(c.workEnd, QStringLiteral("HH:mm"));
    if (!m_workStart.isValid())
        m_workStart = QTime(8, 30);
    if (!m_workEnd.isValid())
        m_workEnd = QTime(17, 30);
    m_payday = qBound(1, c.payday, 31);
    m_dailyIncome = c.dailyIncome;
    m_showPayday = c.showPayday;
    m_showFriday = c.showFriday;
    m_showHoliday = c.showHoliday;
    m_showEarnings = c.showEarnings;
    m_showMascot = c.showMascot;
    m_mascotMode = c.mascotMode.isEmpty() ? QStringLiteral("image") : c.mascotMode;

    m_eventName = c.eventName;
    m_dateConfig = c.date;
    m_dateText = c.date;
    m_annivMode = (c.annivMode == QLatin1String("countdown") || c.annivMode == QLatin1String("lunar"))
                      ? c.annivMode
                      : QStringLiteral("anniversary");
    m_lunarEvent = c.lunarEvent.isEmpty() ? QStringLiteral("chunjie") : c.lunarEvent;
    m_lunarMonth = qBound(1, c.lunarMonth, 12);
    m_lunarDay = qBound(1, c.lunarDay, 30);
    m_repeatMode = c.repeatMode.isEmpty() ? QStringLiteral("none") : c.repeatMode;
    m_bgMode = c.bgMode.isEmpty() ? QStringLiteral("color") : c.bgMode;
    m_bgImage = c.bgImage;
}

void CardBackend::reload()
{
    applyConfig();
    recompute();
    emit styleChanged();
    // 文案可能改了但"今天休息"这类运行态标题让 recompute() 判定无变化 → 这里补一次通知
    emit titleChanged();
    emit anniversaryChanged();
}

QDate CardBackend::nextPayday(const QDate& today) const
{
    QDate d(today.year(), today.month(), qMin(m_payday, today.daysInMonth()));
    if (d < today) {
        const QDate nm = today.addMonths(1);
        d = QDate(nm.year(), nm.month(), qMin(m_payday, nm.daysInMonth()));
    }
    return d;
}

QDate CardBackend::nextFriday(const QDate& today) const
{
    const int dow = today.dayOfWeek();            // 1=周一 … 7=周日
    QDate fri = today.addDays((5 - dow + 7) % 7); // 本周五（可能是今天）
    if (fri == today && dwEffectiveNow().time() >= m_workEnd)
        fri = fri.addDays(7); // 周五下班后就算下周五
    return fri;
}

QString CardBackend::earningsText(const QDateTime& now) const
{
    const int total = m_workStart.secsTo(m_workEnd);
    if (total <= 0)
        return QStringLiteral("0.000");
    const int done = qBound(0, m_workStart.secsTo(now.time()), total);
    const double earned = m_dailyIncome * double(done) / double(total);
    return QString::number(earned, 'f', 3);
}

void CardBackend::recomputeAnniversary(const QDate& today)
{
    const AnnivMath::Result r =
        (m_annivMode == QLatin1String("lunar"))
            ? AnnivMath::computeLunar(m_lunarEvent, m_lunarMonth, m_lunarDay, today)
            : AnnivMath::compute(m_dateConfig, m_annivMode, m_repeatMode, today);
    const QString num = r.valid ? QString::number(r.days) : QStringLiteral("-");
    // 卡片下方那行日期：倒计时/农历时显示"下一次的那天"，比显示原始日期更有用
    const QString disp = r.valid ? r.target.toString(QStringLiteral("yyyy-MM-dd")) : m_dateConfig;
    if (num == m_bigNumber && disp == m_dateText)
        return;
    m_bigNumber = num;
    m_dateText = disp;
    emit anniversaryChanged();
}

void CardBackend::recompute()
{
    const QDateTime now = dwEffectiveNow();
    const QDate today = now.date();

    if (isAnniversary()) {
        recomputeAnniversary(today);
        return;
    }

    // —— 倒计时卡：今天是否休息由「官方放假表」（能处理调休补班）判定，
    //    没有官方数据的年份退回用户自配的工作日
    const bool rest = Festivals::instance().isRest(today, m_workDays);
    QString displayTitle;
    QString newCountdown;
    if (rest) {
        displayTitle = QStringLiteral("今天休息");
        newCountdown = QStringLiteral("--:--:--");
    } else if (now.time() < m_workStart) {
        displayTitle = m_titleText;
        newCountdown =
            QTime(0, 0).addSecs(m_workStart.secsTo(m_workEnd)).toString(QStringLiteral("HH:mm:ss"));
    } else if (now.time() < m_workEnd) {
        displayTitle = m_titleText;
        newCountdown =
            QTime(0, 0).addSecs(now.time().secsTo(m_workEnd)).toString(QStringLiteral("HH:mm:ss"));
    } else {
        displayTitle = QStringLiteral("已下班");
        newCountdown = QStringLiteral("00:00:00");
    }

    if (displayTitle != m_displayTitle) {
        m_displayTitle = displayTitle;
        emit titleChanged();
    }
    if (newCountdown != m_countdown) {
        m_countdown = newCountdown;
        emit countdownChanged();
    }

    const Festivals::Entry f = Festivals::instance().nextAfter(today);
    QVariantList list;
    if (m_showPayday)
        list << makeMiniCard(QStringLiteral("发薪"), QString::number(today.daysTo(nextPayday(today))),
                             QStringLiteral("天"));
    if (m_showFriday)
        list << makeMiniCard(QStringLiteral("周五"), QString::number(today.daysTo(nextFriday(today))),
                             QStringLiteral("天"));
    if (m_showHoliday)
        list << makeMiniCard(f.name, QString::number(today.daysTo(f.date)), QStringLiteral("天"));
    if (m_showEarnings)
        list << makeMiniCard(QStringLiteral("今天赚了"),
                             rest ? QStringLiteral("0.000") : earningsText(now), QStringLiteral("¥"));

    if (list != m_miniCards) {
        m_miniCards = list;
        emit miniCardsChanged();
    }
}

void CardBackend::startDrag()
{
    if (!m_window)
        return;
    m_dragOffset = DesktopLayer::cursorScreenPos() - DesktopLayer::windowScreenRect(m_window->winId()).topLeft();
}

void CardBackend::dragMove()
{
    if (!m_window)
        return;
    // 拖动过程中就钳制，保证怎么拖都不会丢到屏幕外
    const QRect target = DesktopLayer::sanitizeRect(
        QRect(DesktopLayer::cursorScreenPos() - m_dragOffset,
              DesktopLayer::windowScreenRect(m_window->winId()).size()));
    DesktopLayer::moveToScreenTopLeft(m_window->winId(), target.topLeft());
    if (qEnvironmentVariableIsSet("DW_DRAGLOG"))
        qInfo() << "[drag] card" << m_index << "->" << target.topLeft();
}

void CardBackend::savePosition()
{
    if (!m_window || !m_cfg || m_index >= m_cfg->data().cards.size())
        return;
    const QRect cur = DesktopLayer::windowScreenRect(m_window->winId());
    const QRect r = DesktopLayer::sanitizeRect(cur);
    if (r.topLeft() != cur.topLeft())
        DesktopLayer::moveToScreenTopLeft(m_window->winId(), r.topLeft());
    ConfigStore::CardData& c = m_cfg->mutableData().cards[m_index];
    c.x = r.x();
    c.y = r.y();
    c.hasPos = true;
    m_cfg->save();
}

void CardBackend::resetPosition()
{
    if (!m_window)
        return;
    // 多卡片：第 n 张卡依次往下排，避免叠在一起
    const int margin = qRound(40.0 * m_window->devicePixelRatio());
    const int step = qRound((m_window->height() + 16) * m_window->devicePixelRatio());
    DesktopLayer::placeTopRight(m_window->winId(), margin + m_index * step);
    savePosition();
    qInfo().noquote() << "[reset] 卡片" << m_index << "位置 → 右上角"
                      << DesktopLayer::windowScreenRect(m_window->winId());
}

void CardBackend::finishEditing()
{
    if (m_mgr)
        m_mgr->setEditing(false);
}
