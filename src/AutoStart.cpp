#include "AutoStart.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QSettings>

namespace {
const char* kRunKey = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
}

QString AutoStart::valueName()
{
    return QStringLiteral("DesktopWidget");
}

bool AutoStart::enabled()
{
    QSettings reg(QString::fromLatin1(kRunKey), QSettings::NativeFormat);
    return !reg.value(valueName()).toString().isEmpty();
}

QString AutoStart::command()
{
    QSettings reg(QString::fromLatin1(kRunKey), QSettings::NativeFormat);
    return reg.value(valueName()).toString();
}

bool AutoStart::setEnabled(bool on, QString* detail)
{
    QSettings reg(QString::fromLatin1(kRunKey), QSettings::NativeFormat);
    if (on) {
        // 带引号：路径里有空格也能正确启动
        const QString exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        reg.setValue(valueName(), QStringLiteral("\"%1\"").arg(exe));
    } else {
        reg.remove(valueName());
    }
    reg.sync();
    const QString after = reg.value(valueName()).toString();
    if (detail)
        *detail = after;
    const bool ok = on ? !after.isEmpty() : after.isEmpty();
    if (!ok)
        qWarning().noquote() << "[autostart] 注册表写入后校验失败:" << reg.status() << after;
    return ok;
}

bool AutoStart::refresh()
{
    if (!enabled())
        return false;
    const QString exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    const QString want = QStringLiteral("\"%1\"").arg(exe);
    if (command() == want)
        return false;
    // 绿色包被移动/改名后，旧路径就失效了 → 静默纠正
    QString after;
    const bool ok = setEnabled(true, &after);
    qInfo().noquote() << "[autostart] 路径已过期，纠正为" << after << (ok ? "[成功]" : "[失败]");
    return ok;
}
