#include "DesktopLayer.h"

#include <QDebug>
#include <QHash>
#include <QImage>

#include <windows.h>

namespace {

// 首次挂层前保存的原始窗口样式，供「编辑模式」精确还原。
// ⚠️ 必须**按窗口**分别记录：多张卡片 = 多个窗口，共用一个全局结构会让
//    第 2..N 张卡在退出编辑模式时被还原成第 1 张卡的样式。
struct OriginalState {
    LONG_PTR style = 0;
    LONG_PTR exStyle = 0;
    bool saved = false;
};

QHash<HWND, OriginalState>& origMap()
{
    static QHash<HWND, OriginalState> m;
    return m;
}

bool saveMemDC(HDC mem, int w, int h, const QString& path)
{
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h; // 负数 = top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    QImage img(w, h, QImage::Format_ARGB32);
    if (!GetDIBits(mem, static_cast<HBITMAP>(GetCurrentObject(mem, OBJ_BITMAP)), 0, h,
                   img.bits(), &bi, DIB_RGB_COLORS))
        return false;
    return img.save(path);
}

QString classNameOf(HWND h)
{
    wchar_t buf[256] = {0};
    if (!h)
        return QStringLiteral("<null>");
    GetClassNameW(h, buf, 255);
    return QString::fromWCharArray(buf);
}

QString hwndStr(HWND h)
{
    return QStringLiteral("0x%1").arg(static_cast<quintptr>(reinterpret_cast<quintptr>(h)), 0, 16);
}

struct FindCtx {
    HWND defView = nullptr;        // SHELLDLL_DefView 所在的顶层窗口（Progman 或含它的 WorkerW）
    HWND defViewChild = nullptr;   // SHELLDLL_DefView 本尊（z 序锚点：卡片要插在它下面）
    HWND wallpaperWorker = nullptr; // DefView 之后（更靠底）的那个 WorkerW
    HWND anyWorker = nullptr;
};

BOOL CALLBACK enumProc(HWND top, LPARAM lp)
{
    auto* ctx = reinterpret_cast<FindCtx*>(lp);
    wchar_t cls[128] = {0};
    GetClassNameW(top, cls, 127);
    const QString c = QString::fromWCharArray(cls);

    if (c == QLatin1String("WorkerW")) {
        if (!ctx->anyWorker)
            ctx->anyWorker = top;
        HWND def = FindWindowExW(top, nullptr, L"SHELLDLL_DefView", nullptr);
        if (def) {
            ctx->defView = top;
            ctx->defViewChild = def;
            HWND w = FindWindowExW(nullptr, top, L"WorkerW", nullptr);
            if (w)
                ctx->wallpaperWorker = w;
        }
    } else if (c == QLatin1String("Progman")) {
        HWND def = FindWindowExW(top, nullptr, L"SHELLDLL_DefView", nullptr);
        if (def) {
            if (!ctx->defView) {
                ctx->defView = top;
                ctx->defViewChild = def;
            }
            // Win11 25H2：Progman 下还会有承载壁纸的 WorkerW 子窗口
            if (!ctx->anyWorker)
                ctx->anyWorker = FindWindowExW(top, nullptr, L"WorkerW", nullptr);
        }
    }
    return TRUE;
}

} // namespace

QString DesktopLayer::Info::toLog() const
{
    return QStringLiteral("layer=%1 hwnd=%2 parent=%3 rect=(%4,%5 %6x%7) icons=%8 %9")
        .arg(mode, hwnd, parent)
        .arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height())
        .arg(iconsWindow, note);
}

DesktopLayer::Mode DesktopLayer::modeFromName(const QString& name)
{
    const QString s = name.trimmed().toLower();
    if (s == QLatin1String("workerw"))
        return WorkerW;
    if (s == QLatin1String("progman"))
        return Progman;
    if (s == QLatin1String("bottom"))
        return Bottom;
    if (s == QLatin1String("float"))
        return Float;
    return Auto;
}

