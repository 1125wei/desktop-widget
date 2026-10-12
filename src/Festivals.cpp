#include "Festivals.h"

#include <QDebug>
#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

QDateTime dwEffectiveNow()
{
    const QString s = qEnvironmentVariable("DW_FREEZE");
    if (!s.isEmpty()) {
        const QDateTime d = QDateTime::fromString(s, Qt::ISODate);
        if (d.isValid())
            return d;
    }
    return QDateTime::currentDateTime();
}

namespace {
int kindRank(const QString& kind)
{
    if (kind == QLatin1String("legal"))
        return 0;
    if (kind == QLatin1String("traditional"))
        return 1;
    return 2;
}

QList<QDate> datesFromArray(const QJsonArray& a)
{
    QList<QDate> out;
    for (const QJsonValue& v : a) {
        const QDate d = QDate::fromString(v.toString(), QStringLiteral("yyyy-MM-dd"));
        if (d.isValid())
            out.append(d);
    }
    return out;
}
} // namespace

Festivals& Festivals::instance()
{
    static Festivals f;
    return f;
}

Festivals::Festivals()
{
    bool ok = false;
    loadFromJson(QStringLiteral(":/assets/festivals.json"), ok);
    if (!ok || m_entries.isEmpty()) {
        qWarning().noquote() << "[festivals] 资源表不可用 → 使用内置兜底表";
        loadBuiltinFallback();
        return;
    }
    std::sort(m_entries.begin(), m_entries.end(), [](const Entry& a, const Entry& b) {
        if (a.date != b.date)
            return a.date < b.date;
        return kindRank(a.kind) < kindRank(b.kind);
    });
    qInfo().noquote() << "[festivals] 已载入" << m_entries.size() << "个节日，放假日"
                      << m_offDays.size() << "天，调休补班" << m_makeupDays.size() << "天，官方年份"
                      << m_officialYears << "农历月初" << m_lunarMonths.size() << "条/农历节日"
                      << m_lunarEvents.size() << "个，来源" << m_source;
}

