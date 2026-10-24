# ZIPedIT Crystal

原生 Win32 桌面程序，纯 C（C11）实现，零运行时依赖 —— 用于**「套娃压缩包」递归解压并收集图片**。
这是原 Python + PySide6 版本的完全重写，界面用 GDI 自绘，不依赖 Qt 或任何第三方库。

> 版本：v4.2 · Crystal (C)　平台：Windows 10 / 11 (x64)

## 功能

- **递归解压**：支持 `.7z / .zip / .rar / .tar / .tar.gz / .tgz` 主包与子包逐层展开
- **密码尝试**：一级密码 + 二级密码列表批量试探，可从文本文件导入
- **两种模式**：仅收集图片 / 完整解压（保留目录结构）
- **去重与整理**：可选 MD5 内容去重、按来源分子目录、按扩展名筛选
- **批量处理**：多文件选择、拖拽文件或文件夹、任务历史、设置持久化
- **图库预览**：基于 WIC 的缩略图网格
- **日志**：实时滚动日志，支持导出
- **界面**：深色玻璃质感 —— 渐变卡片、进度环、进度条、动态背景
- **彩蛋**：连点 Logo、版本号循环、Konami 极客模式

## 解压引擎

程序本身不内置解压算法，而是调度系统能力，两条路径按设置中的优先顺序尝试：

| 引擎 | 依赖 | 可处理 | 说明 |
|---|---|---|---|
| 内置调度 | 系统自带 `tar.exe`（libarchive） | zip / tar / tar.gz / tgz | Windows 10 1803+ 默认自带，无需安装 |
| 后备引擎 | `7z.exe / 7za.exe / 7zr.exe` | 7z / rar / 加密包 | 在 PATH 或常见安装目录中自动查找，也可在设置里手动指定 |

若机器上既没有 7-Zip、压缩包又是 7z/rar，程序不会假装成功 —— 日志会明确提示缺少引擎。

## 构建

需要 MinGW-w64 的 `gcc`（支持 C11）和 `make`。推荐直接用
[w64devkit](https://github.com/skeeto/w64devkit/releases)：单个压缩包，解压即用，无需安装。

```bat
:: 方式一：把 w64devkit 解压到项目根目录（工具链不入库，需自行下载）
::   解压后形如 tools\w64devkit\bin\gcc.exe
make

:: 方式二：系统 PATH 里已有 gcc，直接构建
set PATH=C:\path\to\w64devkit\bin;%PATH%
make

:: 方式三：显式指定编译器
make CC=C:\msys64\ucrt64\bin\gcc.exe
```

`Makefile` 的编译器优先级是：命令行 `CC=` → `tools/w64devkit/bin/gcc.exe` → PATH 中的 `gcc`。

也可以不用 make，直接一行：

```bat
gcc -O2 -Wall -Wextra -std=c11 -municode -DUNICODE -D_UNICODE ^
  -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00 -D_WIN32_IE=0x0A00 ^
  -o zipedit.exe src\main.c src\ui.c src\worker.c src\dialogs.c src\selftest.c src\util.c ^
  -lgdi32 -luser32 -lcomctl32 -lcomdlg32 -lshell32 -lole32 -luuid -ladvapi32 -lbcrypt -lmsimg32
```

## 运行

```bat
zipedit.exe              :: 图形界面
zipedit.exe --selftest   :: 命令行自检，不弹窗
```

或者：

```bat
make selftest
```

自检会临时生成多层嵌套压缩包（zip / tar / tar.gz），验证三件事并自行清理：

```
[1/2] images mode + MD5 dedup: OK (4 files, 1 skipped)
[2/2] all mode + subdir:      OK (4 files)
[3/3] history read/write:     OK
SELFTEST OK: dedup / subdir / all-mode / multi-format passed
```

## 配置与数据

| 内容 | 位置 |
|---|---|
| 设置 | `%APPDATA%\ZIPedIT\config.ini` |
| 历史 | `%APPDATA%\ZIPedIT\history.txt` |

设置 `ZIPEDIT_DATA` 环境变量可整体覆盖数据目录（便携使用 / 多份配置隔离）。

## 源码结构

| 文件 | 职责 |
|---|---|
| `src/app.h` | 公共头文件：窗口消息、控件 ID、主题色、全局结构体 |
| `src/main.c` | 入口分发（GUI / `--selftest`）、单实例与初始化 |
| `src/ui.c` | 主窗口与全部自绘控件：卡片、按钮、进度环、渐变条、拖放区 |
| `src/dialogs.c` | 设置 / 关于 / 历史 / 图片图库四个对话框 |
| `src/worker.c` | 解压核心与后台工作线程（引擎调度、递归、密码表、去重） |
| `src/util.c` | 路径、配置读写、历史记录、MD5（bcrypt）、DIB 渲染 |
| `src/selftest.c` | 命令行自检 |

代码约 4100 行（`ui.c` 1521 / `dialogs.c` 780 / `util.c` 688 / `worker.c` 552 / `app.h` 299 / `selftest.c` 217 / `main.c` 38），
只用 Win32 + 系统库，链接 `gdi32 user32 comctl32 comdlg32 shell32 ole32 uuid advapi32 bcrypt msimg32`。

## 已知限制

- 仅 Windows。`tar.exe` 与 Win32 API 都是平台绑定的。
- 7z / rar 依赖外部 7-Zip，未内置解码器。
- 密码表是顺序尝试，不是字典攻击优化器；`MAX_PWD` 为 256 字符。

## 许可

MIT，见 [LICENSE](LICENSE)。
