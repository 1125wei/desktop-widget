#pragma once

#include <QDate>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QVector>

// 节日/放假日历。
//
// 数据来自 :/assets/festivals.json（由 tools/gen_festivals.py 离线生成 2025–2045），
// 文件缺失时退回内置的一小张表，保证程序永远可用。
//   festivals[]  {date, name, kind}  kind: legal(法定) / traditional(传统, 可能不放假) / common(常见节日)
//   offDays[]    官方放假日（含调休连休）
//   makeupWorkdays[] 调休补班日（周末上班）
//   officialYears[]  **国务院已公布**放假安排的年份（只有这些年份才有调休数据）
class Festivals {
public:
    struct Entry {
        QDate date;
        QString name;
        QString kind;
    };

    // 农历节日（设置界面"农历节日模式"用）
    struct LunarEvent {
        QString id;
        QString name;
        int month = 0; // 农历月 1-12
        int day = 0;   // 农历日 1-30；0 = 腊月最后一天（除夕）
        QString note;
    };

    static Festivals& instance();

    // 下一个节日：严格晚于今天的最近一个；同日多个节日时优先 traditional/legal
    Entry nextAfter(const QDate& today) const;

    // 是否休息日：官方年份按官方放假表（含调休补班），未来年份按法定假日 + 周末兜底
    bool isRest(const QDate& d, const QList<int>& manualWorkDays) const;

    bool isOffDay(const QDate& d) const;
    bool hasOfficialYear(int year) const;
    int count() const { return m_entries.size(); }
    QString source() const { return m_source; }

    // —— 农历（数据来自离线生成的 lunarMonths / lunarEvents）——
    QList<LunarEvent> lunarEvents() const { return m_lunarEvents; }
    QString lunarEventName(const QString& id) const;
    // 农历(月,日) → 不早于 from 的下一个公历日期。闰月按非闰月算（节日一律取非闰月），
    // 该月只有 29 天而请求 30 号时退到月末。无数据（超出表范围）返回无效 QDate。
    QDate nextLunarDate(int month, int day, const QDate& from) const;
    // 具名农历节日（含除夕 = 腊月最后一天）
    QDate nextLunarEvent(const QString& id, const QDate& from) const;
    int lunarMonthCount() const { return m_lunarMonths.size(); }

private:
    Festivals();
    void loadFromJson(const QString& path, bool& ok);
    void loadBuiltinFallback();

    struct LunarMonthFirst {
        int year = 0;
        int month = 0;
        bool leap = false;
        QDate first;
    };

    QVector<Entry> m_entries;      // 已按日期排序
    QList<QDate> m_offDays;
    QList<QDate> m_makeupDays;
    QList<int> m_officialYears;
    QVector<LunarMonthFirst> m_lunarMonths; // 已按公历日期排序
    QList<LunarEvent> m_lunarEvents;
    QString m_source = QStringLiteral("builtin");
};

// 统一的"当前时间"入口：DW_FREEZE=2026-10-12T11:00:00 可钉死时间，让渲染/口径完全可复现
QDateTime dwEffectiveNow();