void Festivals::loadFromJson(const QString& path, bool& ok)
{
    QFile f(path);
    if (!f.exists() || !f.open(QIODevice::ReadOnly)) {
        qWarning().noquote() << "[festivals] 打不开" << path;
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
        return;
    const QJsonObject o = doc.object();
    for (const QJsonValue& v : o.value(QStringLiteral("festivals")).toArray()) {
        const QJsonObject e = v.toObject();
        Entry en;
        en.date = QDate::fromString(e.value(QStringLiteral("date")).toString(), QStringLiteral("yyyy-MM-dd"));
        en.name = e.value(QStringLiteral("name")).toString();
        en.kind = e.value(QStringLiteral("kind")).toString(QStringLiteral("common"));
        if (en.date.isValid() && !en.name.isEmpty())
            m_entries.append(en);
    }
    m_offDays = datesFromArray(o.value(QStringLiteral("offDays")).toArray());
    m_makeupDays = datesFromArray(o.value(QStringLiteral("makeupWorkdays")).toArray());
    for (const QJsonValue& v : o.value(QStringLiteral("officialYears")).toArray())
        m_officialYears.append(v.toInt());

    // 农历月表（任意农历月/日 → 公历）
    for (const QJsonValue& v : o.value(QStringLiteral("lunarMonths")).toArray()) {
        const QJsonObject e = v.toObject();
        LunarMonthFirst mm;
        mm.year = e.value(QStringLiteral("y")).toInt();
        mm.month = e.value(QStringLiteral("m")).toInt();
        mm.leap = e.value(QStringLiteral("leap")).toBool();
        mm.first = QDate::fromString(e.value(QStringLiteral("solar")).toString(),
                                     QStringLiteral("yyyy-MM-dd"));
        if (mm.first.isValid() && mm.month >= 1 && mm.month <= 12)
            m_lunarMonths.append(mm);
    }
    std::sort(m_lunarMonths.begin(), m_lunarMonths.end(),
              [](const LunarMonthFirst& a, const LunarMonthFirst& b) { return a.first < b.first; });

    // 农历节日清单
    for (const QJsonValue& v : o.value(QStringLiteral("lunarEvents")).toArray()) {
        const QJsonObject e = v.toObject();
        LunarEvent le;
        le.id = e.value(QStringLiteral("id")).toString();
        le.name = e.value(QStringLiteral("name")).toString();
        le.month = e.value(QStringLiteral("month")).toInt();
        le.day = e.value(QStringLiteral("day")).toInt();
        le.note = e.value(QStringLiteral("note")).toString();
        if (!le.id.isEmpty() && !le.name.isEmpty())
            m_lunarEvents.append(le);
    }

    m_source = o.value(QStringLiteral("generated")).toString(QStringLiteral("assets/festivals.json"));
    ok = !m_entries.isEmpty();
}

QString Festivals::lunarEventName(const QString& id) const
{
    for (const LunarEvent& e : m_lunarEvents) {
        if (e.id == id)
            return e.name;
    }
    return QString();
}

QDate Festivals::nextLunarDate(int month, int day, const QDate& from) const
{
    if (month < 1 || month > 12 || day < 1 || day > 30)
        return {};
    for (int i = 0; i < m_lunarMonths.size(); ++i) {
        const LunarMonthFirst& mm = m_lunarMonths.at(i);
        if (mm.month != month || mm.leap)
            continue; // 节日一律取非闰月
        QDate cand = mm.first.addDays(day - 1);
        // 该农历月只有 29 天却请求 30 号 → 退到月末（下一月初一 减一天）
        if (i + 1 < m_lunarMonths.size()) {
            const QDate nextFirst = m_lunarMonths.at(i + 1).first;
            if (nextFirst.isValid() && cand >= nextFirst)
                cand = nextFirst.addDays(-1);
        }
        if (cand >= from)
            return cand;
    }
    return {}; // 超出农历表范围
}

QDate Festivals::nextLunarEvent(const QString& id, const QDate& from) const
{
    // 除夕 = 春节前一天（腊月廿九或三十，随年份变）
    if (id == QLatin1String("chuxi")) {
        for (const LunarMonthFirst& mm : m_lunarMonths) {
            if (mm.month != 1 || mm.leap)
                continue;
            const QDate chuxi = mm.first.addDays(-1);
            if (chuxi >= from)
                return chuxi;
        }
        return {};
    }
    for (const LunarEvent& e : m_lunarEvents) {
        if (e.id == id)
            return nextLunarDate(e.month, e.day, from);
    }
    return {};
}

void Festivals::loadBuiltinFallback()
{
    const Entry builtin[] = {
        {QDate(2026, 10, 18), QStringLiteral("重阳节"), QStringLiteral("traditional")},
        {QDate(2026, 12, 22), QStringLiteral("冬至"), QStringLiteral("traditional")},
        {QDate(2027, 1, 1), QStringLiteral("元旦"), QStringLiteral("legal")},
        {QDate(2027, 2, 6), QStringLiteral("春节"), QStringLiteral("legal")},
    };
    for (const Entry& e : builtin)
        m_entries.append(e);
    m_source = QStringLiteral("builtin(4)");
}

Festivals::Entry Festivals::nextAfter(const QDate& today) const
{
    for (const Entry& e : m_entries) {
        if (e.date > today)
            return e;
    }
    // 表尾之后（超出生成范围）：用元旦兜底，避免显示负数
    Entry e;
    e.date = QDate(today.year() + 1, 1, 1);
    e.name = QStringLiteral("元旦");
    e.kind = QStringLiteral("legal");
    return e;
}

bool Festivals::isOffDay(const QDate& d) const
{
    return m_offDays.contains(d);
}

bool Festivals::hasOfficialYear(int year) const
{
    return m_officialYears.contains(year);
}

bool Festivals::isRest(const QDate& d, const QList<int>& manualWorkDays) const
{
    // 国务院已公布放假安排的年份：完全按官方表（能正确处理调休补班）
    if (hasOfficialYear(d.year())) {
        if (m_makeupDays.contains(d))
            return false; // 补班日：周末也上班
        if (m_offDays.contains(d))
            return true;
        return d.dayOfWeek() >= 6;
    }
    // 未来年份：法定节假日照休（不含调休），其余按用户自配的工作日
    if (m_offDays.contains(d))
        return true;
    return !manualWorkDays.contains(d.dayOfWeek());
}
