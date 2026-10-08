# U8g2（裁剪版）

来源：[U8g2](https://github.com/olikraus/u8g2) 2.36.18 的 C 库部分（Arduino 库里的 `src/clib`），BSD-2-Clause 许可，见 `LICENSE`。

完整的 C 库有 40 多 MB，大部分是字库和几十种屏幕的驱动。移植到单片机工程时通常只留自己用到的部分，这里做了这些裁剪：

| 文件 | 处理 |
| --- | --- |
| `u8g2.h`、`u8x8.h` | 原样保留 |
| `u8g2_*.c`（画线、画框、字体渲染、缓冲区等）、`u8x8_*.c`（底层通信、命令序列） | 原样保留核心部分；菜单、日志、输入框等用不到的组件删掉了 |
| `u8x8_d_ssd1306_128x64_noname.c` | 只保留 SSD1306 128x64 这一个屏幕驱动（原来有 90 个驱动文件） |
| `u8g2_d_setup.c` | 原文件 450 KB，只摘出 `u8g2_Setup_ssd1306_i2c_128x64_noname_f()` 这一个函数 |
| `u8g2_d_memory.c` | 只摘出 128x64 全缓冲用的 `u8g2_m_16_8_f()`（1 KB 显存） |
| `u8g2_fonts.c` | 原文件约 40 MB，只摘出 `u8g2_font_6x10_tf` 一个字库 |

## 想换字体怎么办

1. 在 [U8g2 字体列表](https://github.com/olikraus/u8g2/wiki/fntlistall) 里挑一个，比如 `u8g2_font_ncenB14_tr`。
2. 从完整版 `u8g2_fonts.c` 里把这个字体的整段定义（`const uint8_t u8g2_font_xxx[...] = "...";`）复制到这里的 `u8g2_fonts.c` 末尾。
3. 代码里 `u8g2_SetFont(&u8g2, u8g2_font_xxx);` 即可。

字体名结尾的字母表示包含哪些字符：`_tf` 全部、`_tr` 只有 ASCII 可见字符、`_tn` 只有数字，字符越少占的 Flash 越小。

## 想换屏幕怎么办

比如 1.3 寸的 SH1106：它的驱动 `u8x8_d_sh1106_128x64_noname()` 就在已有的 `u8x8_d_ssd1306_128x64_noname.c` 里，只要再从完整版 `u8g2_d_setup.c` 里摘出 `u8g2_Setup_sh1106_i2c_128x64_noname_f()` 放进这里的 `u8g2_d_setup.c`，`u8g2_port.c` 里改调这个函数就行。其他型号要先把对应的 `u8x8_d_*.c` 驱动文件拷过来。