QRect DesktopLayer::sanitizeRect(const QRect& r)
{
    RECT rc{r.left(), r.top(), r.right(), r.bottom()};
    HMONITOR mon = MonitorFromRect(&rc, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(MONITORINFO);
    if (!mon || !GetMonitorInfoW(mon, &mi))
        return r;
    const RECT& wa = mi.rcWork;
    const int w = r.width();
    const int h = r.height();
    int x = r.left();
    int y = r.top();

    RECT inter{};
    if (!IntersectRect(&inter, &rc, &wa)) {
        // 完全跑到屏幕外了 → 拉回该显示器右上角
        x = wa.right - w - 60;
        y = wa.top + 60;
    } else {
        if (x + w > wa.right)
            x = wa.right - w - 8;
        if (y + h > wa.bottom)
            y = wa.bottom - h - 8;
        if (x < wa.left)
            x = wa.left + 8;
        if (y < wa.top)
            y = wa.top + 8;
    }
    return QRect(x, y, w, h);
}

bool DesktopLayer::isOnScreen(WId wid)
{
    RECT rc{};
    if (!GetWindowRect(reinterpret_cast<HWND>(wid), &rc))
        return false;
    return MonitorFromRect(&rc, MONITOR_DEFAULTTONULL) != nullptr;
}

void DesktopLayer::detachForEditing(WId wid)
{
    HWND hwnd = reinterpret_cast<HWND>(wid);
    if (!hwnd)
        return;

    RECT r{};
    GetWindowRect(hwnd, &r);

    // 还原成顶层窗口（清 WS_CHILD / 恢复 WS_POPUP），脱离壁纸层
    const OriginalState st = origMap().value(hwnd);
    if (st.saved)
        SetWindowLongPtrW(hwnd, GWL_STYLE, st.style);
    SetParent(hwnd, nullptr);

    const LONG_PTR ex = st.saved ? st.exStyle : GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    // 编辑时置顶 + 可点击，但仍保留 TOOLWINDOW（不污染任务栏）
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE,
                      (ex & ~WS_EX_NOACTIVATE) | WS_EX_TOOLWINDOW | WS_EX_TOPMOST);

    SetWindowPos(hwnd, HWND_TOPMOST, r.left, r.top, r.right - r.left, r.bottom - r.top,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void DesktopLayer::forceRepaint(WId wid)
{
    HWND hwnd = reinterpret_cast<HWND>(wid);
    if (!hwnd)
        return;
    RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
    UpdateWindow(hwnd);
}

void DesktopLayer::dumpChildZOrder(WId wid, const QString& tag)
{
    HWND parent = GetParent(reinterpret_cast<HWND>(wid));
    if (!parent)
        parent = reinterpret_cast<HWND>(wid);
    QStringList order;
    for (HWND c = GetWindow(parent, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) {
        wchar_t buf[128] = {0};
        GetWindowTextW(c, buf, 127);
        const bool self = (c == reinterpret_cast<HWND>(wid));
        RECT r{};
        GetWindowRect(c, &r);
        order << QStringLiteral("%1%2(%3 %4x%5 vis=%6)")
                     .arg(self ? QStringLiteral("*") : QString(), classNameOf(c), hwndStr(c))
                     .arg(r.left).arg(r.top).arg(r.right - r.left)
                     .arg(IsWindowVisible(c) ? 1 : 0);
    }
    qInfo().noquote() << "z序[" << tag << "]自顶向下:" << order.join(QStringLiteral(" | "));
}

bool DesktopLayer::isOnDesktopLayer(WId wid)
{
    HWND hwnd = reinterpret_cast<HWND>(wid);
    if (!hwnd || !IsWindow(hwnd))
        return false;
    HWND parent = GetParent(hwnd);
    if (!parent || !IsWindow(parent))
        return false;
    const QString c = classNameOf(parent);
    if (c == QLatin1String("Progman"))
        return IsWindowVisible(parent);
    if (c == QLatin1String("WorkerW")) {
        RECT r{};
        GetWindowRect(parent, &r);
        // 必须是可见且铺满屏的壁纸层；那种 202x56 的隐藏 WorkerW 挂上去等于隐身
        return IsWindowVisible(parent) && (r.right - r.left) > 400;
    }
    return false;
}

QRect DesktopLayer::windowScreenRect(WId wid)
{
    RECT r{};
    if (!GetWindowRect(reinterpret_cast<HWND>(wid), &r))
        return {};
    return QRect(r.left, r.top, r.right - r.left, r.bottom - r.top);
}

QPoint DesktopLayer::cursorScreenPos()
{
    POINT p{};
    GetCursorPos(&p);
    return QPoint(p.x, p.y);
}

void DesktopLayer::moveToScreenTopLeft(WId wid, const QPoint& screenTopLeft)
{
    HWND hwnd = reinterpret_cast<HWND>(wid);
    HWND parent = GetParent(hwnd);
    POINT pt{screenTopLeft.x(), screenTopLeft.y()};
    if (parent)
        MapWindowPoints(nullptr, parent, &pt, 1); // 屏幕坐标 → 父窗口客户区坐标
    SetWindowPos(hwnd, HWND_TOP, pt.x, pt.y, 0, 0,
                 SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void DesktopLayer::placeTopRight(WId wid, int marginPhysical)
{
    placeTopRightAt(wid, marginPhysical, marginPhysical);
}

QRect DesktopLayer::workAreaPhysical(WId wid)
{
    HWND hwnd = reinterpret_cast<HWND>(wid);
    HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(MONITORINFO);
    RECT area{};
    if (mon && GetMonitorInfoW(mon, &mi))
        area = mi.rcWork; // 工作区（已排除任务栏）
    else
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &area, 0);
    return QRect(area.left, area.top, area.right - area.left, area.bottom - area.top);
}

void DesktopLayer::placeTopRightAt(WId wid, int topOffsetPhysical, int marginPhysical)
{
    HWND hwnd = reinterpret_cast<HWND>(wid);
    RECT r{};
    GetWindowRect(hwnd, &r);
    const QRect wa = workAreaPhysical(wid);
    const int w = r.right - r.left;
    moveToScreenTopLeft(wid, QPoint(wa.x() + wa.width() - w - marginPhysical,
                                    wa.y() + topOffsetPhysical));
}

void DesktopLayer::forget(WId wid)
{
    origMap().remove(reinterpret_cast<HWND>(wid));
}

bool DesktopLayer::grabScreenRect(const QRect& r, const QString& path)
{
    if (r.width() <= 0 || r.height() <= 0)
        return false;
    HDC screen = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(screen);
    HBITMAP bmp = CreateCompatibleBitmap(screen, r.width(), r.height());
    HGDIOBJ old = SelectObject(mem, bmp);
    BitBlt(mem, 0, 0, r.width(), r.height(), screen, r.x(), r.y(), SRCCOPY);
    const bool saved = saveMemDC(mem, r.width(), r.height(), path);
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
    return saved;
}

bool DesktopLayer::grabParentContent(WId wid, const QString& path, QString* parentDesc)
{
    HWND hwnd = reinterpret_cast<HWND>(wid);
    HWND parent = hwnd ? GetParent(hwnd) : nullptr;
    if (parentDesc)
        *parentDesc = parent ? QStringLiteral("%1 %2").arg(classNameOf(parent), hwndStr(parent))
                             : QStringLiteral("<top-level>");
    if (!parent)
        return false;
    return grabWindowContent(reinterpret_cast<WId>(parent), path);
}

bool DesktopLayer::grabWindowContent(WId wid, const QString& path)
{
    HWND hwnd = reinterpret_cast<HWND>(wid);
    if (!hwnd)
        return false;
    RECT rc{};
    GetClientRect(hwnd, &rc);
    const int w = rc.right - rc.left;
    const int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0)
        return false;

    HDC screen = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(screen);
    HBITMAP bmp = CreateCompatibleBitmap(screen, w, h);
    HGDIOBJ old = SelectObject(mem, bmp);
    // PW_RENDERFULLCONTENT(2)：走 DWM 合成表面，能拿到被遮挡窗口（含跨进程子窗口）的画面
    const BOOL ok = PrintWindow(hwnd, mem, 2);
    const bool saved = (ok != 0) && saveMemDC(mem, w, h, path);
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
    return saved;
}

DesktopLayer::Info DesktopLayer::describe(WId wid, const QString& modeName)
{    HWND hwnd = reinterpret_cast<HWND>(wid);
    HWND parent = GetParent(hwnd);
    RECT r{};
    GetWindowRect(hwnd, &r);

    FindCtx ctx;
    EnumWindows(enumProc, reinterpret_cast<LPARAM>(&ctx));

    Info i;
    i.mode = modeName;
    i.hwnd = hwndStr(hwnd);
    i.parent = parent ? QStringLiteral("%1 %2").arg(classNameOf(parent), hwndStr(parent))
                      : QStringLiteral("<top-level>");
    i.rect = QRect(r.left, r.top, r.right - r.left, r.bottom - r.top);
    i.iconsWindow = ctx.defView ? QStringLiteral("%1 %2").arg(classNameOf(ctx.defView), hwndStr(ctx.defView))
                                : QStringLiteral("<none>");
    const bool vis = IsWindowVisible(hwnd) != 0;
    const bool topmost = (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
    if (parent && classNameOf(parent) == QLatin1String("WorkerW")) {
        RECT pr{};
        GetWindowRect(parent, &pr);
        i.note = QStringLiteral("parentRect=(%1,%2 %3x%4) visible=%5 %6")
                     .arg(pr.left).arg(pr.top).arg(pr.right - pr.left).arg(pr.bottom - pr.top)
                     .arg(IsWindowVisible(parent) ? 1 : 0)
                     .arg(parent == ctx.wallpaperWorker ? QStringLiteral("[壁纸层那个]")
                                                        : QStringLiteral("[非壁纸层]"));
    } else if (parent && classNameOf(parent) == QLatin1String("Progman")) {
        i.note = QStringLiteral("[OK?] 挂在 Progman 上（未拆分 WorkerW 的构建）");
    } else if (!parent) {
        i.note = QStringLiteral("[top-level] 未挂桌面层");
    } else {
        i.note = QStringLiteral("[?] 父窗口异常: ") + classNameOf(parent);
    }
    // 关键自检位：窗口自己是否可见、是否残留 TOPMOST（二次挂载"看不见"就是这两处出的问题）
    i.note += QStringLiteral(" | selfVisible=%1 topmost=%2")
                  .arg(vis ? QStringLiteral("是") : QStringLiteral("否"), topmost ? QStringLiteral("是") : QStringLiteral("否"));
    return i;
}

DesktopLayer::Info DesktopLayer::apply(WId wid, Mode mode)
{
    HWND hwnd = reinterpret_cast<HWND>(wid);
    if (!hwnd)
        return describe(wid, QStringLiteral("none"));

    // 记下原始样式（第一次改之前），编辑模式退出时要用它还原成顶层窗口
    if (!origMap().contains(hwnd)) {
        OriginalState st;
        st.style = GetWindowLongPtrW(hwnd, GWL_STYLE);
        st.exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
        st.saved = true;
        origMap().insert(hwnd, st);
    }

    // 不进 Alt-Tab / 任务栏，且不抢焦点
    LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, ex | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW);

    RECT before{};
    GetWindowRect(hwnd, &before);

    QString modeName;
    switch (mode) {
    case Auto:    modeName = QStringLiteral("auto");    break;
    case WorkerW: modeName = QStringLiteral("workerw"); break;
    case Progman: modeName = QStringLiteral("progman"); break;
    case Bottom:  modeName = QStringLiteral("bottom");  break;
    case Float:   modeName = QStringLiteral("float");   break;
    }

    if (mode == Auto || mode == WorkerW || mode == Progman) {
        HWND progman = FindWindowW(L"Progman", nullptr);
        if (progman) {
            DWORD_PTR res = 0;
            // 0x052C：请资源管理器把壁纸层从 Progman 里拆出一个 WorkerW（未公开消息）
            SendMessageTimeoutW(progman, 0x052C, 0, 0, SMTO_NORMAL, 1000, &res);
        }

        FindCtx ctx;
        EnumWindows(enumProc, reinterpret_cast<LPARAM>(&ctx));

        HWND target = nullptr;
        QString why;
        if (mode == WorkerW) {
            target = ctx.wallpaperWorker ? ctx.wallpaperWorker : ctx.anyWorker;
            why = QStringLiteral("强制 WorkerW");
        } else if (mode == Progman) {
            target = progman;
            why = QStringLiteral("强制 Progman");
        } else {
            // Auto：有「含 SHELLDLL_DefView 的 WorkerW」就是老 Win10 布局 → 用它；
            //        Win11 25H2 把图标直接放在 Progman 下 → 挂 Progman
            if (ctx.wallpaperWorker) {
                target = ctx.wallpaperWorker;
                why = QStringLiteral("auto→WorkerW(壁纸层)");
            } else if (ctx.defView) {
                target = ctx.defView;
                why = QStringLiteral("auto→DefView 所在窗口(%1)").arg(classNameOf(ctx.defView));
            } else {
                target = progman;
                why = QStringLiteral("auto→Progman(兜底)");
            }
        }
        if (!target) {
            target = progman;
            why = QStringLiteral("兜底 Progman");
        }
        qInfo().noquote() << "target:" << why << hwndStr(target)
                          << "defView=" << hwndStr(ctx.defView)
                          << "wallpaperWorker=" << hwndStr(ctx.wallpaperWorker);

        if (target) {
            // ⚠️ 关键：重挂父窗口前必须显式把样式改成 WS_CHILD（清掉 WS_POPUP），
            //    否则 SetParent 后 GetParent() 拿到的仍是 owner，窗口实际还是顶层窗口。
            const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
            SetWindowLongPtrW(hwnd, GWL_STYLE, (style & ~WS_POPUP) | WS_CHILD);

            // ⚠️ 编辑模式会留下 WS_EX_TOPMOST；不清掉的话下面 SetWindowPos(HWND_BOTTOM) 会被系统忽略
            if (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST)
                SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

            SetLastError(0);
            HWND prev = SetParent(hwnd, target);
            const DWORD err = GetLastError();
            qInfo() << "SetParent target=" << target << "prev=" << prev << "err=" << err;
            if (err != 0 && !prev) {
                // 兜底：直接改父窗口句柄
                SetWindowLongPtrW(hwnd, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(target));
                qInfo() << "fallback GWLP_HWNDPARENT ->" << GetParent(hwnd);
            }

            // ⚠️ 不能用 HWND_BOTTOM：Progman 下还有个承载壁纸的 WorkerW 子窗口，
            //    沉到底部会跑到壁纸"下面"→ 桌面上看不见。
            //    正确做法：插到 SHELLDLL_DefView（图标层）之后 → 图标之下、壁纸之上。
            POINT pt{before.left, before.top};
            MapWindowPoints(nullptr, target, &pt, 1);
            HWND anchor = ctx.defViewChild ? ctx.defViewChild : HWND_TOP;
            SetWindowPos(hwnd, anchor, pt.x, pt.y,
                         before.right - before.left, before.bottom - before.top,
                         SWP_NOACTIVATE | SWP_SHOWWINDOW);
            qInfo().noquote() << "z序锚点:" << hwndStr(anchor) << "(SHELLDLL_DefView)";

            // 强制重绘：父窗口重绘全部子窗口 + 自己 UpdateWindow（WM_PAINT 同步派发 → Qt 视为已曝光）
            RedrawWindow(target, nullptr, nullptr,
                         RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW | RDW_FRAME);
            RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            UpdateWindow(hwnd);
        }
    } else if (mode == Bottom) {
        SetWindowPos(hwnd, HWND_BOTTOM, before.left, before.top,
                     before.right - before.left, before.bottom - before.top,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    return describe(wid, modeName);
}
