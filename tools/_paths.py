"""项目脚本共用的路径推导 —— 避免把本机用户名/绝对路径写进仓库。

程序的数据目录由 src/ConfigStore.cpp 用 QStandardPaths 决定：
    Windows: %APPDATA%\\wei\\DesktopWidget
其他平台上本项目不运行，但脚本仍能通过 DW_DATA_DIR 覆盖。
"""
import os

APP_DIR_NAME = "wei"
APP_NAME = "DesktopWidget"


def repo_root():
    return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def docs_dir():
    return os.path.join(repo_root(), "docs")


def data_dir():
    """程序运行时数据目录（config.json / widget.log / 自检截图都在这里）。"""
    env = os.environ.get("DW_DATA_DIR")
    if env:
        return env
    base = os.environ.get("APPDATA") or os.path.expanduser("~")
    return os.path.join(base, APP_DIR_NAME, APP_NAME)


def config_path():
    return os.path.join(data_dir(), "config.json")


def qt_dir():
    """Qt 安装目录（构建脚本用，可用 DW_QT_DIR 覆盖）。"""
    return os.environ.get("DW_QT_DIR") or r"D:\Qt\6.8.3\msvc2022_64"
