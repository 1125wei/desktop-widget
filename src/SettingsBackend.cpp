#include "SettingsBackend.h"

#include "CardBackend.h"
#include "Festivals.h" // dwEffectiveNow
#include "WidgetManager.h"

#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QSet>
#include <QTime>
#include <QUrl>

#include <algorithm>

namespace {
// 常用中文字体优先排在前面，方便选择
const QStringList kPreferredFonts{
    QStringLiteral("Microsoft YaHei UI"),
    QStringLiteral("Microsoft YaHei"),
    QStringLiteral("HarmonyOS Sans SC"),
    QStringLiteral("Source Han Sans SC"),
    QStringLiteral("Noto Sans SC"),
    QStringLiteral("SimHei"),
    QStringLiteral("KaiTi"),
    QStringLiteral("DIN Alternate"),
};

// 重复规则：value / 显示名
const QList<QPair<QString, QString>> kRepeatModes{
    {QStringLiteral("none"), QStringLiteral("不重复")},
    {QStringLiteral("yearly"), QStringLiteral("每年")},
    {QStringLiteral("monthly"), QStringLiteral("每月")},
};

// "常用事件"：参考 App 那个快捷选择（前缀直接写进事件名，卡片上读起来就通顺）
// 两种模式给不同的措辞：倒计时 = "距离X还有"，纪念日 = "X已经"
const QStringList kSuggestCountdown{
    QStringLiteral("距离生日还有"),
    QStringLiteral("距离恋爱纪念日还有"),
    QStringLiteral("距离结婚纪念日还有"),
    QStringLiteral("距离入职纪念日还有"),
    QStringLiteral("距离春节还有"),
    QStringLiteral("距离高考还有"),
    QStringLiteral("距离发工资还有"),
    QStringLiteral("距离放假还有"),
    QStringLiteral("距离考试还有"),
};
const QStringList kSuggestAnniversary{
    QStringLiteral("你在世界已经"),
    QStringLiteral("我们已经在一起"),
    QStringLiteral("入职已经"),
    QStringLiteral("结婚已经"),
    QStringLiteral("相识已经"),
    QStringLiteral("养猫已经"),
    QStringLiteral("戒烟已经"),
    QStringLiteral("开始健身已经"),
};

// 农历月/日标签（下拉用）
const QStringList kLunarMonthNames{
    QStringLiteral("正月"), QStringLiteral("二月"), QStringLiteral("三月"), QStringLiteral("四月"),
    QStringLiteral("五月"), QStringLiteral("六月"), QStringLiteral("七月"), QStringLiteral("八月"),
    QStringLiteral("九月"), QStringLiteral("十月"), QStringLiteral("冬月"), QStringLiteral("腊月"),
};
const QStringList kLunarDayNames{
    QStringLiteral("初一"),  QStringLiteral("初二"),  QStringLiteral("初三"),  QStringLiteral("初四"),
    QStringLiteral("初五"),  QStringLiteral("初六"),  QStringLiteral("初七"),  QStringLiteral("初八"),
    QStringLiteral("初九"),  QStringLiteral("初十"),  QStringLiteral("十一"),  QStringLiteral("十二"),
    QStringLiteral("十三"),  QStringLiteral("十四"),  QStringLiteral("十五"),  QStringLiteral("十六"),
    QStringLiteral("十七"),  QStringLiteral("十八"),  QStringLiteral("十九"),  QStringLiteral("二十"),
    QStringLiteral("廿一"),  QStringLiteral("廿二"),  QStringLiteral("廿三"),  QStringLiteral("廿四"),
    QStringLiteral("廿五"),  QStringLiteral("廿六"),  QStringLiteral("廿七"),  QStringLiteral("廿八"),
    QStringLiteral("廿九"),  QStringLiteral("三十"),
};

// 宽松解析日期：2027-3-5 / 2027/03/05 / 2027.3.5 / 2027年3月5日 / 20270305 全部接受
QDate parseLooseDate(const QString& in)
{
    QString s = in.trimmed();
    if (s.isEmpty())
        return {};
    s.replace(QStringLiteral("年"), QStringLiteral("-"));
    s.replace(QStringLiteral("月"), QStringLiteral("-"));
    s.remove(QStringLiteral("日"));
    s.replace(QStringLiteral("/"), QStringLiteral("-"));
    s.replace(QStringLiteral("."), QStringLiteral("-"));
    s.replace(QStringLiteral(" "), QString());
    static const QStringList fmts{QStringLiteral("yyyy-M-d"), QStringLiteral("yyyy-MM-dd"),
                                  QStringLiteral("yyyyMMdd")};
    for (const QString& f : fmts) {
        const QDate d = QDate::fromString(s, f);
        if (d.isValid())
            return d;
    }
    return {};
}

// 宽松解析时间：8:30 / 08:30 / 8点30 / 0830 全部接受
QTime parseLooseTime(const QString& in)
{
    QString s = in.trimmed();
    if (s.isEmpty())
        return {};
    s.replace(QStringLiteral("："), QStringLiteral(":"));
    s.replace(QStringLiteral("点"), QStringLiteral(":"));
    s.replace(QStringLiteral("时"), QStringLiteral(":"));
    s.remove(QStringLiteral("分"));
    s.remove(QStringLiteral(" "));
    static const QStringList fmts{QStringLiteral("H:m"), QStringLiteral("HH:mm"),
                                  QStringLiteral("Hmm"), QStringLiteral("HHmm")};
    for (const QString& f : fmts) {
        const QTime t = QTime::fromString(s, f);
        if (t.isValid())
            return t;
    }
    return {};
}
} // namespace

