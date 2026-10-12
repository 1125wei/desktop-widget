#pragma once

#include <QColor>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantList>

#include "ConfigStore.h" // 需要 CardData 的完整定义（private 方法返回它）

class WidgetManager;

// 设置界面的数据层：**按「当前选中的卡片」**读写。
// 每个字段改动都 ①写进 ConfigStore ②立刻应用到桌面组件 ③防抖落盘
// （对应参考 App 那个"改完即时预览"的手感）
class SettingsBackend : public QObject {
    Q_OBJECT
    // —— 卡片选择
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentChanged)
    Q_PROPERTY(QObject* previewCard READ previewCard NOTIFY currentChanged)
    Q_PROPERTY(QString cardType READ cardType NOTIFY currentChanged)
    Q_PROPERTY(QStringList cardNames READ cardNames NOTIFY cardsChanged)
    Q_PROPERTY(int cardCount READ cardCount NOTIFY cardsChanged)
    Q_PROPERTY(QString configPath READ configPath CONSTANT)
    Q_PROPERTY(QStringList fontFamilies READ fontFamilies NOTIFY fontFamiliesChanged)

    // —— 通用（每张卡都有）
    Q_PROPERTY(QString cardName READ cardName WRITE setCardName NOTIFY styleChanged)
    Q_PROPERTY(int cardWidth READ cardWidth WRITE setCardWidth NOTIFY cardWidthChanged)
    Q_PROPERTY(QString cardColor READ cardColor WRITE setCardColor NOTIFY styleChanged)
    Q_PROPERTY(QString fontColor READ fontColor WRITE setFontColor NOTIFY styleChanged)
    Q_PROPERTY(QString miniCardColor READ miniCardColor WRITE setMiniCardColor NOTIFY styleChanged)
    Q_PROPERTY(QString fontFamily READ fontFamily WRITE setFontFamily NOTIFY styleChanged)

    // —— 倒计时卡
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PROPERTY(QVariantList workDays READ workDays NOTIFY workDaysChanged)
    Q_PROPERTY(bool workdayPreset READ workdayPreset NOTIFY workDaysChanged)
    Q_PROPERTY(QString workStart READ workStart WRITE setWorkStart NOTIFY timeChanged)
    Q_PROPERTY(QString workEnd READ workEnd WRITE setWorkEnd NOTIFY timeChanged)
    Q_PROPERTY(int payday READ payday WRITE setPayday NOTIFY paydayChanged)
    Q_PROPERTY(double dailyIncome READ dailyIncome WRITE setDailyIncome NOTIFY dailyIncomeChanged)
    Q_PROPERTY(bool showPayday READ showPayday WRITE setShowPayday NOTIFY showFlagsChanged)
    Q_PROPERTY(bool showFriday READ showFriday WRITE setShowFriday NOTIFY showFlagsChanged)
    Q_PROPERTY(bool showHoliday READ showHoliday WRITE setShowHoliday NOTIFY showFlagsChanged)
    Q_PROPERTY(bool showEarnings READ showEarnings WRITE setShowEarnings NOTIFY showFlagsChanged)
    Q_PROPERTY(bool showMascot READ showMascot WRITE setShowMascot NOTIFY showFlagsChanged)
    Q_PROPERTY(QString mascotMode READ mascotMode WRITE setMascotMode NOTIFY showFlagsChanged)

    // —— 纪念日卡
    Q_PROPERTY(QString eventName READ eventName WRITE setEventName NOTIFY anniversaryChanged)
    Q_PROPERTY(QString date READ date WRITE setDate NOTIFY anniversaryChanged)
    Q_PROPERTY(QString annivMode READ annivMode WRITE setAnnivMode NOTIFY anniversaryChanged)
    Q_PROPERTY(bool isCountdownMode READ isCountdownMode NOTIFY anniversaryChanged)
    Q_PROPERTY(QString effectPreview READ effectPreview NOTIFY anniversaryChanged)
    Q_PROPERTY(QString dateWarning READ dateWarning NOTIFY anniversaryChanged)
    Q_PROPERTY(QString repeatMode READ repeatMode WRITE setRepeatMode NOTIFY anniversaryChanged)
    Q_PROPERTY(int repeatIndex READ repeatIndex WRITE setRepeatIndex NOTIFY anniversaryChanged)
    Q_PROPERTY(QString bgMode READ bgMode WRITE setBgMode NOTIFY anniversaryChanged)
    Q_PROPERTY(QString bgImage READ bgImage NOTIFY anniversaryChanged)
    Q_PROPERTY(QString bgImageName READ bgImageName NOTIFY anniversaryChanged)
    Q_PROPERTY(bool hasImage READ hasImage NOTIFY anniversaryChanged)
    Q_PROPERTY(QVariantList repeatLabels READ repeatLabels CONSTANT)
    Q_PROPERTY(QVariantList eventSuggestions READ eventSuggestions NOTIFY anniversaryChanged)
    // —— 农历节日模式
    Q_PROPERTY(bool isLunarMode READ isLunarMode NOTIFY anniversaryChanged)
    Q_PROPERTY(QString lunarEvent READ lunarEvent WRITE setLunarEvent NOTIFY anniversaryChanged)
    Q_PROPERTY(int lunarEventIndex READ lunarEventIndex WRITE setLunarEventIndex NOTIFY anniversaryChanged)
    Q_PROPERTY(bool lunarCustom READ lunarCustom NOTIFY anniversaryChanged)
    Q_PROPERTY(QVariantList lunarEventLabels READ lunarEventLabels NOTIFY anniversaryChanged)
    Q_PROPERTY(int lunarMonthIndex READ lunarMonthIndex WRITE setLunarMonthIndex NOTIFY anniversaryChanged)
    Q_PROPERTY(int lunarDayIndex READ lunarDayIndex WRITE setLunarDayIndex NOTIFY anniversaryChanged)
    Q_PROPERTY(QVariantList lunarMonthLabels READ lunarMonthLabels CONSTANT)
    Q_PROPERTY(QVariantList lunarDayLabels READ lunarDayLabels CONSTANT)
    // 自检用：DW_OPEN_DATE_PICKER=1 时设置界面加载后自动弹出日历（方便抓图核对）
    Q_PROPERTY(bool autoOpenDatePicker READ autoOpenDatePicker CONSTANT)

