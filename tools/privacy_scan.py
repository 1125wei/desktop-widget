"""发布前隐私扫描：推送公开仓库前跑一遍，命中即需处理。

覆盖：API Key / GitHub token / 手机号 / 邮箱 / 本机用户名路径 / 内网地址。
用法：python tools/privacy_scan.py [目录]（默认扫项目根目录）
"""
import os
import re
import sys

PATTERNS = [
    ("OpenAI/DeepSeek 等 sk- key", re.compile(r"sk-[A-Za-z0-9_\-]{16,}")),
    ("GitHub classic token", re.compile(r"ghp_[A-Za-z0-9]{20,}")),
    ("GitHub fine-grained token", re.compile(r"github_pat_[A-Za-z0-9_]{20,}")),
    ("GitHub OAuth token", re.compile(r"gho_[A-Za-z0-9]{20,}")),
    ("手机号", re.compile(r"(?<!\d)1[3-9]\d{9}(?!\d)")),
    ("邮箱", re.compile(r"[\w.\-]+@[\w\-]+\.[A-Za-z]{2,}")),
    ("Windows 用户目录（泄漏用户名）", re.compile(r"[Cc]:\\+Users\\+[^\\/\s\"']+")),
    ("MSYS 用户目录", re.compile(r"/c/Users/[^/\s\"']+")),
    ("内网地址", re.compile(r"\b(?:10|172\.(?:1[6-9]|2\d|3[01])|192\.168)\.\d{1,3}\.\d{1,3}\b")),
    ("私钥", re.compile(r"-----BEGIN [A-Z ]*PRIVATE KEY-----")),
    ("Bearer 串", re.compile(r"[Bb]earer\s+[A-Za-z0-9._\-]{20,}")),
]

SKIP_DIRS = {".git", "build", "dist", ".vs", ".cache", "__pycache__", "node_modules"}
SKIP_EXT = {".png", ".jpg", ".jpeg", ".ico", ".zip", ".exe", ".dll", ".pdb", ".obj", ".so", ".ttf"}
# 扫描器自身含正则原文，跳过（否则永远自命中）
SKIP_FILES = {"privacy_scan.py"}

# 允许出现的"假命中"（本机路径是构建脚本的默认值，可在环境变量里覆盖）
ALLOW = [
    r"[Cc]:\\Users\\%USERNAME%",
    r"/c/Users/\$\(whoami\)",
    r"/c/Users/\$USER",
]


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else os.getcwd()
    hits = 0
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
        for fn in filenames:
            if os.path.splitext(fn)[1].lower() in SKIP_EXT or fn in SKIP_FILES:
                continue
            path = os.path.join(dirpath, fn)
            try:
                text = open(path, encoding="utf-8", errors="ignore").read()
            except OSError:
                continue
            for label, rx in PATTERNS:
                for m in rx.finditer(text):
                    s = m.group(0)
                    if any(re.fullmatch(a, s) for a in ALLOW):
                        continue
                    line = text[: m.start()].count("\n") + 1
                    rel = os.path.relpath(path, root)
                    print(f"[命中] {label}: {rel}:{line}  →  {s}")
                    hits += 1
            # 本机开发路径（D:\\Qt 等）单独提示：不算隐私，但公开前最好可配置
    print(f"\n扫描完成，命中 {hits} 处。" + ("需要处理后再推送。" if hits else "干净，可以推送。"))


if __name__ == "__main__":
    main()