SettingsBackend::SettingsBackend(ConfigStore* cfg, WidgetManager* mgr, QObject* parent)
    : QObject(parent)
    , m_cfg(cfg)
    , m_mgr(mgr)
{
    // 防抖存盘：连续拖动滑块/连续点选时不要每次都写文件
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(500);
    connect(&m_saveTimer, &QTimer::timeout, this, [this]() {
        if (m_cfg)
            m_cfg->save();
    });

    const QStringList all = QFontDatabase::families();
    QSet<QString> seen;
    for (const QString& f : kPreferredFonts) {
        if (all.contains(f) && !seen.contains(f)) {
            m_fonts << f;
            seen.insert(f);
        }
    }
    for (const QString& f : all) {
        if (!seen.contains(f)) {
            m_fonts << f;
            seen.insert(f);
        }
    }
}

ConfigStore::CardData* SettingsBackend::current()
{
    if (!m_cfg || m_cfg->data().cards.isEmpty())
        return nullptr;
    if (m_current < 0 || m_current >= m_cfg->data().cards.size())
        m_current = 0;
    return &m_cfg->mutableData().cards[m_current];
}

const ConfigStore::CardData* SettingsBackend::current() const
{
    return const_cast<SettingsBackend*>(this)->current();
}

QObject* SettingsBackend::previewCard() const
{
    return m_mgr ? m_mgr->cardAt(m_current) : nullptr;
}

QString SettingsBackend::cardType() const
{
    const ConfigStore::CardData* c = current();
    return c ? c->type : QStringLiteral("countdown");
}

QStringList SettingsBackend::cardNames() const
{
    QStringList out;
    if (m_cfg) {
        for (const ConfigStore::CardData& c : m_cfg->data().cards)
            out << c.cardName;
    }
    return out;
}

int SettingsBackend::cardCount() const
{
    return m_cfg ? m_cfg->data().cards.size() : 0;
}

QString SettingsBackend::configPath() const
{
    return m_cfg ? m_cfg->path() : QString();
}

void SettingsBackend::setCurrentIndex(int v)
{
    if (!m_cfg || m_cfg->data().cards.isEmpty())
        return;
    v = qBound(0, v, m_cfg->data().cards.size() - 1);
    if (v == m_current)
        return;
    m_current = v;
    emit currentChanged();
    emitAll();
}

QString SettingsBackend::cardName() const
{
    const ConfigStore::CardData* c = current();
    return c ? c->cardName : QString();
}
int SettingsBackend::cardWidth() const
{
    const ConfigStore::CardData* c = current();
    return c ? c->cardWidth : 312;
}
QString SettingsBackend::cardColor() const
{
    const ConfigStore::CardData* c = current();
    return c ? c->cardColor : QStringLiteral("#FFFFFF");
}
QString SettingsBackend::fontColor() const
{
    const ConfigStore::CardData* c = current();
    return c ? c->fontColor : QStringLiteral("#3A3A3A");
}
QString SettingsBackend::miniCardColor() const
{
    const ConfigStore::CardData* c = current();
    return c ? c->miniCardColor : QStringLiteral("#F2F2F2");
}
QString SettingsBackend::fontFamily() const
{
    const ConfigStore::CardData* c = current();
    return c ? c->fontFamily : QStringLiteral("Microsoft YaHei UI");
}

