#pragma once

#include <QString>

// 开机自启：写 HKEY_CURRENT_USER\...\CurrentVersion\Run（HKCU，不需要管理员权限）
// 放在独立文件里是为了能被自检开关直接调用（托盘菜单点不到时也能验证）
namespace AutoStart {
QString valueName();          // 注册表值名（DesktopWidget）
bool enabled();               // 是否已设置
QString command();            // 当前写入的命令行
bool setEnabled(bool on, QString* detail = nullptr); // 返回是否成功，detail 回填写后的值
// 已开启时校验路径是否仍指向当前 exe（程序被移动/改名后自动纠正），返回是否改动过
bool refresh();
} // namespace AutoStart
