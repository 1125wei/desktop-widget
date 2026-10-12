#include "AutoStart.h"
#include "CardBackend.h" // AnnivMath（纪念日口径自检用）
#include "ConfigStore.h"
#include "DesktopLayer.h"
#include "Festivals.h"
#include "SettingsBackend.h"
#include "SettingsWindow.h"
#include "TrayController.h"
#include "WidgetManager.h"

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QImage>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickView>
#include <QScreen>
#include <QStandardPaths>
#include <QTimer>
#include <QtGlobal>

namespace {
// 把每张卡片窗口的自身渲染结果抓成 PNG（被遮挡也能抓到，用于离屏核对外观）
void grabAllCards(WidgetManager* mgr, const QString& dataDir, const QString& prefix)
{
    for (int i = 0; i < mgr->cardCount(); ++i) {
        QQuickView* v = mgr->viewAt(i);
        if (!v)
            continue;
        const QImage img = v->grabWindow();
        const QString path = QStringLiteral("%1/%2-%3.png").arg(dataDir, prefix).arg(i);
        if (img.save(path))
            qInfo().noquote() << "[shot]" << prefix << i << path << img.size();
        else
            qWarning() << "[shot] 失败" << i;
    }
}
} // namespace

static void installFileLogger(const QString& dir)
{
    static QFile logFile(dir + QStringLiteral("/widget.log"));
    if (!logFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return;
    qInstallMessageHandler([](QtMsgType, const QMessageLogContext&, const QString& msg) {
        logFile.write(QDateTime::currentDateTime().toString(QStringLiteral("[hh:mm:ss] ")).toUtf8());
        logFile.write(msg.toUtf8());
        logFile.write("\n");
        logFile.flush();
    });
}

int main(int argc, char* argv[])
{
    // 托盘（QSystemTrayIcon / QMenu）在 Widgets 里，所以用 QApplication
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("DesktopWidget"));
    QApplication::setOrganizationName(QStringLiteral("wei"));
    QApplication::setQuitOnLastWindowClosed(false); // 托盘常驻：关掉设置窗口不等于退出
    // 固定用 Basic 风格：设置界面全部自绘深色样式，避免跟随系统风格走样
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    installFileLogger(dataDir);
    qInfo().noquote() << "===== DesktopWidget 启动 =====";

    ConfigStore cfg;
    if (!cfg.load())
        qInfo().noquote() << "没有配置文件，使用默认值";
    if (cfg.ensureCards()) {
        qInfo().noquote() << "配置里没有卡片 → 建立默认倒计时卡";
        cfg.save();
    }

    // 节日表（离线生成的 2025–2045）——初始化时载入一次
    qInfo().noquote() << "节日表:" << Festivals::instance().count() << "条，来源"
                      << Festivals::instance().source();

    // 层策略：命令行环境变量优先，其次配置文件
    const QString envLayer = qEnvironmentVariable("DW_LAYER");
    const QString layerName = envLayer.isEmpty() ? cfg.data().layerMode : envLayer;
    const DesktopLayer::Mode mode = DesktopLayer::modeFromName(layerName);

    WidgetManager mgr(&cfg, mode);
    mgr.start(); // 每张卡片一个窗口 → 挂桌面层 → 恢复位置

    SettingsBackend settingsBackend(&cfg, &mgr);
    SettingsWindow settingsWindow(&settingsBackend, &mgr);
    TrayController tray(&mgr, &cfg, &settingsWindow);
    tray.setup();
    tray.setVisibleChecked(cfg.data().visible);
    AutoStart::refresh(); // 绿色包被移动过 → 纠正开机自启里的旧路径

    // ==================== 自检钩子（全部无打扰：不抢焦点、自动退出）====================

    // 抓每张卡片自身的渲染结果（离屏核对外观）
    if (qEnvironmentVariableIsSet("DW_SHOT")) {
        const int shotMs = qEnvironmentVariable("DW_SHOT_MS", QStringLiteral("2000")).toInt();
        QTimer::singleShot(shotMs, &app, [&mgr, dataDir]() { grabAllCards(&mgr, dataDir, QStringLiteral("self-shot")); });
    }

    // 设置界面自检（DW_SHOT_SETTINGS=1）：打开设置窗口并抓自身渲染结果
    // DW_SETTINGS_CARD=<索引> 可先选中某张卡片（核对该类型的配置行）
    const QString selCard = qEnvironmentVariable("DW_SETTINGS_CARD");
    if (!selCard.isEmpty())
        QTimer::singleShot(800, &settingsBackend, [&settingsBackend, selCard]() {
            settingsBackend.setCurrentIndex(selCard.toInt());
        });

    if (qEnvironmentVariableIsSet("DW_SHOT_SETTINGS")) {
        QTimer::singleShot(600, &settingsWindow, [&settingsWindow]() { settingsWindow.show(); });
        QTimer::singleShot(3000, &app, [&settingsWindow, dataDir]() {
            const QString path = dataDir + QStringLiteral("/settings.png");
            if (settingsWindow.grabTo(path))
                qInfo().noquote() << "设置界面截图:" << path;
            else
                qWarning() << "设置界面截图失败";
        });
    }

    // 「改动即时生效」自检：改 5 项配置 → 抓卡片渲染 → 复原
    if (qEnvironmentVariableIsSet("DW_TEST_APPLY")) {
        QTimer::singleShot(1500, &app, [&settingsBackend]() {
            settingsBackend.setTitle(QStringLiteral("测试生效"));
            settingsBackend.setCardColor(QStringLiteral("#FFF7D6"));
            settingsBackend.setFontColor(QStringLiteral("#1E4E8A"));
            settingsBackend.setDailyIncome(200);
            settingsBackend.setShowFriday(false);
            qInfo().noquote() << "[test] 已通过 SettingsBackend 改 5 项配置";
        });
        QTimer::singleShot(3000, &app, [&mgr, dataDir]() { grabAllCards(&mgr, dataDir, QStringLiteral("apply-test")); });
        QTimer::singleShot(4200, &app, [&settingsBackend]() {
            settingsBackend.resetDefaults();
            qInfo().noquote() << "[test] 已恢复默认值（位置不受影响）";
        });
    }

    // 新增纪念日卡（自检）：验证多卡片全流程
    if (qEnvironmentVariableIsSet("DW_ADD_ANNIV")) {
        QTimer::singleShot(1200, &app, [&mgr]() {
            const int i = mgr.addCard(QStringLiteral("anniversary"));
            qInfo().noquote() << "[test] 新增纪念日卡 → 索引" << i;
        });
        QTimer::singleShot(3200, &app, [&mgr, dataDir]() { grabAllCards(&mgr, dataDir, QStringLiteral("add-anniv")); });
        QTimer::singleShot(3600, &app, [&mgr]() { mgr.dumpState(QStringLiteral("新增纪念日卡后")); });
    }

    // 设置纪念日卡内容（自检）：选中纪念日卡 → 改事件名/日期/重复 → 抓渲染
    // （时间安排在 DW_ADD_ANNIV 之后，方便两个开关一起跑）
    if (qEnvironmentVariableIsSet("DW_SET_ANNIV")) {
        QTimer::singleShot(3200, &app, [&settingsBackend, &cfg]() {
            int idx = -1;
            for (int i = 0; i < cfg.data().cards.size(); ++i) {
                if (cfg.data().cards.at(i).isAnniversary()) {
                    idx = i;
                    break;
                }
            }
            if (idx < 0)
                return;
            settingsBackend.setCurrentIndex(idx);
            settingsBackend.setEventName(QStringLiteral("距离生日还有"));
            settingsBackend.setDate(QStringLiteral("2000-05-20"));
            settingsBackend.setRepeatMode(QStringLiteral("yearly"));
            qInfo().noquote() << "[test] 纪念日卡已改为 每年 5-20 生日倒计时";
        });
        QTimer::singleShot(4800, &app, [&mgr, dataDir]() { grabAllCards(&mgr, dataDir, QStringLiteral("set-anniv")); });
    }

    // 编辑模式端到端自检：进编辑 → 退出 → 抓壁纸层内容（验证"退出后还看得见"）
    if (qEnvironmentVariableIsSet("DW_EDIT_TEST")) {
        QTimer::singleShot(2000, &app, [&mgr]() { mgr.setEditing(true); });
        QTimer::singleShot(5000, &app, [&mgr]() { mgr.setEditing(false); });
        QTimer::singleShot(7500, &app, [&mgr, dataDir]() {
            for (int i = 0; i < mgr.cardCount(); ++i) {
                QQuickView* v = mgr.viewAt(i);
                if (!v)
                    continue;
                QString desc;
                const QString p = QStringLiteral("%1/layer-after-edit-%2.png").arg(dataDir).arg(i);
                if (DesktopLayer::grabParentContent(v->winId(), p, &desc))
                    qInfo().noquote() << "[edit-test] 卡片" << i << "退出后壁纸层抓图:" << p << desc;
            }
        });
    }

    // 抓屏幕 + 壁纸层内容（每张卡片各来一张）
    if (qEnvironmentVariableIsSet("DW_PROBE")) {
        QTimer::singleShot(2500, &app, [&mgr, dataDir]() {
            for (int i = 0; i < mgr.cardCount(); ++i) {
                QQuickView* v = mgr.viewAt(i);
                if (!v)
                    continue;
                const QRect r = DesktopLayer::windowScreenRect(v->winId());
                qInfo().noquote() << "[probe] 卡片" << i << "rect:" << r;
                DesktopLayer::grabScreenRect(r.adjusted(-40, -40, 40, 40),
                                             QStringLiteral("%1/probe-%2.png").arg(dataDir).arg(i));
                QString desc;
                DesktopLayer::grabParentContent(v->winId(),
                                                QStringLiteral("%1/layer-content-%2.png").arg(dataDir).arg(i), &desc);
            }
        });
    }

    // 纪念日口径/日期解析自检（DW_TEST_ANNIV=1，配 DW_FREEZE 用）
    if (qEnvironmentVariableIsSet("DW_TEST_ANNIV")) {
        QTimer::singleShot(900, &app, [&settingsBackend, &cfg]() {
            int idx = -1;
            for (int i = 0; i < cfg.data().cards.size(); ++i)
                if (cfg.data().cards.at(i).isAnniversary())
                    idx = i;
            if (idx < 0) {
                qWarning() << "[test-anniv] 没有纪念日卡";
                return;
            }
            const QDate today = dwEffectiveNow().date();
            qInfo().noquote() << "[test-anniv] 今天 =" << today.toString(Qt::ISODate);

            // ① 日期解析（宽松）
            const QStringList inputs{"2027-3-5", "2027/03/05", "2027年3月5日", "20270305",
                                     "2027.3.5", "2027-02-31", "abc", ""};
            settingsBackend.setCurrentIndex(idx);
            for (const QString& in : inputs) {
                const QString before = cfg.data().cards.at(idx).date;
                const bool ok = settingsBackend.applyDateInput(in);
                const QString after = cfg.data().cards.at(idx).date;
                qInfo().noquote() << QStringLiteral("[test-anniv] 输入 %1 → %2，配置日期 %3 → %4")
                                         .arg(in.isEmpty() ? QStringLiteral("(空)") : in,
                                              ok ? QStringLiteral("接受") : QStringLiteral("拒绝"), before, after);
                settingsBackend.applyDateInput("2003-11-25"); // 复位
            }

            // ② 两种模式 × 重复规则的口径（与卡片共用 AnnivMath）
            struct Case {
                const char* mode;
                const char* date;
                const char* repeat;
                int expect;
            };
            const Case cases[] = {
                {"anniversary", "2003-11-25", "none", 8357},   // 参考截图那条
                {"anniversary", "2027-03-01", "none", 0},      // 起始日在未来 → 0
                {"countdown", "2027-03-15", "none", 154},
                {"countdown", "2003-11-25", "none", 0},        // 已过期 → 0
                {"countdown", "2000-05-20", "yearly", 220},
                {"countdown", "2000-10-20", "monthly", 8},
            };
            int pass = 0, total = 0;
            for (const Case& c : cases) {
                const AnnivMath::Result r = AnnivMath::compute(QString::fromLatin1(c.date),
                                                               QString::fromLatin1(c.mode),
                                                               QString::fromLatin1(c.repeat), today);
                const bool ok = r.valid && r.days == c.expect;
                ++total;
                if (ok)
                    ++pass;
                qInfo().noquote() << QStringLiteral("[test-anniv] %1/%2/%3 → %4 天（期望 %5）%6")
                                         .arg(QString::fromLatin1(c.mode), QString::fromLatin1(c.date),
                                              QString::fromLatin1(c.repeat))
                                         .arg(r.days)
                                         .arg(c.expect)
                                         .arg(ok ? QStringLiteral("✓") : QStringLiteral("✗ 不符"));
            }
            qInfo().noquote() << QStringLiteral("[test-anniv] 口径用例 %1/%2 通过").arg(pass).arg(total);
        });
    }

    // 纪念日「两种模式」端到端自检（DW_TEST_MODE=1）：改模式/日期 → 卡片渲染 + 设置侧提示
    if (qEnvironmentVariableIsSet("DW_TEST_MODE")) {
        auto annivIndex = [&cfg]() {
            for (int i = 0; i < cfg.data().cards.size(); ++i)
                if (cfg.data().cards.at(i).isAnniversary())
                    return i;
            return -1;
        };
        auto logState = [&settingsBackend](const char* tag) {
            qInfo().noquote() << QStringLiteral("[test-mode] %1 → 卡片会显示[%2] 提醒[%3]")
                                     .arg(QString::fromUtf8(tag), settingsBackend.effectPreview(),
                                          settingsBackend.dateWarning().isEmpty()
                                              ? QStringLiteral("无")
                                              : settingsBackend.dateWarning());
        };

        QTimer::singleShot(1000, &app, [&settingsBackend, annivIndex, logState]() {
            const int idx = annivIndex();
            if (idx < 0)
                return;
            settingsBackend.setCurrentIndex(idx);
            settingsBackend.setAnnivMode(QStringLiteral("countdown"));
            // 故意用不带前导零的写法（旧版就是卡在这里静默失败）
            const bool ok = settingsBackend.applyDateInput(QStringLiteral("2027-3-5"));
            qInfo().noquote() << "[test-mode] 宽松输入 2027-3-5 接受 =" << ok;
            settingsBackend.setRepeatIndex(0); // 不重复
            logState("倒计时/不重复/2027-3-5");
        });
        QTimer::singleShot(1800, &app, [&mgr, dataDir]() { grabAllCards(&mgr, dataDir, QStringLiteral("mode-countdown")); });
        QTimer::singleShot(2400, &app, [&settingsBackend, annivIndex, logState]() {
            if (annivIndex() < 0)
                return;
            settingsBackend.applyDateInput(QStringLiteral("2003-11-25")); // 已过期
            logState("倒计时/不重复/过去的日期");
            settingsBackend.setRepeatIndex(1); // 每年
            logState("倒计时/每年/2003-11-25");
        });
        QTimer::singleShot(3200, &app, [&settingsBackend, annivIndex, logState]() {
            if (annivIndex() < 0)
                return;
            settingsBackend.setAnnivMode(QStringLiteral("anniversary"));
            logState("纪念日/2003-11-25");
        });
        QTimer::singleShot(4000, &app, [&mgr, dataDir]() { grabAllCards(&mgr, dataDir, QStringLiteral("mode-anniversary")); });
    }

    // 农历自检（DW_TEST_LUNAR=1，配 DW_FREEZE=2026-10-12T11:00:00）：逐条对已知日期
    if (qEnvironmentVariableIsSet("DW_TEST_LUNAR")) {
        QTimer::singleShot(950, &app, [&settingsBackend, &cfg]() {
            const QDate today = dwEffectiveNow().date();
            const Festivals& f = Festivals::instance();
            int pass = 0, total = 0;

            struct C1 {
                const char* id;
                const char* expect;
            };
            const C1 c1[] = {
                {"chunjie", "2027-02-06"},   // 下一个春节
                {"chuxi", "2027-02-05"},     // 除夕（该年腊月只有 29 天）
                {"zhongqiu", "2027-09-15"},  // 下一个中秋
                {"chongyang", "2026-10-18"}, // 重阳（与倒计时卡的"重阳节 6 天"呼应）
                {"duanwu", "2027-06-09"},    // 下一个端午
            };
            for (const C1& c : c1) {
                const QDate d = f.nextLunarEvent(QString::fromLatin1(c.id), today);
                const bool ok = d.isValid() && d.toString(Qt::ISODate) == QString::fromLatin1(c.expect);
                ++total;
                if (ok)
                    ++pass;
                qInfo().noquote() << QStringLiteral("[test-lunar] 节日 %1 → %2（期望 %3）%4")
                                         .arg(QString::fromLatin1(c.id), d.toString(Qt::ISODate),
                                              QString::fromLatin1(c.expect),
                                              ok ? QStringLiteral("✓") : QStringLiteral("✗ 不符"));
            }

            struct C2 {
                int m;
                int d;
                const char* expect;
            };
            const C2 c2[] = {
                {9, 9, "2026-10-18"},    // 农历九月初九
                {12, 23, "2027-01-30"},  // 腊月廿三（小年）
                {12, 30, "2027-02-05"},  // 腊月三十 → 该年腊月只有 29 天，退到月末
                {1, 1, "2027-02-06"},    // 正月初一
            };
            for (const C2& c : c2) {
                const QDate d = f.nextLunarDate(c.m, c.d, today);
                const bool ok = d.isValid() && d.toString(Qt::ISODate) == QString::fromLatin1(c.expect);
                ++total;
                if (ok)
                    ++pass;
                qInfo().noquote() << QStringLiteral("[test-lunar] 农历%1月%2日 → %3（期望 %4）%5")
                                         .arg(c.m)
                                         .arg(c.d)
                                         .arg(d.isValid() ? d.toString(Qt::ISODate) : QStringLiteral("无效"),
                                              QString::fromLatin1(c.expect),
                                              ok ? QStringLiteral("✓") : QStringLiteral("✗ 不符"));
            }
            qInfo().noquote() << QStringLiteral("[test-lunar] 用例 %1/%2 通过").arg(pass).arg(total);

            int idx = -1;
            for (int i = 0; i < cfg.data().cards.size(); ++i)
                if (cfg.data().cards.at(i).isAnniversary())
                    idx = i;
            if (idx >= 0) {
                settingsBackend.setCurrentIndex(idx);
                settingsBackend.setAnnivMode(QStringLiteral("lunar"));
                for (const QString& id : {QStringLiteral("chunjie"), QStringLiteral("chuxi"),
                                          QStringLiteral("zhongqiu")}) {
                    settingsBackend.setLunarEvent(id);
                    qInfo().noquote() << QStringLiteral("[test-lunar] 农历模式 %1 → 卡片会显示[%2] 提醒[%3]")
                                             .arg(id, settingsBackend.effectPreview(),
                                                  settingsBackend.dateWarning().isEmpty()
                                                      ? QStringLiteral("无")
                                                      : settingsBackend.dateWarning());
                }
                // 自定义农历生日：九月初九（先显式切到"自定义"，与界面路径一致）
                settingsBackend.setLunarEventIndex(settingsBackend.lunarEventLabels().size() - 1);
                settingsBackend.setLunarMonthIndex(8);
                settingsBackend.setLunarDayIndex(8);
                settingsBackend.setEventName(QStringLiteral("距离农历生日还有"));
                qInfo().noquote() << QStringLiteral("[test-lunar] 自定义九月初九 → 卡片会显示[%1] 提醒[%2]")
                                         .arg(settingsBackend.effectPreview(),
                                              settingsBackend.dateWarning().isEmpty()
                                                  ? QStringLiteral("无")
                                                  : settingsBackend.dateWarning());
            }
        });
        QTimer::singleShot(2200, &app, [&mgr, dataDir]() { grabAllCards(&mgr, dataDir, QStringLiteral("lunar")); });
    }

    // 删卡片自检（DW_DEL_CARD=<索引>）：删除指定卡片并 dump 状态（验证索引重排）
    const QString delCard = qEnvironmentVariable("DW_DEL_CARD");
    if (!delCard.isEmpty()) {
        QTimer::singleShot(1500, &app, [&mgr, delCard]() {
            mgr.dumpState(QStringLiteral("删除前"));
            mgr.removeCard(delCard.toInt());
        });
        QTimer::singleShot(2500, &app, [&mgr]() { mgr.dumpState(QStringLiteral("删除后")); });
    }

    // 开机自启自检（DW_AUTOSTART=1）：写注册表 → 读回校验 → 恢复原状
    if (qEnvironmentVariableIsSet("DW_AUTOSTART")) {
        QTimer::singleShot(1200, &app, []() {
            const bool before = AutoStart::enabled();
            QString after;
            const bool okOn = AutoStart::setEnabled(true, &after);
            qInfo().noquote() << "[test] 开机自启 开启:" << okOn << "注册表值 =" << after
                              << "(写入前 =" << before << ")";
            const bool okOff = AutoStart::setEnabled(before, &after);
            qInfo().noquote() << "[test] 开机自启 恢复:" << okOff << "注册表值 =" << after;
        });
    }

    const QString quitMs = qEnvironmentVariable("DW_QUIT_MS");
    if (!quitMs.isEmpty())
        QTimer::singleShot(quitMs.toInt(), &app, &QCoreApplication::quit);

    return app.exec();
}