QString SettingsBackend::title() const
{
    const ConfigStore::CardData* c = current();
    return c ? c->title : QString();
}
QString SettingsBackend::workStart() const
{
    const ConfigStore::CardData* c = current();
    return c ? c->workStart : QStringLiteral("08:30");
}
QString SettingsBackend::workEnd() const
{
    const ConfigStore::CardData* c = current();
    return c ? c->workEnd : QStringLiteral("17:30");
}
int SettingsBackend::payday() const
{
    const ConfigStore::CardData* c = current();
    return c ? c->payday : 25;
}
double SettingsBackend::dailyIncome() const
{
    const ConfigStore::CardData* c = current();
    return c ? c->dailyIncome : 100.0;
}

bool SettingsBackend::flag(const QString& key) const
{
    const ConfigStore::CardData* c = current();
    if (!c)
        return false;
    if (key == QLatin1String("showPayday"))
        return c->showPayday;
    if (key == QLatin1String("showFriday"))
        return c->showFriday;
    if (key == QLatin1String("showHoliday"))
        return c->showHoliday;
    if (key == QLatin1String("showMascot"))
        return c->showMascot;
    return c->showEarnings;
}

QVariantList SettingsBackend::workDays() const
{
    QVariantList out;
    const ConfigStore::CardData* c = current();
    const QList<int> wd = c ? c->workDays : QList<int>{1, 2, 3, 4, 5};
    for (int day = 1; day <= 7; ++day)
        out << wd.contains(day);
    return out;
}

bool SettingsBackend::workdayPreset() const
{
    const ConfigStore::CardData* c = current();
    return c && c->workDays == QList<int>{1, 2, 3, 4, 5};
}

QString SettingsBackend::eventName() const
{
    const ConfigStore::CardData* c = current();
    return c ? c->eventName : QString();
}
QString SettingsBackend::date() const
{
    const ConfigStore::CardData* c = current();
    return c ? c->date : QStringLiteral("2003-11-25");
}
QString SettingsBackend::repeatMode() const
{
    const ConfigStore::CardData* c = current();
    return c ? c->repeatMode : QStringLiteral("none");
}
int SettingsBackend::repeatIndex() const
{
    const QString m = repeatMode();
    for (int i = 0; i < kRepeatModes.size(); ++i) {
        if (kRepeatModes.at(i).first == m)
            return i;
    }
    return 0;
}
QString SettingsBackend::bgMode() const
{
    const ConfigStore::CardData* c = current();
    return c ? c->bgMode : QStringLiteral("color");
}
QString SettingsBackend::bgImage() const
{
    const ConfigStore::CardData* c = current();
    return c ? c->bgImage : QString();
}
QString SettingsBackend::bgImageName() const
{
    const QString p = bgImage();
    return p.isEmpty() ? QString() : QFileInfo(p).fileName();
}
bool SettingsBackend::hasImage() const
{
    const QString p = bgImage();
    return !p.isEmpty() && QFileInfo::exists(p);
}

QVariantList SettingsBackend::repeatLabels() const
{
    QVariantList out;
    for (const auto& p : kRepeatModes)
        out << p.second;
    return out;
}

QVariantList SettingsBackend::eventSuggestions() const
{
    QVariantList out;
    if (isLunarMode()) {
        // 农历模式下给"距离春节还有/距离中秋还有…"这类现成措辞
        for (const Festivals::LunarEvent& e : Festivals::instance().lunarEvents())
            out << QStringLiteral("距离%1还有").arg(e.name);
        out << QStringLiteral("距离农历生日还有");
        return out;
    }
    const QStringList& list = isCountdownMode() ? kSuggestCountdown : kSuggestAnniversary;
    for (const QString& s : list)
        out << s;
    return out;
}

void SettingsBackend::touchConfig()
{
    if (!m_cfg)
        return;
    if (m_mgr)
        m_mgr->reloadAll(); // 实时预览 + 桌面组件同步
    m_saveTimer.start();
}

