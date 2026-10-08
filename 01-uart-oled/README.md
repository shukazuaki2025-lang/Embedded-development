# 01 · 串口通信测试 + OLED 显示（NUCLEO-G474RE）

第一个项目：用 USB 线把 NUCLEO-G474RE 连到电脑，测试电脑和板子之间的串口通信；再接一块 OLED 屏，把收到的消息和通信状态显示在屏上。

## 你需要

| 东西 | 说明 |
| --- | --- |
| NUCLEO-G474RE | 板载 ST-LINK，烧录和串口都走同一根 USB 线 |
| 0.96 寸 OLED，SSD1306，128x64，I2C（4 针：GND VCC SCL SDA） | 1.3 寸 SH1106 也行，见下方“换屏” |
| USB 线 | Nucleo 上 ST-LINK 那个口（板子顶部） |
| 4 根杜邦线（母对母或公对母） | 接 OLED |

## 接线

OLED 接到 Nucleo 的 Arduino 排针上（排针上印有 D14、D15、3V3、GND）：

| OLED 引脚 | Nucleo 引脚 | STM32 引脚 |
| --- | --- | --- |
| GND | GND | – |
| VCC | 3V3 | – |
| SCL | D15 | PB8（I2C1_SCL） |
| SDA | D14 | PB9（I2C1_SDA） |

```
  OLED            NUCLEO-G474RE (Arduino 排针)
 ┌──────┐
 │ GND  ├──────── GND
 │ VCC  ├──────── 3V3
 │ SCL  ├──────── D15 / PB8
 │ SDA  ├──────── D14 / PB9
 └──────┘
```

串口不用额外接线：板载 ST-LINK 的虚拟串口（VCP）内部接在 LPUART1（PA2 TX / PA3 RX），USB 一插电脑就能看到一个串口。

## 最快的烧录方法：拖拽（不用装任何软件）

`firmware/uart_oled_nucleo_g474re.bin` 是已经编译好的程序。Nucleo 插上电脑后，“此电脑”里会出现一个叫 **NOD_G474RE** 的 U 盘，把这个 `.bin` 文件拖进去，ST-LINK 指示灯闪几下就烧录好了，U 盘会自动重新出现。

## 编译和烧录

