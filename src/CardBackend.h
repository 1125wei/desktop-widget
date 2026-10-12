#pragma once

#include <QColor>
#include <QDate>
#include <QObject>
#include <QPoint>
#include <QString>
#include <QTime>
#include <QTimer>
#include <QUrl>
#include <QVariantList>

#include "ConfigStore.h"
#include "DesktopLayer.h"

class WidgetManager;
class QQuickWindow;

// 纪念日口径的唯一实现（**设置界面的效果预览与桌面卡片共用**，避免两处算法不一致）
namespace AnnivMath {
struct Result {
    bool valid = false;   // 日期字符串是否可解析
    QDate target;         // 实际参与计算的那一天
    int days = 0;         // 天数（非负）
    bool elapsed = false; // true = "已经 N 天"；false = "还有 N 天"
    bool expired = false; // 倒计时模式下：日期已过去且不重复 → 只能显示 0
};
// mode: "countdown"(还有多少天) | "anniversary"(已经多少天) | "lunar"(农历节日，永远算"还有多少天")
// repeat: "none" | "yearly" | "monthly"（仅倒计时模式有意义）
Result compute(const QString& dateText, const QString& mode, const QString& repeat, const QDate& today);
// 农历节日模式：eventId == "custom" 时用 lunarMonth/lunarDay；否则用具名节日（春节/除夕/…）
Result computeLunar(const QString& eventId, int lunarMonth, int lunarDay, const QDate& today);
} // namespace AnnivMath

// 单张卡片的数据后端（QML 里通过上下文属性 Card 注入）。
// 一个 CardBackend 对应 ConfigStore::cards[index] 与一个 QQuickView 窗口：
//   · 倒计时卡：title / countdown / miniCards（发薪、周五、下一个节日、今天赚了）
//   · 纪念日卡：eventName / bigNumber / unitText / dateText / 背景
// 所有口径在 recompute() 里按秒重算，QML 只负责画。
//
// 口径（已用参考截图逐位反推验证）：
//   纪念日天数 = 起始日期.daysTo(今天)，不含起始日（2003-11-25 → 2026-10-12 = 8357 ✓）
//   今天赚了   = 日收入 × 已过工作秒数 ÷ 工作总秒数，保留 3 位小数（100 → 18.099 ✓）
class CardBackend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString type READ type NOTIFY styleChanged)
    Q_PROPERTY(QString cardName READ cardName NOTIFY styleChanged)
    Q_PROPERTY(bool isCountdown READ isCountdown NOTIFY styleChanged)
    Q_PROPERTY(bool isAnniversary READ isAnniversary NOTIFY styleChanged)

    // —— 外观
    Q_PROPERTY(qreal cardWidth READ cardWidth NOTIFY styleChanged)
    Q_PROPERTY(QColor cardColor READ cardColor NOTIFY styleChanged)
    Q_PROPERTY(QColor fontColor READ fontColor NOTIFY styleChanged)
    Q_PROPERTY(QColor countdownColor READ countdownColor NOTIFY styleChanged)
    Q_PROPERTY(QColor miniCardColor READ miniCardColor NOTIFY styleChanged)
    Q_PROPERTY(QColor miniLabelColor READ miniLabelColor NOTIFY styleChanged)
    Q_PROPERTY(QColor miniValueColor READ miniValueColor NOTIFY styleChanged)
    Q_PROPERTY(QColor accentColor READ accentColor NOTIFY styleChanged)
    Q_PROPERTY(QString fontFamily READ fontFamily NOTIFY styleChanged)
    Q_PROPERTY(bool showMascot READ showMascot NOTIFY styleChanged)
    Q_PROPERTY(QString mascotMode READ mascotMode NOTIFY styleChanged)

    // —— 倒计时卡
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(QString countdown READ countdown NOTIFY countdownChanged)
    Q_PROPERTY(QVariantList miniCards READ miniCards NOTIFY miniCardsChanged)

    // —— 纪念日卡
    Q_PROPERTY(QString eventName READ eventName NOTIFY anniversaryChanged)
    Q_PROPERTY(QString bigNumber READ bigNumber NOTIFY anniversaryChanged)
    Q_PROPERTY(QString unitText READ unitText NOTIFY anniversaryChanged)
    Q_PROPERTY(QString dateText READ dateText NOTIFY anniversaryChanged)
    Q_PROPERTY(QString annivMode READ annivMode NOTIFY anniversaryChanged)
    Q_PROPERTY(QString repeatMode READ repeatMode NOTIFY anniversaryChanged)
    Q_PROPERTY(QString bgMode READ bgMode NOTIFY anniversaryChanged)
    Q_PROPERTY(QUrl bgImage READ bgImage NOTIFY anniversaryChanged)
    Q_PROPERTY(bool hasImage READ hasImage NOTIFY anniversaryChanged)

    // —— 交互（编辑模式是全局的，这里只读）
    Q_PROPERTY(bool editing READ editing NOTIFY editingChanged)