void SettingsBackend::emitAll()
{
    emit styleChanged();
    emit titleChanged();
    emit workDaysChanged();
    emit timeChanged();
    emit paydayChanged();
    emit dailyIncomeChanged();
    emit showFlagsChanged();
    emit anniversaryChanged();
}

void SettingsBackend::setCardName(const QString& v)
{
    ConfigStore::CardData* c = current();
    if (!c)
        return;
    const QString t = v.trimmed();
    if (t.isEmpty() || t == c->cardName)
        return;
    c->cardName = t;
    emit styleChanged();
    emit cardsChanged();
    touchConfig();
}

void SettingsBackend::setTitle(const QString& v)
{
    ConfigStore::CardData* c = current();
    if (!c)
        return;
    const QString t = v.trimmed().isEmpty() ? QStringLiteral("下班还有") : v.trimmed();
    if (t == c->title)
        return;
    c->title = t;
    emit titleChanged();
    touchConfig();
}

void SettingsBackend::setWorkStart(const QString& v)
{
    ConfigStore::CardData* c = current();
    if (!c)
        return;
    const QTime t = QTime::fromString(v, QStringLiteral("HH:mm"));
    if (!t.isValid()) {
        emit timeChanged(); // 非法输入 → 让界面回退到已存值
        return;
    }
    c->workStart = t.toString(QStringLiteral("HH:mm"));
    emit timeChanged();
    touchConfig();
}

void SettingsBackend::setWorkEnd(const QString& v)
{
    ConfigStore::CardData* c = current();
    if (!c)
        return;
    const QTime t = QTime::fromString(v, QStringLiteral("HH:mm"));
    if (!t.isValid()) {
        emit timeChanged();
        return;
    }
    c->workEnd = t.toString(QStringLiteral("HH:mm"));
    emit timeChanged();
    touchConfig();
}

void SettingsBackend::setPayday(int v)
{
    ConfigStore::CardData* c = current();
    if (!c)
        return;
    v = qBound(1, v, 31);
    if (v == c->payday)
        return;
    c->payday = v;
    emit paydayChanged();
    touchConfig();
}

void SettingsBackend::setDailyIncome(double v)
{
    ConfigStore::CardData* c = current();
    if (!c)
        return;
    v = qBound(0.0, v, 1000000.0);
    if (qFuzzyCompare(v, c->dailyIncome))
        return;
    c->dailyIncome = v;
    emit dailyIncomeChanged();
    touchConfig();
}

#define DW_FLAG_SETTER(Name, Field)                                                                \
    void SettingsBackend::set##Name(bool v)                                                        \
    {                                                                                              \
        ConfigStore::CardData* c = current();                                                      \
        if (!c || v == c->Field)                                                                   \
            return;                                                                                \
        c->Field = v;                                                                              \
        emit showFlagsChanged();                                                                   \
        touchConfig();                                                                             \
    }

DW_FLAG_SETTER(ShowPayday, showPayday)
DW_FLAG_SETTER(ShowFriday, showFriday)
DW_FLAG_SETTER(ShowHoliday, showHoliday)
DW_FLAG_SETTER(ShowEarnings, showEarnings)
DW_FLAG_SETTER(ShowMascot, showMascot)

QString SettingsBackend::mascotMode() const
{
    const ConfigStore::CardData* c = current();
    if (!c || c->mascotMode.isEmpty())
        return QStringLiteral("image");
    return c->mascotMode;
}

void SettingsBackend::setMascotMode(const QString& v)
{
    ConfigStore::CardData* c = current();
    if (!c)
        return;
    if (v != QLatin1String("image") && v != QLatin1String("vector"))
        return;
    if (v == c->mascotMode)
        return;
    c->mascotMode = v;
    emit showFlagsChanged();
    touchConfig();
}

#define DW_COLOR_SETTER(Name, Field, Fallback)                                                     \
    void SettingsBackend::set##Name(const QString& v)                                              \
    {                                                                                              \
        ConfigStore::CardData* c = current();                                                      \
        if (!c)                                                                                    \
            return;                                                                                \
        QColor col(v);                                                                             \
        if (!col.isValid()) {                                                                      \
            emit styleChanged();                                                                   \
            return;                                                                                \
        }                                                                                          \
        const QString hex = col.name(QColor::HexRgb).toUpper();                                     \
        if (hex == c->Field)                                                                       \
            return;                                                                                \
        c->Field = hex.isEmpty() ? QStringLiteral(Fallback) : hex;                                 \
        emit styleChanged();                                                                       \
        touchConfig();                                                                             \
    }

