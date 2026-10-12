#include "ConfigStore.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace {
int clampWidth(int w)
{
    return qBound(240, w, 760);
}

QJsonObject cardToJson(const ConfigStore::CardData& c)
{
    QJsonObject o;
    o.insert(QStringLiteral("type"), c.type);
    o.insert(QStringLiteral("cardName"), c.cardName);
    o.insert(QStringLiteral("hasPos"), c.hasPos);
    o.insert(QStringLiteral("x"), c.x);
    o.insert(QStringLiteral("y"), c.y);
    o.insert(QStringLiteral("cardWidth"), c.cardWidth);
    o.insert(QStringLiteral("cardColor"), c.cardColor);
    o.insert(QStringLiteral("fontColor"), c.fontColor);
    o.insert(QStringLiteral("miniCardColor"), c.miniCardColor);
    o.insert(QStringLiteral("fontFamily"), c.fontFamily);

    o.insert(QStringLiteral("title"), c.title);
    QJsonArray wd;
    for (int v : c.workDays)
        wd.append(v);
    o.insert(QStringLiteral("workDays"), wd);
    o.insert(QStringLiteral("workStart"), c.workStart);
    o.insert(QStringLiteral("workEnd"), c.workEnd);
    o.insert(QStringLiteral("payday"), c.payday);
    o.insert(QStringLiteral("dailyIncome"), c.dailyIncome);
    o.insert(QStringLiteral("showPayday"), c.showPayday);
    o.insert(QStringLiteral("showFriday"), c.showFriday);
    o.insert(QStringLiteral("showHoliday"), c.showHoliday);
    o.insert(QStringLiteral("showEarnings"), c.showEarnings);
    o.insert(QStringLiteral("showMascot"), c.showMascot);
    o.insert(QStringLiteral("mascotMode"), c.mascotMode);

    o.insert(QStringLiteral("eventName"), c.eventName);
    o.insert(QStringLiteral("date"), c.date);
    o.insert(QStringLiteral("annivMode"), c.annivMode);
    o.insert(QStringLiteral("lunarEvent"), c.lunarEvent);
    o.insert(QStringLiteral("lunarMonth"), c.lunarMonth);
    o.insert(QStringLiteral("lunarDay"), c.lunarDay);
    o.insert(QStringLiteral("repeatMode"), c.repeatMode);
    o.insert(QStringLiteral("bgMode"), c.bgMode);
    o.insert(QStringLiteral("bgImage"), c.bgImage);
    return o;
}

// 逐个字段读，缺字段就用 fallback（= 当前值），保证向前/向后兼容
ConfigStore::CardData cardFromJson(const QJsonObject& o, ConfigStore::CardData c)
{
    c.type = o.value(QStringLiteral("type")).toString(c.type);
    c.cardName = o.value(QStringLiteral("cardName")).toString(c.cardName);
    c.hasPos = o.value(QStringLiteral("hasPos")).toBool(c.hasPos);
    c.x = o.value(QStringLiteral("x")).toInt(c.x);
    c.y = o.value(QStringLiteral("y")).toInt(c.y);
    c.cardWidth = clampWidth(o.value(QStringLiteral("cardWidth")).toInt(c.cardWidth));
    c.cardColor = o.value(QStringLiteral("cardColor")).toString(c.cardColor);
    c.fontColor = o.value(QStringLiteral("fontColor")).toString(c.fontColor);
    c.miniCardColor = o.value(QStringLiteral("miniCardColor")).toString(c.miniCardColor);
    c.fontFamily = o.value(QStringLiteral("fontFamily")).toString(c.fontFamily);

    c.title = o.value(QStringLiteral("title")).toString(c.title);
    if (o.contains(QStringLiteral("workDays"))) {
        QList<int> wd;
        for (const QJsonValue& v : o.value(QStringLiteral("workDays")).toArray()) {
            const int day = v.toInt();
            if (day >= 1 && day <= 7)
                wd.append(day);
        }
        if (!wd.isEmpty())
            c.workDays = wd;
    }
    c.workStart = o.value(QStringLiteral("workStart")).toString(c.workStart);
    c.workEnd = o.value(QStringLiteral("workEnd")).toString(c.workEnd);
    c.payday = o.value(QStringLiteral("payday")).toInt(c.payday);
    c.dailyIncome = o.value(QStringLiteral("dailyIncome")).toDouble(c.dailyIncome);
    c.showPayday = o.value(QStringLiteral("showPayday")).toBool(c.showPayday);
    c.showFriday = o.value(QStringLiteral("showFriday")).toBool(c.showFriday);
    c.showHoliday = o.value(QStringLiteral("showHoliday")).toBool(c.showHoliday);
    c.showEarnings = o.value(QStringLiteral("showEarnings")).toBool(c.showEarnings);
    c.showMascot = o.value(QStringLiteral("showMascot")).toBool(c.showMascot);
    c.mascotMode = o.value(QStringLiteral("mascotMode")).toString(c.mascotMode);
    if (c.mascotMode != QLatin1String("image") && c.mascotMode != QLatin1String("vector"))
        c.mascotMode = QStringLiteral("image");

    c.eventName = o.value(QStringLiteral("eventName")).toString(c.eventName);
    c.date = o.value(QStringLiteral("date")).toString(c.date);
    c.annivMode = o.value(QStringLiteral("annivMode")).toString(c.annivMode);
    if (c.annivMode != QLatin1String("countdown") && c.annivMode != QLatin1String("anniversary")
        && c.annivMode != QLatin1String("lunar"))
        c.annivMode = QStringLiteral("anniversary");
    c.lunarEvent = o.value(QStringLiteral("lunarEvent")).toString(c.lunarEvent);
    c.lunarMonth = o.value(QStringLiteral("lunarMonth")).toInt(c.lunarMonth);
    c.lunarDay = o.value(QStringLiteral("lunarDay")).toInt(c.lunarDay);
    c.repeatMode = o.value(QStringLiteral("repeatMode")).toString(c.repeatMode);
    c.bgMode = o.value(QStringLiteral("bgMode")).toString(c.bgMode);
    c.bgImage = o.value(QStringLiteral("bgImage")).toString(c.bgImage);
    return c;
}
} // namespace

