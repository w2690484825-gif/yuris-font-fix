# yuris-font-fix

修复 **YU-RIS** 引擎 Galgame（如 **Whirlpool** 系列作品）在中文/非日文版 Windows（即使已转日区）下，设置界面 `フォント設定` 字体下拉列表完全空白、只能使用默认 `MS ゴシック` 的补丁。

Fix for the blank font selection dropdown (`フォント設定`) in **YU-RIS** engine visual novels (such as **Whirlpool** titles) when running on non-Japanese Windows (e.g. Chinese/English Windows with Japanese non-Unicode system locale).

---

## 问题描述

在中文版或英文版 Windows（非 Unicode 程序语言已设为日语）下运行 YU-RIS 引擎游戏时，打开 `CONFIG -> フォント設定`，输入框里只显示默认的 `MS ゴシック`，点开下拉菜单后列表一片空白，无法切换电脑里已经安装的 `游ゴシック`、`メイリオ`、`BIZ UDゴシック` 等日语字体。

### 原因说明
- YU-RIS 引擎在调用 `EnumFontFamiliesExA` 枚举字体时，不仅检查字符集，还会用硬编码的 Shift-JIS 字符串 `"日本語"` 去比对 `ENUMLOGFONTEXA.elfScript`。
- Windows 生成 `elfScript` 时依据的是**系统显示语言**，而不是非 Unicode 区域设置：
  - 在简体中文 Windows 下，GDI 会生成 `"日语"`，再按 Shift-JIS（CP932）转码；由于简体字 `语` 不在 Shift-JIS 字符集中，最终传给游戏的是 `"日?"`（`93 FA 3F`）。
  - 在英文 Windows 下，GDI 会生成 `"Japanese"`。
- 两者都无法匹配 `"日本語"`，导致所有日语字体全部被引擎过滤掉。

---

## 使用方法

1. 点击右侧 [Releases](../../releases) 页面，下载最新版的 `yuris-font-fix-v1.0.zip`。
2. 解压，将 `dsound.dll` 复制到游戏根目录（与游戏主程序 `.exe` 在同一文件夹）。
3. 正常启动游戏，打开 `CONFIG -> フォント設定` 即可看到完整的日语字体列表。
4. 支持通用复制：同一个 `dsound.dll` 可直接放到其他同款毛病的 Whirlpool / YU-RIS 作品目录中使用。

**卸载方法**：直接删除游戏目录里的 `dsound.dll` 即可，本补丁不修改任何游戏本体文件。

---

## 实现原理

- 这是一个轻量的 32 位 `dsound.dll` 代理补丁，将全部 12 个标准 DirectSound 接口原样转发给系统 `%SystemRoot%\SysWOW64\dsound.dll`，不影响游戏音效。
- 启动时对 `gdi32.dll!EnumFontFamiliesExA` 安装标准 2 字节热补丁（Hotpatch）。
- 当枚举到的字体条目满足 `lfCharSet == SHIFTJIS_CHARSET (128)` 时，在交给游戏回调函数前将 `elfScript` 修正为 Shift-JIS 编码的 `"日本語"`（`93 FA 96 7B 8C EA 00`）；其他字符集保持原样不动。
- 零侵入，不修改游戏 `.exe` 或加壳/DRM（兼容 SoftDenchi 等加密外壳）。

---

## 从源码编译

使用 `zig cc`（或 MinGW-w64 / MSVC）编译为 32 位 x86 Windows DLL：

```bash
zig cc -target x86-windows-gnu -shared -O2 -s -o dsound.dll dsound_fontfix.c dsound.def -lkernel32 -lgdi32 -luser32
```

---

## English Summary

- **Usage**: Download `yuris-font-fix-v1.0.zip` from [Releases](../../releases), extract `dsound.dll` next to the game's `.exe`, and launch the game. Delete `dsound.dll` to uninstall.
- **How it works**: Proxies all 12 exports of 32-bit `dsound.dll` to `SysWOW64\dsound.dll` and hotpatches `gdi32.dll!EnumFontFamiliesExA` to normalize `ENUMLOGFONTEXA.elfScript` to Shift-JIS `"日本語"` for `SHIFTJIS_CHARSET (128)` entries without modifying game executables or DRM.

## License

MIT License