DW_COLOR_SETTER(CardColor, cardColor, "#FFFFFF")
DW_COLOR_SETTER(FontColor, fontColor, "#3A3A3A")
DW_COLOR_SETTER(MiniCardColor, miniCardColor, "#F2F2F2")

void SettingsBackend::setFontFamily(const QString& v)
{
    ConfigStore::CardData* c = current();
    if (!c || v.isEmpty() || v == c->fontFamily)
        return;
    c->fontFamily = v;
    emit styleChanged();
    touchConfig();
}

void SettingsBackend::setCardWidth(int v)
{
    ConfigStore::CardData* c = current();
    if (!c)
        return;
    v = qBound(240, v, 760);
    if (v == c->cardWidth)
        return;
    c->cardWidth = v;
    emit cardWidthChanged();
    touchConfig();
    // 窗口尺寸会跟着 QML 根 item 变化，等 Qt 完成 resize 后再把位置钳回屏内并落盘
    const int idx = m_current;
    QTimer::singleShot(220, this, [this, idx]() {
        if (CardBackend* b = m_mgr ? m_mgr->cardAt(idx) : nullptr)
            b->savePosition();
    });
}

void SettingsBackend::setEventName(const QString& v)
{
    ConfigStore::CardData* c = current();
    if (!c)
        return;
    const QString t = v.trimmed();
    if (t.isEmpty() || t == c->eventName)
        return;
    c->eventName = t;
    emit anniversaryChanged();
    touchConfig();
}

void SettingsBackend::setDate(const QString& v)
{
    ConfigStore::CardData* c = current();
    if (!c)
        return;
    const QDate d = parseLooseDate(v);
    if (!d.isValid()) {
        emit anniversaryChanged(); // 非法 → 界面回退
        return;
    }
    const QString s = d.toString(QStringLiteral("yyyy-MM-dd"));
    if (s == c->date)
        return;
    c->date = s;
    emit anniversaryChanged();
    touchConfig();
}

// 输入框提交路径：解析失败要**明确告诉调用方**（旧实现只发信号，而 QML 那边的"回退显示"
// 被 activeFocus 挡住 → 用户打了半天日期，输入框留着他的字、模型还是旧值，卡片毫无反应）
bool SettingsBackend::applyDateInput(const QString& text)
{
    ConfigStore::CardData* c = current();
    if (!c)
        return false;
    const QDate d = parseLooseDate(text);
    if (!d.isValid()) {
        emit anniversaryChanged();
        return false;
    }
    const QString s = d.toString(QStringLiteral("yyyy-MM-dd"));
    if (s != c->date) {
        c->date = s;
        touchConfig();
    }
    emit anniversaryChanged();
    return true;
}

bool SettingsBackend::applyTimeInput(const QString& which, const QString& text)
{
    ConfigStore::CardData* c = current();
    if (!c)
        return false;
    const QTime t = parseLooseTime(text);
    if (!t.isValid()) {
        emit timeChanged();
        return false;
    }
    const QString s = t.toString(QStringLiteral("HH:mm"));
    const bool isStart = (which == QLatin1String("start"));
    if (isStart) {
        if (s != c->workStart) {
            c->workStart = s;
            touchConfig();
        }
    } else {
        if (s != c->workEnd) {
            c->workEnd = s;
            touchConfig();
        }
    }
    emit timeChanged();
    return true;
}

QString SettingsBackend::annivMode() const
{
    const ConfigStore::CardData* c = current();
    if (!c)
        return QStringLiteral("anniversary");
    // ⚠️ 别写成"非 countdown 就是 anniversary"——那样会把 lunar 吃掉（设置界面预览显示错，
    //    农历那一行也永远不出现）。三种模式都要原样透传。
    if (c->annivMode == QLatin1String("countdown") || c->annivMode == QLatin1String("lunar"))
        return c->annivMode;
    return QStringLiteral("anniversary");
}