public:
    SettingsBackend(ConfigStore* cfg, WidgetManager* mgr, QObject* parent = nullptr);

    int currentIndex() const { return m_current; }
    QObject* previewCard() const;
    QString cardType() const;
    QStringList cardNames() const;
    int cardCount() const;
    QString configPath() const;
    QStringList fontFamilies() const { return m_fonts; }

    QString cardName() const;
    int cardWidth() const;
    QString cardColor() const;
    QString fontColor() const;
    QString miniCardColor() const;
    QString fontFamily() const;

    QString title() const;
    QVariantList workDays() const; // 7 个 bool，index 0=周一
    bool workdayPreset() const;
    QString workStart() const;
    QString workEnd() const;
    int payday() const;
    double dailyIncome() const;
    bool showPayday() const { return flag(QStringLiteral("showPayday")); }
    bool showFriday() const { return flag(QStringLiteral("showFriday")); }
    bool showHoliday() const { return flag(QStringLiteral("showHoliday")); }
    bool showEarnings() const { return flag(QStringLiteral("showEarnings")); }
    bool showMascot() const { return flag(QStringLiteral("showMascot")); }
    QString mascotMode() const;

    QString eventName() const;
    QString date() const;
    QString annivMode() const;
    bool isCountdownMode() const { return annivMode() == QLatin1String("countdown"); }
    QString effectPreview() const; // "卡片上会显示成什么"（与卡片同一套口径算出）
    QString dateWarning() const;   // 模式/日期组合不合适时的提醒
    QString repeatMode() const;
    int repeatIndex() const;
    QString bgMode() const;
    QString bgImage() const;
    QString bgImageName() const;
    bool hasImage() const;
    QVariantList repeatLabels() const;
    QVariantList eventSuggestions() const;
    // —— 农历
    bool isLunarMode() const { return annivMode() == QLatin1String("lunar"); }
    QString lunarEvent() const;
    int lunarEventIndex() const; // 末位 = 「自定义农历日期」
    bool lunarCustom() const { return lunarEvent() == QLatin1String("custom"); }
    QVariantList lunarEventLabels() const;
    int lunarMonthIndex() const; // 0-11
    int lunarDayIndex() const;   // 0-29
    QVariantList lunarMonthLabels() const;
    QVariantList lunarDayLabels() const;
    bool autoOpenDatePicker() const { return qEnvironmentVariableIsSet("DW_OPEN_DATE_PICKER"); }