ConfigStore::CardData ConfigStore::defaultCard(const QString& type)
{
    CardData c;
    c.type = type;
    if (type == QLatin1String("anniversary")) {
        c.cardName = QStringLiteral("纪念日");
        c.cardColor = QStringLiteral("#16283C");  // 深蓝夜色（bgMode=color 时的底）
        c.fontColor = QStringLiteral("#FFFFFF");
        c.fontFamily = QStringLiteral("Microsoft YaHei UI");
        c.eventName = QStringLiteral("你在世界已经");
        c.date = QStringLiteral("2003-11-25");
        c.annivMode = QStringLiteral("anniversary"); // 默认"已经多少天"（与参考截图一致）
        c.repeatMode = QStringLiteral("none");
        c.bgMode = QStringLiteral("moon");       // 默认用内置月夜渐变（无需外部图片）
    } else {
        c.cardName = QStringLiteral("下班倒计时");
        c.type = QStringLiteral("countdown");
    }
    return c;
}

ConfigStore::ConfigStore(QObject* parent)
    : QObject(parent)
{
}

QString ConfigStore::path() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/config.json");
}

bool ConfigStore::ensureCards()
{
    if (!m_data.cards.isEmpty())
        return false;
    m_data.cards.append(defaultCard(QStringLiteral("countdown")));
    return true;
}

bool ConfigStore::load()
{
    QFile f(path());
    if (!f.exists())
        return false;
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "config.json 打开失败:" << f.errorString();
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) {
        qWarning() << "config.json 不是合法 JSON，忽略";
        return false;
    }
    const QJsonObject o = doc.object();

    Data d;
    d.layerMode = o.value(QStringLiteral("layerMode")).toString(d.layerMode);
    d.visible = o.value(QStringLiteral("visible")).toBool(d.visible);

    if (o.contains(QStringLiteral("cards"))) {
        // v2：卡片列表
        for (const QJsonValue& v : o.value(QStringLiteral("cards")).toArray()) {
            if (!v.isObject())
                continue;
            const QJsonObject co = v.toObject();
            CardData c = defaultCard(co.value(QStringLiteral("type")).toString(QStringLiteral("countdown")));
            d.cards.append(cardFromJson(co, c));
        }
    }
    if (d.cards.isEmpty()) {
        // v1 迁移：老配置只有一张卡片，字段是扁平的
        CardData c = defaultCard(QStringLiteral("countdown"));
        if (o.contains(QStringLiteral("title")) || o.contains(QStringLiteral("workStart"))) {
            c = cardFromJson(o, c);
            c.type = QStringLiteral("countdown");
            c.cardName = QStringLiteral("下班倒计时");
            qInfo().noquote() << "[migrate] 检测到 v1 扁平配置 → 迁移为 cards[0] 倒计时卡"
                              << "位置" << c.x << c.y << "宽度" << c.cardWidth;
        }
        d.cards.append(c);
    }

    m_data = d;
    qInfo().noquote() << "配置已加载:" << path() << "卡片数:" << m_data.cards.size();
    return true;
}

bool ConfigStore::save() const
{
    QJsonObject o;
    o.insert(QStringLiteral("version"), 2);
    o.insert(QStringLiteral("layerMode"), m_data.layerMode);
    o.insert(QStringLiteral("visible"), m_data.visible);

    QJsonArray cards;
    for (const CardData& c : m_data.cards)
        cards.append(cardToJson(c));
    o.insert(QStringLiteral("cards"), cards);

    QFile f(path());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        qWarning() << "config.json 写入失败:" << f.errorString();
        return false;
    }
    f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
    qInfo().noquote() << "配置已保存:" << path() << "卡片数:" << m_data.cards.size();
    return true;
}