void SettingsBackend::setAnnivMode(const QString& v)
{
    ConfigStore::CardData* c = current();
    if (!c)
        return;
    if (v != QLatin1String("countdown") && v != QLatin1String("anniversary")
        && v != QLatin1String("lunar"))
        return;
    if (v == c->annivMode)
        return;
    const QString old = c->annivMode;
    c->annivMode = v;
    // 顺手纠正"措辞不匹配"的默认事件名，免得出现"距离生日还有 8357 天"这种读不通的组合
    if (v == QLatin1String("lunar")) {
        if (c->lunarEvent.isEmpty())
            c->lunarEvent = QStringLiteral("chunjie");
        if (c->lunarEvent != QLatin1String("custom")) {
            const QString name = Festivals::instance().lunarEventName(c->lunarEvent);
            if (!name.isEmpty())
                c->eventName = QStringLiteral("距离%1还有").arg(name);
        }
    } else if (v == QLatin1String("anniversary")) {
        if (c->eventName.startsWith(QStringLiteral("距离")))
            c->eventName = kSuggestAnniversary.first();
    } else { // countdown
        if (c->eventName.contains(QStringLiteral("已经")))
            c->eventName = kSuggestCountdown.first();
    }
    qInfo().noquote() << "[settings] 纪念日模式:" << old << "→" << v << "事件名:" << c->eventName;
    emit anniversaryChanged();
    touchConfig();
}

// ==================== 农历节日模式 ====================
QString SettingsBackend::lunarEvent() const
{
    const ConfigStore::CardData* c = current();
    if (!c || c->lunarEvent.isEmpty())
        return QStringLiteral("chunjie");
    return c->lunarEvent;
}

QVariantList SettingsBackend::lunarEventLabels() const
{
    QVariantList out;
    for (const Festivals::LunarEvent& e : Festivals::instance().lunarEvents())
        out << (e.note.isEmpty() ? e.name : QStringLiteral("%1（%2）").arg(e.name, e.note));
    out << QStringLiteral("自定义农历日期");
    return out;
}

int SettingsBackend::lunarEventIndex() const
{
    const QList<Festivals::LunarEvent> evs = Festivals::instance().lunarEvents();
    const QString id = lunarEvent();
    if (id == QLatin1String("custom"))
        return evs.size(); // 末位
    for (int i = 0; i < evs.size(); ++i) {
        if (evs.at(i).id == id)
            return i;
    }
    return 0;
}

QVariantList SettingsBackend::lunarMonthLabels() const
{
    QVariantList out;
    for (const QString& s : kLunarMonthNames)
        out << s;
    return out;
}

QVariantList SettingsBackend::lunarDayLabels() const
{
    QVariantList out;
    for (const QString& s : kLunarDayNames)
        out << s;
    return out;
}

int SettingsBackend::lunarMonthIndex() const
{
    const ConfigStore::CardData* c = current();
    if (!c)
        return 0;
    return qBound(0, c->lunarMonth - 1, 11);
}

int SettingsBackend::lunarDayIndex() const
{
    const ConfigStore::CardData* c = current();
    if (!c)
        return 0;
    return qBound(0, c->lunarDay - 1, 29);
}

void SettingsBackend::setLunarEvent(const QString& v)
{
    ConfigStore::CardData* c = current();
    if (!c)
        return;
    const QString id = v.isEmpty() ? QStringLiteral("chunjie") : v;
    if (id == c->lunarEvent)
        return;
    c->lunarEvent = id;
    if (id != QLatin1String("custom")) {
        const QString name = Festivals::instance().lunarEventName(id);
        if (!name.isEmpty())
            c->eventName = QStringLiteral("距离%1还有").arg(name);
    }
    qInfo().noquote() << "[settings] 农历节日 →" << id << "事件名:" << c->eventName;
    emit anniversaryChanged();
    touchConfig();
}

void SettingsBackend::setLunarEventIndex(int v)
{
    const QList<Festivals::LunarEvent> evs = Festivals::instance().lunarEvents();
    if (v < 0 || v > evs.size())
        return;
    setLunarEvent(v == evs.size() ? QStringLiteral("custom") : evs.at(v).id);
}