public slots:
    void setCurrentIndex(int v);
    void setCardName(const QString& v);
    void setCardWidth(int v);
    void setCardColor(const QString& v);
    void setFontColor(const QString& v);
    void setMiniCardColor(const QString& v);
    void setFontFamily(const QString& v);

    void setTitle(const QString& v);
    void setWorkStart(const QString& v);
    void setWorkEnd(const QString& v);
    void setPayday(int v);
    void setDailyIncome(double v);
    void setShowPayday(bool v);
    void setShowFriday(bool v);
    void setShowHoliday(bool v);
    void setShowEarnings(bool v);
    void setShowMascot(bool v);
    void setMascotMode(const QString& v);

    void setEventName(const QString& v);
    void setDate(const QString& v);
    void setAnnivMode(const QString& v);
    void setLunarEvent(const QString& v);
    void setLunarEventIndex(int v);
    void setLunarMonthIndex(int v);
    void setLunarDayIndex(int v);
    void setRepeatMode(const QString& v);
    void setRepeatIndex(int v);
    void setBgMode(const QString& v);
    void applyEventSuggestion(const QString& v);

public:
    Q_INVOKABLE void toggleWorkDay(int index); // 0=周一
    Q_INVOKABLE void applyWorkdayPreset();     // 周一到周五
    // 输入框提交（宽松解析 + 归一化）：成功返回 true；失败返回 false，调用方负责把输入框恢复成已存值
    Q_INVOKABLE bool applyDateInput(const QString& text);
    Q_INVOKABLE bool applyTimeInput(const QString& which, const QString& text); // which: start|end
    Q_INVOKABLE void addCard(const QString& type);
    Q_INVOKABLE void removeCurrentCard();
    Q_INVOKABLE void chooseBgImage();
    Q_INVOKABLE void clearBgImage();
    Q_INVOKABLE void previewBgImage(const QString& path); // 选中即预览（未确认前先看效果）
    Q_INVOKABLE void editPosition();                      // 关闭设置 → 进入编辑模式拖动
    Q_INVOKABLE void resetDefaults();                     // 只重置当前这张卡（位置保留）
    Q_INVOKABLE void closeWindow();
    Q_INVOKABLE void openConfigFolder();

signals:
    void currentChanged();
    void cardsChanged();
    void styleChanged();
    void cardWidthChanged();
    void titleChanged();
    void workDaysChanged();
    void timeChanged();
    void paydayChanged();
    void dailyIncomeChanged();
    void showFlagsChanged();
    void anniversaryChanged();
    void fontFamiliesChanged();
    void closeRequested();

private:
    ConfigStore::CardData* current();
    const ConfigStore::CardData* current() const;
    bool flag(const QString& key) const;
    void touchConfig(); // 应用 + 防抖存盘
    void emitAll();

    ConfigStore* m_cfg = nullptr;
    WidgetManager* m_mgr = nullptr;
    int m_current = 0;
    QTimer m_saveTimer;
    QStringList m_fonts;
};