public:
    CardBackend(ConfigStore* cfg, int index, WidgetManager* mgr, QObject* parent = nullptr);

    void attachWindow(QQuickWindow* w) { m_window = w; }
    QQuickWindow* window() const { return m_window; }
    int index() const { return m_index; }
    void setIndex(int i) { m_index = i; }
    void reload(); // 配置改了 → 重读本卡字段 + 重算 + 通知 QML

    QString type() const { return m_type; }
    QString cardName() const { return m_cardName; }
    bool isCountdown() const { return m_type != QLatin1String("anniversary"); }
    bool isAnniversary() const { return m_type == QLatin1String("anniversary"); }

    qreal cardWidth() const { return m_cardWidth; }
    QColor cardColor() const { return m_cardColor; }
    QColor fontColor() const { return m_fontColor; }
    bool showMascot() const { return m_showMascot; }
    QString mascotMode() const { return m_mascotMode; }
    QColor countdownColor() const { return m_fontColor; }
    QColor miniCardColor() const { return m_miniCardColor; }
    QColor miniLabelColor() const
    {
        QColor c = m_fontColor;
        c.setAlphaF(0.78);
        return c;
    }
    QColor miniValueColor() const { return m_fontColor.darker(115); }
    QColor accentColor() const { return QColor(QStringLiteral("#2F88FF")); }
    QString fontFamily() const { return m_fontFamily; }

    QString title() const { return m_displayTitle; }
    QString countdown() const { return m_countdown; }
    QVariantList miniCards() const { return m_miniCards; }

    QString eventName() const { return m_eventName; }
    QString bigNumber() const { return m_bigNumber; }
    QString unitText() const { return QStringLiteral("天"); }
    QString dateText() const { return m_dateText; }
    QString annivMode() const { return m_annivMode; }
    QString repeatMode() const { return m_repeatMode; }
    QString bgMode() const { return m_bgMode; }
    QUrl bgImage() const;
    bool hasImage() const { return !m_bgImage.isEmpty(); }

    bool editing() const;

    // —— 交互（桌面卡片窗口用）
    Q_INVOKABLE void startDrag();
    Q_INVOKABLE void dragMove();
    Q_INVOKABLE void savePosition();
    Q_INVOKABLE void resetPosition();
    Q_INVOKABLE void finishEditing();

signals:
    void styleChanged();
    void titleChanged();
    void countdownChanged();
    void miniCardsChanged();
    void anniversaryChanged();
    void editingChanged();

private:
    void applyConfig();
    void recompute();
    QDate nextPayday(const QDate& today) const;
    QDate nextFriday(const QDate& today) const;
    QString earningsText(const QDateTime& now) const;
    void recomputeAnniversary(const QDate& today);

    QQuickWindow* m_window = nullptr;
    WidgetManager* m_mgr = nullptr;
    ConfigStore* m_cfg = nullptr;
    int m_index = 0;
    QTimer m_timer;
    QPoint m_dragOffset;

    QString m_type = QStringLiteral("countdown");
    QString m_cardName;
    qreal m_cardWidth = 312;
    QColor m_cardColor{QStringLiteral("#FFFFFF")};
    QColor m_fontColor{QStringLiteral("#3A3A3A")};
    QColor m_miniCardColor{QStringLiteral("#F2F2F2")};
    QString m_fontFamily = QStringLiteral("Microsoft YaHei UI");

    QString m_titleText = QStringLiteral("下班还有"); // 配置里的"卡片文案"
    QString m_displayTitle = QStringLiteral("下班还有"); // 运行态标题（休息/已下班 会覆盖）
    QString m_countdown = QStringLiteral("--:--:--");
    QVariantList m_miniCards;
    QList<int> m_workDays{1, 2, 3, 4, 5};
    QTime m_workStart{8, 30};
    QTime m_workEnd{17, 30};
    int m_payday = 25;
    double m_dailyIncome = 100.0;
    bool m_showPayday = true;
    bool m_showFriday = true;
    bool m_showHoliday = true;
    bool m_showEarnings = true;
    bool m_showMascot = true;
    QString m_mascotMode = QStringLiteral("image");

    QString m_eventName = QStringLiteral("你在世界已经");
    QString m_dateConfig = QStringLiteral("2003-11-25"); // 配置里的原始日期
    QString m_dateText = QStringLiteral("2003-11-25");   // 卡片上显示的日期（倒计时+循环时是下一次）
    QString m_annivMode = QStringLiteral("anniversary");
    QString m_lunarEvent = QStringLiteral("chunjie");
    int m_lunarMonth = 1;
    int m_lunarDay = 1;
    QString m_repeatMode = QStringLiteral("none");
    QString m_bgMode = QStringLiteral("color");
    QString m_bgImage;
    QString m_bigNumber = QStringLiteral("0");
};