void SettingsBackend::setLunarMonthIndex(int v)
{
    ConfigStore::CardData* c = current();
    if (!c)
        return;
    const int m = qBound(1, v + 1, 12);
    // 注意：即使数值没变也要切到"自定义"——用户从"春节"切到手动月/日时，
    // 月份可能恰好相同，这时如果早返回就不会切成自定义（下拉值也会显示回节日）
    const bool changed = (m != c->lunarMonth) || (c->lunarEvent != QLatin1String("custom"));
    c->lunarMonth = m;
    c->lunarEvent = QStringLiteral("custom");
    if (!changed)
        return;
    emit anniversaryChanged();
    touchConfig();
}

void SettingsBackend::setLunarDayIndex(int v)
{
    ConfigStore::CardData* c = current();
    if (!c)
        return;
    const int d = qBound(1, v + 1, 30);
    const bool changed = (d != c->lunarDay) || (c->lunarEvent != QLatin1String("custom"));
    c->lunarDay = d;
    c->lunarEvent = QStringLiteral("custom");
    if (!changed)
        return;
    emit anniversaryChanged();
    touchConfig();
}

// "卡片上会显示成什么" —— 用与卡片**完全同一套**口径（AnnivMath）算，避免两处算法漂移
QString SettingsBackend::effectPreview() const
{
    const ConfigStore::CardData* c = current();
    if (!c || !c->isAnniversary())
        return QString();
    const QDate today = dwEffectiveNow().date();
    const AnnivMath::Result r =
        isLunarMode() ? AnnivMath::computeLunar(lunarEvent(), c->lunarMonth, c->lunarDay, today)
                      : AnnivMath::compute(c->date, annivMode(), c->repeatMode, today);
    if (!r.valid)
        return isLunarMode() ? QStringLiteral("农历数据不足（表只覆盖 2025–2045），卡片会显示 “-”")
                             : QStringLiteral("日期无法识别，卡片会显示 “-”");
    return QStringLiteral("%1 %2 天").arg(c->eventName, QString::number(r.days));
}

QString SettingsBackend::dateWarning() const
{
    const ConfigStore::CardData* c = current();
    if (!c || !c->isAnniversary())
        return QString();
    const QDate today = dwEffectiveNow().date();

    if (isLunarMode()) {
        const AnnivMath::Result r =
            AnnivMath::computeLunar(lunarEvent(), c->lunarMonth, c->lunarDay, today);
        if (!r.valid)
            return QStringLiteral("农历表只覆盖 2025–2045 年，超出范围算不出来");
        if (r.days == 0)
            return QStringLiteral("按当前设置，今天就是这一天 🎉");
        return QString();
    }

    const QDate d = QDate::fromString(c->date, QStringLiteral("yyyy-MM-dd"));
    if (!d.isValid())
        return QStringLiteral("日期格式无法识别：可用 2027-3-5 / 2027/03/05 / 2027年3月5日");
    const AnnivMath::Result r = AnnivMath::compute(c->date, annivMode(), c->repeatMode, today);
    if (r.expired)
        return QStringLiteral("该日期已经过去，倒计时只会停在 0 天：请把重复规则改成「每年/每月」，"
                              "或把日期改到未来");
    if (isCountdownMode() && r.days > 365 * 5)
        return QStringLiteral("距离目标超过 5 年，确认一下年份有没有打错");
    if (!isCountdownMode() && d > today)
        return QStringLiteral("起始日在未来，「纪念日模式」会显示 0 天：这种情况请改用「倒计时模式」");
    return QString();
}

void SettingsBackend::setRepeatMode(const QString& v)
{
    ConfigStore::CardData* c = current();
    if (!c || v == c->repeatMode)
        return;
    for (const auto& p : kRepeatModes) {
        if (p.first == v) {
            c->repeatMode = v;
            emit anniversaryChanged();
            touchConfig();
            return;
        }
    }
}

void SettingsBackend::setRepeatIndex(int i)
{
    if (i < 0 || i >= kRepeatModes.size())
        return;
    setRepeatMode(kRepeatModes.at(i).first);
}

void SettingsBackend::setBgMode(const QString& v)
{
    ConfigStore::CardData* c = current();
    if (!c || v == c->bgMode)
        return;
    if (v != QLatin1String("color") && v != QLatin1String("image") && v != QLatin1String("moon"))
        return;
    c->bgMode = v;
    emit anniversaryChanged();
    touchConfig();
}