改了代码以后需要自己编译，推荐用 [PlatformIO](https://platformio.org/)（VS Code 插件或命令行）：

```bash
cd 01-uart-oled
pio run                # 编译
pio run -t upload      # 烧录（通过板载 ST-LINK）
pio device monitor     # 打开串口监视器，115200
```

在 VS Code 里：用 PlatformIO 打开 `01-uart-oled` 文件夹，点底部状态栏的 ✓（编译）、→（烧录）、🔌（串口监视器）。

> Windows 第一次用 Nucleo 需要装 ST-LINK 驱动（STSW-LINK009）；装好后设备管理器里会出现 “STMicroelectronics STLink Virtual COM Port (COMx)”。

也可以用 Arduino IDE：安装 “STM32 MCU based boards”（stm32duino）板包和 U8g2 库，开发板选 Nucleo-64 → NUCLEO-G474RE，把 `src/main.cpp` 的内容复制成 `.ino` 即可。

## 测试通信

串口参数：**115200 波特率，8 数据位，无校验，1 停止位，发送时带换行（\n 或 \r\n 都可以）**。

### 方法 1：任意串口助手（XCOM、SSCOM、PuTTY、PlatformIO monitor 都行）

1. 烧录后按一下板子上的黑色复位键 B2，串口会打印：
   ```
   === 01-uart-oled: NUCLEO-G474RE ready ===
   I2C scan:
     found device at 0x3C
   OLED ok
   commands: ping | info | led on | led off | led blink | clear | help
   ```
2. 发送 `ping`，板子回复 `pong`。
3. 发送任意文字，比如 `hello`，板子回复 `ECHO: hello`，同时 OLED 上显示这条消息。
4. 按一下板子上的蓝色按键 B1，电脑会收到 `BTN: B1 pressed (board -> PC test)`，这是测试“板子 → 电脑”方向。

| 命令 | 板子的回复 |
| --- | --- |
| `ping` | `pong` |
| `info` | 板子型号、主频、运行时间、收发计数、OLED 状态 |
| `led on` / `led off` / `led blink` | 控制绿色 LED（LD2），上电默认 5 Hz 闪烁（亮 100 ms + 灭 100 ms） |
| `led fast` / `led slow` | 快闪 10 Hz（50 ms）/ 慢闪 1 Hz（500 ms） |
| `led blink 200` | 自定义闪烁半周期，单位 ms，范围 20–2000 |
| `clear` | 清空 OLED 上的消息和计数 |
| `help` | 列出命令 |
| 其他任何文字 | `ECHO: <你发的文字>` |

### 方法 2：自动测试脚本

```bash
pip install pyserial
python tools/serial_test.py --list          # 看看是哪个串口
python tools/serial_test.py COM5            # Windows，换成你的串口号
python tools/serial_test.py /dev/ttyACM0    # Linux
python tools/serial_test.py COM5 --chat     # 测完进入手动聊天模式
```

脚本会自动发 5 条命令并检查回复，全部通过就说明通信正常。

## OLED 上显示什么

```
┌────────────────────────┐
│UART <-> OLED 115200    │  标题和波特率
│RX3   TX5   00:01:23    │  收到行数 / 发出行数 / 运行时间
│────────────────────────│
│>hello                  │  最新收到的消息
│ ping                   │
│ info                   │
│LED:blink 100ms       ■│  LED 状态和闪烁半周期；右下角方块跟绿灯同步亮灭
└────────────────────────┘
```

OLED 字库只有英文，发中文会显示成 `?`。

## 常见问题

| 现象 | 处理 |
| --- | --- |
| 电脑上找不到串口 | 换一根能传数据的 USB 线；Windows 装 ST-LINK 驱动 |
| 串口有输出但全是乱码 | 波特率设成 115200 |
| 串口打印 `no I2C device found` | 检查 SDA/SCL 是否接反、VCC 是否接 3V3 |
| 扫描到的地址是 `0x3D` | 把 `src/main.cpp` 里 `OLED_I2C_ADDR` 改成 `0x3D` |
| 屏亮了但画面错位 / 右侧有花点 | 可能是 SH1106 屏，见下方“换屏” |
| 屏幕偶尔花屏或不刷新 | 杜邦线太长时 400 kHz 的 I2C 可能不稳，把 `src/main.cpp` 里 `u8g2.setBusClock(400000);` 这一行删掉（回到 100 kHz） |
| 发了文字没回复 | 串口助手要勾选“发送新行”/“加回车换行” |

## 换屏 / 换板子

- **1.3 寸 SH1106**：把 `src/main.cpp` 里的 `U8G2_SSD1306_128X64_NONAME_F_HW_I2C` 换成 `U8G2_SH1106_128X64_NONAME_F_HW_I2C`。
- **别的 STM32 板子**（Blue Pill、其他 Nucleo 等）：改 `platformio.ini` 里的 `board`，并按新板子的 I2C 引脚接线；如果板子没有 `USER_BTN`（比如 Blue Pill），把 `BUTTON_PIN` 改成你接按键的引脚。
- **ESP32 等非 STM32 芯片**：除了 `board`，还要把 `platform = ststm32` 改成对应平台（ESP32 是 `espressif32`），删掉 `upload_protocol` / `debug_tool` 两行，并把 `LED_PIN`、`BUTTON_PIN` 改成那块板子的引脚号；串口和 OLED 代码不用改。

## 文件

```
01-uart-oled/
├── platformio.ini        PlatformIO 工程配置
├── src/main.cpp          板子程序
├── firmware/             编译好的 .bin，可直接拖进 NOD_G474RE 盘
└── tools/serial_test.py  电脑端串口自动测试脚本
```
