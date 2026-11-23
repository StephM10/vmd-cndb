# Windows VMD 高 DPI / 4K 缩放调查

调查日期：2026-11-23。范围：Windows x64；会话报告安装版本为 VMD 2.0.0a7 / internal 2.0.0a6，路径 `C:\Program Files\University of Illinois\VMD2`，Tcl/Tk 8.6。本次只读检查安装脚本、查阅一手资料；未更改应用、Windows 设置或注册表。

## 结论与推荐路径

**把终端调到舒适字号很容易；让 VMD 所有面板在 4K 上清晰缩放属于中等规模的上游工作；让混合 FLTK/Tk 窗口在不同缩放比例的显示器之间实时切换，工程量较高。** 这是按下面的界面归属、工具包能力和测试面作出的工程判断。

推荐先识别终端类型并单独调整字号；剩余面板先评估 VMD 的 Windows `System` DPI 兼容性缩放，作为可读性补救。如果要维护 VMD 源码，优先建立 FLTK 1.4 的原生缩放路径，再逐个修复 Tk 插件的字体和布局，最后把混合 DPI 多屏切换列为独立验收目标。当前 cndb 文件读取插件没有 VMD 主窗口的布局所有权；VMD 把 Tcl/Tk GUI 扩展注册为独立菜单窗口。[VMD 插件接口](https://www.ks.uiuc.edu/Research/vmd/plugins/doxygen/tcltkplugins.html)

## 先区分四类窗口

| 用户看到的窗口 | 字号/缩放的所有者 | 可行的短期处理 | 工作量判断 |
|---|---|---|---|
| 随 VMD 启动的 Windows 原生文本控制台，或 Windows Terminal 中的 VMD | Console Host / Windows Terminal | Console Host 标题栏菜单 → Properties → Font；Terminal 的配置文件 → Appearance → Font size，或 `Ctrl`+`+` / `Ctrl`+`-` | 低；与主面板无联动 |
| 从菜单打开、标题为 `VMD TkConsole` 的 TkCon | VMD 附带 TkCon 脚本、Tk 字体 | 打开后使用其 `tkcon font` 接口设置较大点字号 | 低；仅改变控制台文字 |
| VMD Main、Representations 等内建面板 | VMD C++ / FLTK | 系统兼容性缩放可先改善可读性；原生修复需核实 FLTK 版本并重新构建 | 中 |
| Extensions 下的 Tk 插件对话框 | 各插件 Tcl/Tk 脚本和 Tk | 点字号、命名字体、布局尺寸和图标逐项处理 | 单个插件低至中；全套和多屏较高 |

Console Host 的 Properties 与 Defaults 有不同的持久化范围，前者可保存到应用项或启动快捷方式；只想调 VMD 时应使用该窗口的 Properties。[Microsoft 控制台设置](https://devblogs.microsoft.com/commandline/understanding-windows-console-host-settings/) Windows Terminal 的字号按点计并由配置文件管理。[Microsoft 字体设置](https://learn.microsoft.com/en-us/windows/terminal/customize-settings/profile-appearance) / [快捷键](https://learn.microsoft.com/en-us/windows/terminal/customize-settings/key-bindings) VMD 官方明确区分标准文本控制台和 TkCon。[VMD TkCon](https://www.ks.uiuc.edu/Research/vmd/plugins/vmdtkcon/)

Windows Terminal 能否承接当前 VMD 启动方式，需要实际测试控制台附着行为；现有 Console Host 的字号设置是最直接的路径。Microsoft 建议控制台应用尊重用户的字体选择；向 VMD 加入 `SetCurrentConsoleFontEx` 不能覆盖所有终端宿主。[Microsoft 控制台字体 API](https://learn.microsoft.com/en-us/windows/console/setcurrentconsolefontex)

## 本机 TkConsole 的具体证据

只读检查发现：

- `scripts/vmd/loadplugins.tcl:256` 将 `vmdtkcon` 注册为 `Plugins/Tk Console`。
- `plugins/noarch/tcl/vmdtkcon2.0/vmdtkcon.tcl:12` 设置标题 `VMD TkConsole`。
- `plugins/noarch/tcl/vmdtkcon2.0/tkcon-modified.tcl:668–669` 创建 `tkconfixed` 和粗体变体，使用 `-size -12`。
- 同文件 `:3666–3676` 的 `tkcon font` 分支直接配置控制台文本控件字体。

Tk 的负字号表示像素，正字号表示点；所以这里的默认值固定为 12 像素。[Tk 8.6 字体语义](https://www.tcl-lang.org/man/tcl8.6/TkCmd/font.htm) 按本机接口，打开 TkConsole 后可使用 `tkcon font Consolas 16` 设置 16 点字体；该命令由源码核实，本次未执行。它不调整 FLTK 面板，也不调整 TkConsole 菜单、状态栏和其他插件布局。

`tk scaling` 的单位是“像素/点”，换算为 Windows 缩放因子 s 时为 `96*s/72`，因此 100% 为约 1.333，200% 为约 2.667。它影响 Tk 后续的物理单位换算；Tk 8.6 文档对已有控件是否动态重排保留未定义行为，固定像素尺寸也需要另外处理。[Tk 8.6 scaling](https://www.tcl-lang.org/man/tcl8.6/TkCmd/tk.htm)

## Windows 兼容性补救的边界

DPI unaware 程序在 96 DPI 下绘制，Windows 再放大位图，因此可读性可以提高、锐度会下降。System-DPI-aware 程序通常只按会话的系统 DPI 布局一次；切换到另一缩放比例的显示器时，Windows 再做位图缩放。Per-Monitor V2 才是原生动态缩放目标。[Microsoft DPI 模型](https://learn.microsoft.com/en-us/windows/win32/hidpi/high-dpi-desktop-application-development-on-windows)

Windows 的应用属性兼容性页提供 DPI override。`System (Enhanced)` 使用 GDI Scaling 改善部分 GDI 文字/绘图；其收益依赖实际绘图 API，位图和其他 API 绘制的内容不自动获得同样改善，文字度量也可能造成裁切。因此对 VMD 这种含 OpenGL 的混合界面，先评估 `System` 的可读性，再单独比较 Enhanced，并检查分子视图、窗口尺寸和鼠标拾取。此处是建议，未对本机应用该设置。[Microsoft GDI Scaling 原理与配置](https://blogs.windows.com/windowsdeveloper/2017/05/19/improving-high-dpi-experience-gdi-based-desktop-apps/)

## 上游源码修复的工作范围

1. **核实构建依赖与 DPI 声明。** VMD 官方 2020 年源码已指出 Windows 高 DPI 需要 FLTK 1.4 的额外处理；这份公开源码不能直接代表本机 2025 年构建的实现。[VMD FLTK/OpenGL 源码](https://www.ks.uiuc.edu/Research/vmd/doxygen/FltkOpenGLDisplayDevice_8C-source.html) FLTK 1.4 在 Windows 提供每屏比例和默认 Per-Monitor V2；主面板可沿现有工具包升级，FLTK 的 ABI 变化要求重新编译。[FLTK Windows DPI](https://www.fltk.org/doc-1.4/osissues.html) / [官方发布说明](https://github.com/fltk/fltk/releases)
2. **统一区域和像素坐标。** FLTK 1.4 把控件尺寸、字体和绘图坐标统一为可缩放的逻辑单位；OpenGL viewport 则要使用 `pixel_w()` / `pixel_h()`。还需核对拾取坐标、窗口位置和导出截图尺寸。[FLTK 单位](https://www.fltk.org/doc-1.4/drawing.html) / [FLTK OpenGL 高 DPI](https://www.fltk.org/doc-1.4/opengl.html)
3. **协调 Tk 与进程 DPI 模式。** VMD 官方源码建议 Tk 使用 Ttk 和点单位布局。[VMD Tcl 初始化源码](https://www.ks.uiuc.edu/Research/vmd/doxygen/TclTextInterp_8C-source.html) Tk 8.6 的 Windows 上游讨论说明其使用 system DPI awareness。[Tk 官方 Windows 缺陷记录](https://core.tcl-lang.org/tk/tktview/bee96b4e80c5c26763a0a09be4e57f41a1473386) Tk 9 增强了可缩放控件和尺寸语义，但需要 Tcl 9，旧 C 扩展需重建并处理 API 变化；应单独安排迁移和插件回归。[Tk 9 发布与 ABI](https://www.tcl-lang.org/software/tcltk/9.0.html) / [Tk 9 脚本迁移](https://core.tcl-lang.org/tk/wiki?name=Migrating+scripts+to+Tk+9)
4. **正确设置 awareness 并处理变化。** Microsoft 推荐用 manifest 声明；API 方式必须在任何 HWND 创建之前调用。只加 awareness 声明会取消部分系统补偿，应用仍要重新布局、刷新字体与图标并处理 `WM_DPICHANGED`。从已经打开的控制台设置进程 awareness 太晚。[Microsoft awareness 初始化](https://learn.microsoft.com/en-us/windows/win32/hidpi/setting-the-default-dpi-awareness-for-a-process) / [动态 DPI 迁移要求](https://learn.microsoft.com/en-us/windows/win32/hidpi/high-dpi-desktop-application-development-on-windows)

验收至少覆盖 100%、150%、200% 下的冷启动；100%↔200% 显示器间拖动；运行中改变比例；主窗口、Representations、TkConsole、常用 Tk 插件的文字裁切和点击位置；OpenGL 视口、拾取与截图尺寸。初次启动正确与跨屏实时正确必须分别记录。

## 主面板现有字号和缩放入口核查

**在已查的 VMD 2.0 官方说明和本机配置脚本中，未找到可确认适用于该安装构建的主面板全局字号设置或缩放快捷键。** 官方 [VMD 2.0 Quick Guide](https://www.ks.uiuc.edu/Research/vmd/vmd2intro/quickguide.html) 没有给出该入口。本机 `vmd.rc`、`scripts/vmd` 和 `plugins/noarch/tcl/vmdprefs1.1/vmdprefs.tcl` 的定向搜索未找到主面板字体或 DPI 缩放配置；Preferences 的 Menus 页保存窗口开关和位置，Display 页的 `display resize` 改变分子显示窗口尺寸。它们的配置范围与主面板字号不同。[VMD Preferences 官方说明](https://www.ks.uiuc.edu/Research/vmd/plugins/vmdprefs/)

FLTK 1.4 文档提供 `Ctrl`+`+` / `Ctrl`+`-` / `Ctrl`+`0` 和 `FLTK_SCALING_FACTOR`，属于工具包能力；VMD 旧版公开源码也在 FLTK 1.4 的高 DPI 注释中列出这些机制。本机实际链接的 FLTK 版本和是否禁用快捷键尚待确认，因此这里只将它们列为待验证候选。[FLTK 缩放接口与快捷键](https://www.fltk.org/doc-1.4/group__fl__screen.html) / [VMD 对应源码注释](https://www.ks.uiuc.edu/Research/vmd/doxygen/FltkOpenGLDisplayDevice_8C-source.html)

## alpha 发布页与 UHD 宣称核查

检索时官方 Windows 最新测试下载仍列为 VMD 2.0.0a6（2025-08-12），条目原文为 **“Resizable main window, Qwikstructure option, Molywood GUI”**。这里明确承诺的是主窗口可调整大小；该条目没有描述字体随窗口改变、可调整字号、自动 DPI 缩放或逐显示器 DPI 切换。[VMD 官方测试版本](https://www.ks.uiuc.edu/Research/vmd/alpha/)

[VMD 2.0 介绍页](https://www.ks.uiuc.edu/Research/vmd/vmd2intro/) 另有支持触摸屏和超高清屏幕的总体宣称，但没有给出缩放比例、主面板字号入口、Windows DPI awareness 级别或跨屏验收结果。应把窗口可调整大小、UHD 总体支持和经验证的 DPI 缩放能力分别记录。

限制：本次未确认用户所指终端、安装构建的准确 FLTK 版本、manifest 或运行时 DPI context，未取得该 alpha 对应的完整 C++ GUI 源码或验证其内部快捷键，未执行字号命令或缩放实验；源码修复工作量为工程判断，Tk 9 的控件改善不构成对本机 VMD 混合多屏行为的验证。


## 本机临时会话验证补充

在独立、随后退出的原版 VMD 图形进程中调用 `vmdtkcon` 后，执行
`tkcon font Consolas 16`。`font actual` 从 Courier New、9 点变为 Consolas、
16 点；`font metrics -linespace` 从 15 变为 24。只改变该临时会话，未修改
用户当前窗口、持久字体配置或 Windows DPI 设置。日志保存在本地
`build/official-install/font-probe-result.txt`。这验证了终端文字调整接口，
没有验证主面板或跨显示器自动缩放。