void SettingsBackend::applyEventSuggestion(const QString& v)
{
    setEventName(v);
}

void SettingsBackend::toggleWorkDay(int index)
{
    ConfigStore::CardData* c = current();
    if (!c || index < 0 || index > 6)
        return;
    const int day = index + 1;
    QList<int> wd = c->workDays;
    if (wd.contains(day))
        wd.removeAll(day);
    else
        wd.append(day);
    std::sort(wd.begin(), wd.end());
    c->workDays = wd;
    emit workDaysChanged();
    touchConfig();
}

void SettingsBackend::applyWorkdayPreset()
{
    ConfigStore::CardData* c = current();
    if (!c)
        return;
    c->workDays = QList<int>{1, 2, 3, 4, 5};
    emit workDaysChanged();
    touchConfig();
}

void SettingsBackend::addCard(const QString& type)
{
    if (!m_mgr)
        return;
    const int idx = m_mgr->addCard(type);
    if (idx < 0)
        return;
    emit cardsChanged();
    setCurrentIndex(idx);
    qInfo().noquote() << "[settings] 新增组件" << type << "→ 选中索引" << idx;
}

void SettingsBackend::removeCurrentCard()
{
    if (!m_mgr || !m_cfg)
        return;
    if (m_cfg->data().cards.size() <= 1) {
        qWarning().noquote() << "[settings] 只剩一张卡片，不能删";
        return;
    }
    const int idx = m_current;
    m_mgr->removeCard(idx);
    m_current = qBound(0, idx - 1, qMax(0, m_cfg->data().cards.size() - 1));
    emit cardsChanged();
    emit currentChanged();
    emitAll();
    qInfo().noquote() << "[settings] 已删除卡片" << idx << "，当前选中" << m_current;
}

void SettingsBackend::chooseBgImage()
{
    ConfigStore::CardData* c = current();
    if (!c)
        return;
    const QString start = c->bgImage.isEmpty() ? QDir::homePath() : QFileInfo(c->bgImage).absolutePath();
    const QString f = QFileDialog::getOpenFileName(
        nullptr, QStringLiteral("选择背景图片"), start,
        QStringLiteral("图片文件 (*.png *.jpg *.jpeg *.bmp *.webp *.gif)"));
    if (f.isEmpty())
        return;
    c->bgImage = QDir::toNativeSeparators(f);
    c->bgMode = QStringLiteral("image");
    emit anniversaryChanged();
    touchConfig();
    qInfo().noquote() << "[settings] 背景图 =" << c->bgImage;
}

void SettingsBackend::previewBgImage(const QString& path)
{
    if (path.isEmpty())
        return;
    ConfigStore::CardData* c = current();
    if (!c)
        return;
    c->bgImage = QDir::toNativeSeparators(path);
    c->bgMode = QStringLiteral("image");
    emit anniversaryChanged();
    touchConfig();
}

void SettingsBackend::clearBgImage()
{
    ConfigStore::CardData* c = current();
    if (!c)
        return;
    c->bgImage.clear();
    c->bgMode = QStringLiteral("color");
    emit anniversaryChanged();
    touchConfig();
}

void SettingsBackend::editPosition()
{
    emit closeRequested();
    if (m_mgr)
        m_mgr->setEditing(true);
}

void SettingsBackend::resetDefaults()
{
    ConfigStore::CardData* c = current();
    if (!c)
        return;
    const ConfigStore::CardData d = ConfigStore::defaultCard(c->type);
    // 位置/尺寸/名字保留，其余回默认（用户只是想把"内容"重置）
    const bool hasPos = c->hasPos;
    const int x = c->x, y = c->y, w = c->cardWidth;
    const QString name = c->cardName;
    *c = d;
    c->hasPos = hasPos;
    c->x = x;
    c->y = y;
    c->cardWidth = w;
    c->cardName = name;
    emitAll();
    emit cardWidthChanged();
    touchConfig();
    qInfo().noquote() << "[settings] 已恢复默认值（当前卡片）";
}

void SettingsBackend::closeWindow()
{
    emit closeRequested();
}

void SettingsBackend::openConfigFolder()
{
    if (!m_cfg)
        return;
    const QFileInfo fi(m_cfg->path());
    QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absolutePath()));
}
