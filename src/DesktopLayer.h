#pragma once

#include <QPoint>
#include <QRect>
#include <QString>
#include <QtGlobal>
#include <QtGui/qwindowdefs.h> // WId

// 把窗口挂到 Windows「桌面层」（壁纸层）的 Win32 封装。
//
// Windows 没有公开 API 可以创建「桌面小组件」；业界通用做法（Wallpaper Engine 同款）：
//   1) 给 Progman 发未公开消息 0x052C，让资源管理器分裂出一个 WorkerW 窗口专门承载壁纸；
//   2) 找到承载图标的那层 SHELLDLL_DefView，再取它**后面**的那个 WorkerW 兄弟窗口；
//   3) SetParent(我们的窗口, 那个 WorkerW) → 窗口画在壁纸上、图标之下、被普通窗口正常遮挡。
class DesktopLayer {
public:
    enum Mode {
        Auto,     // 默认：自动挑（有可见的壁纸层 WorkerW 就用它，否则用 Progman）
        WorkerW,  // 强制挂 WorkerW（老 Win10 的壁纸层）
        Progman,  // 强制挂 Progman
        Bottom,   // 不换父窗口，只把窗口压到 z 序最底
        Float,    // 什么都不做：普通无边框浮窗（兜底/对比用）
    };

    struct Info {
        QString mode;
        QString hwnd;
        QString parent;       // 父窗口类名 + 句柄
        QRect rect;           // 屏幕物理像素矩形
        QString iconsWindow;  // 桌面图标层（SHELLDLL_DefView）所在窗口
        QString note;
        QString toLog() const;
    };

    static Mode modeFromName(const QString& name);

    // 应用指定层级策略；窗口须已 show()（有原生句柄与几何）
    static Info apply(WId wid, Mode mode);

    // 当前是否还挂在有效的桌面层上（父窗口被销毁/被换掉时返回 false）
    static bool isOnDesktopLayer(WId wid);

    // 调试：打印某窗口的子窗口 z 序（从顶到底），用于排查"挂上了但看不见"
    static void dumpChildZOrder(WId wid, const QString& tag);

    // 强制重绘某窗口（WM_PAINT 同步派发，让 Qt 认为窗口已曝光并出帧）
    static void forceRepaint(WId wid);

    // 编辑模式：脱离桌面层，变回「可点击、置顶」的顶层窗口（否则贴在图标层下点不到）
    static void detachForEditing(WId wid);

    // 把矩形钳制到所在显示器工作区内（防止拖到屏幕外→"看不见"）；完全在屏外则拉回右上角
    static QRect sanitizeRect(const QRect& r);

    // 窗口是否还和任意显示器相交（判断"是不是拖丢了"）
    static bool isOnScreen(WId wid);

    static Info describe(WId wid, const QString& modeName);

    // 把窗口左上角移到指定的**屏幕物理坐标**
    static void moveToScreenTopLeft(WId wid, const QPoint& screenTopLeft);

    static QRect windowScreenRect(WId wid);
    static QPoint cursorScreenPos();

    // 摆到窗口所在显示器工作区的右上角（margin 为物理像素，右边距=上边距）
    static void placeTopRight(WId wid, int marginPhysical);

    // 右上角 + 自定义上边距（多张卡片纵向排列用）
    static void placeTopRightAt(WId wid, int topOffsetPhysical, int marginPhysical);

    // 窗口所在显示器的**工作区**（物理像素，已排除任务栏）
    static QRect workAreaPhysical(WId wid);

    // 窗口销毁时清掉缓存的原始样式（多卡片增删必须调，否则句柄复用会读到脏数据）
    static void forget(WId wid);

    // 自检用：把屏幕上某块矩形（物理像素）抓成 PNG
    static bool grabScreenRect(const QRect& r, const QString& path);

    // 自检用：抓某个窗口「自身渲染内容」（被遮挡也能抓到），用于证明壁纸层上真的画出来了
    static bool grabWindowContent(WId wid, const QString& path);

    // 自检用：抓 wid 的父窗口内容（即壁纸层/桌面层），parentDesc 回填父窗口描述
    static bool grabParentContent(WId wid, const QString& path, QString* parentDesc = nullptr);
};
