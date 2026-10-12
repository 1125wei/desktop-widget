#pragma once

#include <QList>
#include <QObject>
#include <QString>

// 配置存储：%APPDATA%/wei/DesktopWidget/config.json
//
// v2（本节）把"单张卡片"升级为**卡片列表**：cards 数组里每项一张卡，
// 各自有类型、位置、尺寸、配色与专属参数（倒计时 / 纪念日）。
// 老版本（v1）的扁平字段仍在 load() 里被识别并**迁移**成第一张倒计时卡，
// 用户升级后位置和设置不会丢。
class ConfigStore : public QObject {
    Q_OBJECT
public:
    // 一张卡片的全部配置。type 决定哪些字段有意义：
    //   "countdown"   下班倒计时 → title / workDays / workStart / workEnd / payday / dailyIncome / show*
    //   "anniversary" 纪念日     → eventName / date / repeatMode / bgMode / bgImage
    // 通用字段：cardName（设置列表里的名字）/ 位置 / 尺寸 / 配色 / 字体
    struct CardData {
        QString type = QStringLiteral("countdown");
        QString cardName = QStringLiteral("下班倒计时");
        bool hasPos = false;
        int x = 0;              // 屏幕物理像素（Win32 坐标，非 Qt 逻辑像素）
        int y = 0;
        int cardWidth = 312;    // 逻辑像素（设计稿基准见 qml/*.qml）
        QString cardColor = QStringLiteral("#FFFFFF");
        QString fontColor = QStringLiteral("#3A3A3A");
        QString miniCardColor = QStringLiteral("#F2F2F2");
        QString fontFamily = QStringLiteral("Microsoft YaHei UI");

        // —— 倒计时卡专属
        QString title = QStringLiteral("下班还有");
        QList<int> workDays{1, 2, 3, 4, 5};
        QString workStart = QStringLiteral("08:30");
        QString workEnd = QStringLiteral("17:30");
        int payday = 25;
        double dailyIncome = 100.0;
        bool showPayday = true;
        bool showFriday = true;
        bool showHoliday = true;
        bool showEarnings = true;
        bool showMascot = true; // 是否显示卡通形象
        QString mascotMode = QStringLiteral("image"); // image = 用户提供的猫；vector = 自绘矢量

        // —— 纪念日卡专属
        QString eventName = QStringLiteral("你在世界已经");
        QString date = QStringLiteral("2003-11-25");
        // "countdown" = 倒计时模式（还有多少天） / "anniversary" = 纪念日模式（已经多少天）
        //  / "lunar" = 农历节日模式（春节/中秋… 永远算"还有多少天"）
        QString annivMode = QStringLiteral("anniversary");
        QString lunarEvent = QStringLiteral("chunjie"); // 具名农历节日 id；"custom" = 用下面的月/日
        int lunarMonth = 1;                             // 农历月 1-12
        int lunarDay = 1;                               // 农历日 1-30
        QString repeatMode = QStringLiteral("none"); // none | yearly | monthly
        QString bgMode = QStringLiteral("color");    // color | image | moon
        QString bgImage;                             // 背景图绝对路径（bgMode=image 时有效）

        bool isAnniversary() const { return type == QLatin1String("anniversary"); }
    };

    struct Data {
        QString layerMode = QStringLiteral("auto");
        bool visible = true;
        QList<CardData> cards;
    };

    explicit ConfigStore(QObject* parent = nullptr);

    // 某类型的出厂默认卡（纪念日卡默认深蓝月夜底 + 白字，贴近参考 App）
    static CardData defaultCard(const QString& type);

    QString path() const;
    bool load();
    bool save() const;

    bool ensureCards(); // 一张卡都没有时补一张默认倒计时卡；返回是否改动过

    const Data& data() const { return m_data; }
    Data& mutableData() { return m_data; }

private:
    Data m_data;
};
