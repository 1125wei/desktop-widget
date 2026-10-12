#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
生成 src 用的节日/放假日历：assets/festivals.json

为什么要离线生成：农历节日（春节/端午/中秋/重阳/除夕）和节气（清明/冬至）需要历法计算，
程序里不引第三方库，所以用 cnlunar 一次性算好 2025–2045 打成资源。

数据来源：
  1) 农历与节气：cnlunar（纯 python，1900–2100 可靠）
  2) 官方放假与**调休补班**：NateScarlet/holiday-cn（国务院每年公布后更新）。
     ⚠️ 只有已公布的年份才有调休数据 → 只有这些年份会被 C++ 侧当作"官方年份"，
     其余年份退回"法定假日 + 用户自配工作日"。

用法：python tools/gen_festivals.py            # 生成 assets/festivals.json
      python tools/gen_festivals.py --verify   # 只打印几个已知日期做校验

⚠️ 耗时：cnlunar 是**逐日**算农历的，生成 21 年（含"每个月初一"表）要扫 ~7 万天，
   实测约 10 分钟。这是离线一次性工具，慢点没关系，别以为卡死了。
"""
import datetime
import json
import os
import sys
import urllib.request

try:
    import cnlunar
except ImportError:
    sys.exit("需要 cnlunar：python -m pip install cnlunar -i https://pypi.tuna.tsinghua.edu.cn/simple")

YEARS = range(2025, 2046)
OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "assets", "festivals.json")

# 固定公历节日： (月, 日, 名称, 类别)
FIXED = [
    (1, 1, "元旦", "legal"),
    (2, 14, "情人节", "common"),
    (3, 8, "妇女节", "common"),
    (5, 1, "劳动节", "legal"),
    (6, 1, "儿童节", "common"),
    (9, 10, "教师节", "common"),
    (10, 1, "国庆节", "legal"),
    (10, 31, "万圣节", "common"),
    (11, 11, "双十一", "common"),
    (12, 25, "圣诞节", "common"),
]

# 农历节日： (月, 日, 名称, 类别)   除夕单独处理（腊月最后一天 = 春节前一天）
LUNAR = [
    (1, 1, "春节", "legal"),
    (1, 15, "元宵节", "traditional"),
    (5, 5, "端午节", "legal"),
    (7, 7, "七夕节", "traditional"),
    (7, 15, "中元节", "traditional"),
    (8, 15, "中秋节", "legal"),
    (9, 9, "重阳节", "traditional"),
    (12, 8, "腊八节", "traditional"),
]

# 供"农历节日模式"下拉用的清单：(id, 显示名, 农历月, 农历日, 备注)
#   除夕没有固定农历日（腊月廿九或三十）→ 用 day=0 表示"腊月最后一天"
LUNAR_EVENTS = [
    ("chunjie", "春节", 1, 1, ""),
    ("yuanxiao", "元宵节", 1, 15, ""),
    ("longtai", "龙抬头", 2, 2, ""),
    ("duanwu", "端午节", 5, 5, ""),
    ("qixi", "七夕节", 7, 7, ""),
    ("zhongyuan", "中元节", 7, 15, ""),
    ("zhongqiu", "中秋节", 8, 15, ""),
    ("chongyang", "重阳节", 9, 9, ""),
    ("labai", "腊八节", 12, 8, ""),
    ("xiaonian", "小年", 12, 23, ""),
    ("chuxi", "除夕", 12, 0, "腊月最后一天"),
]

# 节气（公历节日，但日期每年变）： (节气名, 显示名, 类别)
TERMS = [
    ("清明", "清明节", "legal"),
    ("冬至", "冬至", "traditional"),
]

# 法定放假核心日（用于给"没有官方数据的年份"兜底：这些日子照休）
LEGAL_CORE = {"元旦", "春节", "清明节", "劳动节", "端午节", "中秋节", "国庆节"}


def solar_terms(year):
    """返回 {节气名: (month, day)}"""
    probe = cnlunar.Lunar(datetime.datetime(year, 6, 15), godType="8char")
    return dict(probe.thisYearSolarTermsDic)


def lunar_days_to_month_first_solar(year):
    """扫全年，找出 (农历月, 农历日) → 公历日期（非闰月）"""
    found = {}
    d = datetime.datetime(year, 1, 1)
    while d.year == year:
        l = cnlunar.Lunar(d, godType="8char")
        if not l.isLunarLeapMonth:
            found.setdefault((l.lunarMonth, l.lunarDay), d.date())
        d += datetime.timedelta(days=1)
    return found


def lunar_month_firsts(years):
    """每个**农历年**的每个月初一 → 公历日期（含闰月）。

    为什么要扫农历年而不是公历年：腊月/正月跨公历年，按公历年扫会漏掉月初。
    返回 [{"y":农历年,"m":月,"leap":是否闰月,"solar":"YYYY-MM-DD"}]
    """
    out = []
    for y in years:
        d = datetime.datetime(y - 1, 11, 1)
        end = datetime.datetime(y + 1, 3, 1)
        seen = set()
        while d < end:
            l = cnlunar.Lunar(d, godType="8char")
            if l.lunarYear == y and l.lunarDay == 1:
                key = (l.lunarMonth, bool(l.isLunarLeapMonth))
                if key not in seen:
                    seen.add(key)
                    out.append({"y": y, "m": l.lunarMonth, "leap": bool(l.isLunarLeapMonth),
                                "solar": d.date().isoformat()})
            d += datetime.timedelta(days=1)
    return sorted(out, key=lambda x: x["solar"])


def build_year(year):
    out = []
    for m, d, name, kind in FIXED:
        out.append((datetime.date(year, m, d), name, kind))

    lunar = lunar_days_to_month_first_solar(year)
    for m, d, name, kind in LUNAR:
        dt = lunar.get((m, d))
        if dt:
            out.append((dt, name, kind))
    chunjie = lunar.get((1, 1))
    if chunjie:
        out.append((chunjie - datetime.timedelta(days=1), "除夕", "traditional"))

    terms = solar_terms(year)
    for key, name, kind in TERMS:
        if key in terms:
            m, d = terms[key]
            out.append((datetime.date(year, m, d), name, kind))

    # 母亲节 = 5 月第 2 个周日；父亲节 = 6 月第 3 个周日
    def nth_sunday(month, n):
        d = datetime.date(year, month, 1)
        d += datetime.timedelta(days=(6 - d.weekday()) % 7)  # 第一个周日
        return d + datetime.timedelta(days=7 * (n - 1))

    out.append((nth_sunday(5, 2), "母亲节", "common"))
    out.append((nth_sunday(6, 3), "父亲节", "common"))
    return sorted(set(out), key=lambda x: (x[0], x[1]))


def fetch_official(year):
    """抓官方放假安排（含调休补班）。多镜像依次尝试，失败返回 None。"""
    urls = [
        f"https://cdn.jsdelivr.net/gh/NateScarlet/holiday-cn@master/{year}.json",
        f"https://raw.githubusercontent.com/NateScarlet/holiday-cn/master/{year}.json",
        f"https://ghproxy.net/https://raw.githubusercontent.com/NateScarlet/holiday-cn/master/{year}.json",
    ]
    for u in urls:
        try:
            req = urllib.request.Request(u, headers={"User-Agent": "Mozilla/5.0 DesktopWidget/1.0"})
            with urllib.request.urlopen(req, timeout=12) as r:
                data = json.loads(r.read().decode("utf-8"))
            days = [d for d in data.get("days", []) if d.get("date")]
            if days:
                print(f"  [官方] {year}: {len(days)} 天  ← {u.split('/')[2]}")
                return days
        except Exception as e:  # noqa: BLE001
            print(f"  [官方] {year} 失败 {u.split('/')[2]}: {type(e).__name__}")
    return None


def main():
    if "--verify" in sys.argv:
        for y, m, d, name in [(2026, 2, 17, "春节"), (2026, 6, 19, "端午"),
                              (2026, 9, 25, "中秋"), (2026, 10, 18, "重阳"),
                              (2025, 1, 29, "春节"), (2024, 2, 10, "春节")]:
            l = cnlunar.Lunar(datetime.datetime(y, m, d), godType="8char")
            print(f"{y}-{m:02d}-{d:02d} 农历{l.lunarMonth}月{l.lunarDay}日 → 期望 {name}")
        return

    festivals, off_days, makeup, official_years = [], [], [], []

    for year in YEARS:
        for dt, name, kind in build_year(year):
            festivals.append({"date": dt.isoformat(), "name": name, "kind": kind})

    print("抓取官方放假安排（含调休）：")
    for year in YEARS:
        days = fetch_official(year)
        if not days:
            continue
        official_years.append(year)
        for d in days:
            if d.get("isOffDay", True):
                off_days.append(d["date"])
            else:
                makeup.append(d["date"])

    # 没有官方数据的年份：法定节假日核心日照休（不含调休，交回用户自配工作日）
    core_names = LEGAL_CORE
    for f in festivals:
        if f["kind"] == "legal" and f["name"] in core_names:
            off_days.append(f["date"])

    all_off = sorted(set(off_days))
    all_makeup = sorted(set(makeup))

    print("生成农历月表（任意农历月/日 → 公历）...")
    lunar_months = lunar_month_firsts(range(min(YEARS), max(YEARS) + 1))
    print(f"  {len(lunar_months)} 条月初（含闰月）")

    data = {
        "generated": f"gen_festivals.py {datetime.date.today().isoformat()} (cnlunar + holiday-cn)",
        "range": [min(YEARS), max(YEARS)],
        "festivals": festivals,
        "offDays": all_off,
        "makeupWorkdays": all_makeup,
        "officialYears": sorted(official_years),
        # 农历月表：y=农历年 m=农历月 leap=是否闰月 solar=该月初一的公历日期
        "lunarMonths": lunar_months,
        # 农历节日清单（设置界面下拉用）
        "lunarEvents": [{"id": i, "name": n, "month": m, "day": dd, "note": note}
                        for i, n, m, dd, note in LUNAR_EVENTS],
    }
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as fp:
        json.dump(data, fp, ensure_ascii=False, indent=1)

    print(f"\n写出 {OUT}")
    print(f"  节日 {len(festivals)} 条 / 放假日 {len(all_off)} 天 / 调休补班 {len(all_makeup)} 天")
    print(f"  官方年份（有调休数据）: {sorted(official_years)}")
    print("  抽查 2026 年节日：")
    for f in festivals:
        if f["date"].startswith("2026"):
            print("   ", f["date"], f["name"], f["kind"])


if __name__ == "__main__":
    main()
