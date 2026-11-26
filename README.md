# yuris-font-fix

Fix for the blank font selection dropdown (`フォント設定`) in **YU-RIS** engine visual novels (such as **Whirlpool** titles) when running on non-Japanese Windows (e.g., Chinese / English Windows with Japanese non-Unicode system locale).

修复 **YU-RIS** 引擎 Galgame（如 **Whirlpool** 系列作品）在非日文版 Windows（例如中文/英文界面 Windows + 日区非 Unicode 设置）下，设置界面 `フォント設定` 字体下拉列表完全空白、只能显示默认 `MS ゴシック` 的问题。

---

## Problem / 问题描述

When running YU-RIS engine games on Chinese or English Windows with Japanese system locale (`ACP = 932`), opening `CONFIG -> フォント設定` shows `MS ゴシック` in the box, while the dropdown list is completely empty (0 selectable fonts), even though Japanese fonts are installed.

在中文/英文版 Windows（已转日区）下运行 YU-RIS 引擎游戏时，打开设置中的 `フォント設定`，下拉列表一片空白，无法切换电脑里已安装的游ゴシック、メイリオ等其他日语字体。

### Root Cause / 原因

- YU-RIS enumerates fonts via `EnumFontFamiliesExA` and filters entries by comparing `ENUMLOGFONTEXA.elfScript` against the hardcoded Shift-JIS string `"日本語"`.
- On Windows, GDI generates `elfScript` using the **OS Display Language**, not the non-Unicode system locale:
  - On Chinese Windows, GDI generates `"日语"`, then converts it to Shift-JIS (`CP932`). Since Simplified Chinese `语` (`U+8BED`) does not exist in Shift-JIS, it becomes `"日?"` (`93 FA 3F`).
  - On English Windows, GDI generates `"Japanese"`.
- Neither matches `"日本語"`, so every Japanese font is rejected.

---

## Usage / 使用方法

1. Download `yuris-font-fix-v1.0.zip` from the [Releases](../../releases) page.
2. Extract `dsound.dll` into the game's root directory (next to the game's `.exe`).
3. Launch the game normally and open `CONFIG -> フォント設定`.

To uninstall, simply delete `dsound.dll`.

1. 从右侧 [Releases](../../releases) 页面下载 `yuris-font-fix-v1.0.zip`。
2. 解压，将 `dsound.dll` 放到游戏根目录（与游戏主程序 `.exe` 同级）。
3. 正常启动游戏，进入设置界面的 `フォント設定` 即可看到完整的日语字体列表。

如需卸载，直接删除 `dsound.dll` 即可，不修改任何游戏本体文件。

---

## How It Works / 原理

- Acts as a lightweight 32-bit `dsound.dll` proxy that forwards all 12 standard DirectSound exports to `%SystemRoot%\SysWOW64\dsound.dll`.
- Installs a minimal 2-byte hotpatch on `gdi32.dll!EnumFontFamiliesExA`.
- Whenever an enumerated font has `lfCharSet == SHIFTJIS_CHARSET (128)`, rewrites `elfScript` to Shift-JIS `"日本語"` (`93 FA 96 7B 8C EA 00`) before forwarding to the game's callback. Other charsets are left untouched.
- Does not modify game executables or DRM wrappers (compatible with SoftDenchi).

---

## Building from Source / 从源码编译

Compiled as a 32-bit x86 Windows DLL using `zig cc` (or MinGW-w64 / MSVC):

```bash
zig cc -target x86-windows-gnu -shared -O2 -s -o dsound.dll dsound_fontfix.c dsound.def -lkernel32 -lgdi32 -luser32
```

## License

MIT License
