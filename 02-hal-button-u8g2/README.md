# 02 · HAL 库工程 + 按键状态机 + 移植 U8g2（NUCLEO-G474RE）

[学习路线](../ROADMAP.md) 阶段 1 的第一个项目。从这个项目开始，不再用 Arduino 框架，改用 ST 官方的 **HAL 库**（和 STM32CubeMX、STM32CubeIDE 用的是同一套代码）。

## 这个项目做什么

用板载蓝色按键 B1 控制绿灯 LD2，OLED 和串口实时显示按键事件：

| 操作 | 效果 |
| --- | --- |
| 单击 | LD2 亮/灭切换（停止闪烁） |
| 双击 | LD2 开始 2 Hz 闪烁 |
| 长按 0.8 秒 | 单击、双击、EXTI 边沿三个计数清零（长按次数保留） |

做完能学到：

- HAL 工程的结构：时钟配置、外设初始化、MSP、中断服务函数分别在哪里
- 时钟树：怎么从内部 16 MHz 配到 170 MHz
- `printf` 重定向到串口
- 定时器中断 + 按键消抖 + 状态机识别单击、双击、长按
- EXTI 外部中断，亲眼看到按键抖动
- 直接写寄存器点灯，对照 HAL 源码看 HAL 到底帮你做了什么
- 把第三方库（U8g2）移植到新平台

## 硬件

和 01 完全一样，不需要新零件：NUCLEO-G474RE + 0.96 寸 SSD1306 OLED，OLED 接 GND、3V3、D15（PB8，SCL）、D14（PB9，SDA）。接线图见 [01 的 README](../01-uart-oled/README.md#接线)。

## 烧录（拖拽，最快）

把 `firmware/hal_button_u8g2_nucleo_g474re.bin` 拖进“此电脑”里的 **NOD_G474RE**（F: 盘），ST-LINK 指示灯闪几下就烧好了。

## 测试

### 1. 串口

串口助手打开 COM3，**115200**，按一下黑色复位键 B2，应该看到：

```
=== 02-hal-button-u8g2: NUCLEO-G474RE (STM32 HAL) ===
SYSCLK = 170 MHz
I2C scan:
  found device at 0x3C
OLED ok (0x3C)
B1: click = toggle LD2, double-click = blink, long press 0.8 s = clear counters
```

`SYSCLK = 170 MHz` 说明时钟配置成功；`found device at 0x3C` 说明 OLED 接线没问题。

每次按键事件串口会打印一行，比如：

```
[   12345 ms] SHORT   short=1 double=0 long=0 exti_edges=2  LD2=on
```

### 2. OLED

```
┌────────────────────────┐
│02 HAL+U8g2  B1:up      │  ← B1 当前状态（消抖后），按住会变 down
│────────────────────────│
│short:1    dbl :0       │  ← 单击、双击次数
│long :0    EXTI:2       │  ← 长按次数、B1 引脚原始边沿数
│last : SHORT            │  ← 最近一次事件
│LD2  : on               │  ← 绿灯模式
│up 00:01:23 f:26ms    ■ │  ← 运行时间、刷新一帧用时、LD2 状态方块
└────────────────────────┘
```

### 3. 按这个表逐项测

| 操作 | 预期 |
| --- | --- |
| 上电 | LD2 2 Hz 闪烁，右下角方块跟着闪 |
| 单击 | 松手后约 0.25 秒 LD2 切换亮/灭，`short` +1 |
| 双击 | 第二下按下的瞬间 LD2 开始闪烁，`dbl` +1 |
| 按住不放 | 第一行变成 `B1:down`；满 0.8 秒时 `long` +1，`short`、`dbl`、`EXTI` 清零（不用等松手） |
| 快速连按 3 下 | 先报 DOUBLE，再报 SHORT |
| 连按 20 次单击（每次间隔 0.5 秒以上） | `short` 正好 +20，一次不多一次不少 |
| 看 `EXTI` | 每按一下（按下 + 松开）应该 +2；如果明显多于 2 倍，就是按键有抖动。这时 `short` 计数仍然准确，这就是消抖的作用 |
| 看 `f:` | 刷新一整屏要 25 ms 左右，期间 CPU 一直在等 I2C。下一个项目 03 用 DMA 解决这个问题 |

> 为什么单击要等 0.25 秒才生效？因为松手后要等一会儿，确认不是双击的第一下。想要单击立刻响应，就只能放弃双击，这是按键设计里常见的取舍。

## 代码结构

文件分成两类：CubeMX 会自动生成的，和需要自己写的。

| 文件 | 作用 | 谁写 |
| --- | --- | --- |
| `src/main.c` | 时钟配置、各外设初始化（`MX_xxx_Init`）、主循环 | CubeMX 生成 |
| `src/stm32g4xx_hal_msp.c` | 外设底层初始化：开时钟、配引脚复用、开中断 | CubeMX 生成 |
| `src/stm32g4xx_it.c` | 中断服务函数 | CubeMX 生成 |
| `include/stm32g4xx_hal_conf.h` | 编译哪些 HAL 模块 | CubeMX 生成 |
| `src/app.c` | 业务逻辑：LED（寄存器写法）、事件处理、OLED 界面 | 自己写 |
| `src/button.c` | 按键消抖 + 状态机，和硬件无关 | 自己写 |
| `src/u8g2_port.c` | U8g2 移植层：I2C 发送和延时两个回调 | 自己写 |
| `src/retarget.c` | `printf` 重定向到串口 | 自己写 |
| `lib/u8g2/` | 裁剪过的 U8g2 库，见 [lib/u8g2/README.md](lib/u8g2/README.md) | 第三方 |

一次按键从硬件到屏幕的完整路径：

```
TIM6 每 10 ms 溢出一次
  → TIM6_DAC_IRQHandler()            stm32g4xx_it.c，中断向量表里的名字
  → HAL_TIM_IRQHandler()             HAL 库：清标志位，判断是哪种中断
  → HAL_TIM_PeriodElapsedCallback()  app.c：读 B1 电平
  → button_update()                  button.c：消抖 + 状态机，识别出 SHORT
  → 放进事件队列
主循环 app_loop()
  → 从队列取出 SHORT → 切换 LD2、计数 +1、printf
  → oled_draw() → U8g2 → u8x8_byte_stm32_hal_i2c() → HAL_I2C_Master_Transmit()
```

## 知识点

### 1. 时钟树：16 MHz 怎么变成 170 MHz

```
HSI 16 MHz → ÷M(4) → 4 MHz → ×N(85) → 340 MHz → ÷R(2) → 170 MHz = SYSCLK
```

见 `main.c` 的 `SystemClock_Config()`。两个容易忽略的点：主频超过 150 MHz 要把内核电压调到 **Range 1 Boost**；Flash 读取跟不上 170 MHz，要设置 **4 个等待周期**（`FLASH_LATENCY_4`）。这两条 CubeMX 会自动处理，但面试可能会问为什么。

### 2. 为什么要消抖，为什么用定时器扫描

机械按键按下和松开的瞬间，触点会弹跳几毫秒，引脚电平来回跳好几次。如果每个边沿都算一次按下，按一下就可能被当成按了好几下。OLED 上的 `EXTI` 计数就是没消抖的原始边沿数，可以直接看到这个现象。

这里的做法是：TIM6 每 10 ms 读一次引脚，**新电平必须连续 2 次（20 ms）一致才认为真的变了**。用定时器扫描而不是 EXTI 中断，是因为抖动会让 EXTI 中断连续触发很多次，而定时扫描的开销是固定的，和抖动多少无关。EXTI 更适合真正需要立刻响应的信号，比如编码器、急停、低功耗唤醒。

### 3. 按键状态机

见 `button.c` 开头的状态图。四个状态：`IDLE`（没按）、`PRESSED`（按下了）、`WAIT_SECOND`（松开了，等第二下）、`WAIT_RELEASE`（已报告长按或双击，等松开）。三个时间参数在 `button.h` 里：消抖 20 ms、长按 800 ms、双击间隔 250 ms。

`button.c` 不碰任何硬件，只接收“这次读到的电平”，所以可以直接在电脑上用 gcc 编译测试，也能拿去给别的按键、别的单片机用。这种“算法和硬件分开”的写法后面每个项目都会用到。

### 4. 寄存器 vs HAL

`app.c` 里的 `led_init_reg()` 用 6 行寄存器操作配置 PA5：开 GPIOA 时钟（`RCC->AHB2ENR`）、设成输出（`MODER`）、推挽（`OTYPER`）、低速（`OSPEEDR`）、无上下拉（`PUPDR`）、输出低电平（`BRR`）。CubeMX 生成的代码里这些步骤都藏在 `HAL_GPIO_Init()` 里。

再看 HAL 库里 `HAL_GPIO_WritePin()` 的源码（`stm32g4xx_hal_gpio.c`），去掉参数检查就两行：

```c
if (PinState != GPIO_PIN_RESET)
  GPIOx->BSRR = (uint32_t)GPIO_Pin;   /* 置高 */
else
  GPIOx->BRR = (uint32_t)GPIO_Pin;    /* 置低 */
```

和 `led_write_reg()` 一模一样。为什么用 BSRR/BRR 而不是 `ODR |= ...`？因为 `ODR |= x` 是“读-改-写”三步，如果中间被中断打断、中断里也改了 ODR，就会把中断的修改覆盖掉；写 BSRR/BRR 是一条指令，只影响写 1 的那一位，天然不怕打断。

### 5. U8g2 移植

U8g2 是纯 C 库，和硬件无关。移植只需要实现两个回调（`u8g2_port.c`）：

- `u8x8_byte_stm32_hal_i2c()`：U8g2 要发数据时，先攒进 32 字节缓冲区，传输结束时用 `HAL_I2C_Master_Transmit()` 一次发出
- `u8x8_gpio_and_delay_stm32_hal()`：U8g2 要延时时调 `HAL_Delay()`；4 针 I2C 屏没有复位脚，GPIO 相关的消息直接返回成功

一个坑：U8g2 和 HAL 都用**左移一位的 8 位地址**，OLED 的 7 位地址 0x3C 要写成 `0x3C << 1`（0x78）。

另外完整的 U8g2 有 40 多 MB（大部分是字库），这里只保留了用到的部分，详见 [lib/u8g2/README.md](lib/u8g2/README.md)。

### 6. 中断优先级

数字越小优先级越高：SysTick 0 > TIM6 2 > EXTI 3。SysTick 最高，保证 `HAL_GetTick()` 计时准确；TIM6 的按键扫描比 EXTI 计数重要，所以比 EXTI 高。高优先级的中断可以打断正在执行的低优先级中断（抢占）。

### 7. 中断和主循环怎么安全地共享数据

TIM6 中断识别出事件后不直接处理（处理里有 `printf` 和刷屏，太慢，中断里不能做），而是放进一个 8 格的环形队列，主循环再取出来。队列只有中断写 `evt_head`、只有主循环写 `evt_tail`，一个写一个读，所以不需要关中断。被中断和主循环共同访问的变量都加了 `volatile`，告诉编译器每次都要真的去内存里读，不能用寄存器里缓存的旧值。

## 自己用 CubeMX 配一遍（推荐做）

这个项目的初始化代码是照着 CubeMX 的格式手写的，方便在云端编译。建议你自己用 CubeMX 配一遍，再和这里的代码对照，这是工作中每天都在用的技能。CubeMX 版本不同，界面细节可能略有区别。

1. 安装 STM32CubeIDE（里面自带 CubeMX），新建工程，在 **Board Selector** 里选 NUCLEO-G474RE。问是否按默认初始化所有外设时选 **No**，自己配。
2. **Pinout**：
   - PC13 设为 `GPIO_EXTI13`，在 System Core → GPIO 里把模式改成上升沿和下降沿都触发（Rising/Falling edge），No pull，User Label 填 `B1`
   - PA5 设为 `GPIO_Output`，User Label 填 `LD2`
   - Connectivity → **LPUART1** → Asynchronous，115200，8N1，确认引脚是 PA2、PA3
   - Connectivity → **I2C1** → I2C，Speed Mode 选 Fast Mode（400 kHz），引脚确认或改成 PB8（SCL）、PB9（SDA）
   - Timers → **TIM6** → 勾选 Activated，Prescaler 填 `16999`，Counter Period 填 `99`
   - System Core → **NVIC**：勾上 TIM6 和 EXTI line[15:10] 的中断，抢占优先级分别填 2 和 3
3. **Clock Configuration**：PLL Source 选 HSI，在 HCLK 里填 `170` 回车，CubeMX 会自动算出 M=4、N=85、R=2。
4. 生成代码，然后：
   - 把这里的 `app.c/.h`、`button.c/.h`、`u8g2_port.c/.h`、`retarget.c` 拷进工程的 `Core/Src`、`Core/Inc`，`lib/u8g2` 整个文件夹拷进工程并加到头文件路径
   - 在 `main.c` 的 `/* USER CODE BEGIN 2 */` 里调用 `app_init();`，在 `while (1)` 里的 `/* USER CODE BEGIN 3 */` 里调用 `app_loop();`
   - CubeIDE 生成的 `syscalls.c` 里也有一个 `_write()`，它是弱定义，会被 `retarget.c` 里的覆盖，不用删
5. 对照检查：CubeMX 生成的 `SystemClock_Config()`、`MX_I2C1_Init()` 里的 I2C `Timing` 值应该和这里一样。

## 改代码后自己编译

和 01 一样用 PlatformIO：

```bash
cd 02-hal-button-u8g2
pio run                # 编译
pio run -t upload      # 烧录
pio device monitor     # 串口监视器
```

第一次编译会下载 ST 的 HAL 库包（framework-stm32cubeg4，几十 MB），网速慢的话先用上面的拖拽方式测试。

## 过关自测

路线图里这个项目的标准：

- [ ] 快速连按、长按都不误判（按上面的测试表逐项过一遍）
- [ ] 能说清楚 `HAL_GPIO_WritePin()` 最后写的是哪个寄存器

面试常问，先自己想，再展开看参考答案：

<details>
<summary>中断服务函数里不能做什么？</summary>

不能做耗时的事：`printf`、阻塞式串口/I2C 收发、`HAL_Delay()`（它靠 SysTick 中断计时，如果所在中断的优先级不低于 SysTick，计时就停了，会永远卡住）、复杂计算、动态分配内存。中断里只做最少的事（读数据、置标志、放进队列），剩下的交给主循环。这个项目里刷屏和 `printf` 都在主循环里做，就是这个原因。

</details>

<details>
<summary>为什么要消抖？有哪些方法？</summary>

机械触点弹跳会让一次按下产生多个边沿。方法有硬件消抖（RC 滤波 + 施密特触发器）和软件消抖（延时再确认、定时扫描连续 N 次一致）。软件里最常用的是定时扫描，开销固定，不会被抖动拖累。

</details>

<details>
<summary><code>volatile</code> 什么时候必须加？</summary>

变量的值可能在编译器“看不见”的地方被改的时候：被中断服务函数修改的全局变量、多任务共享的变量、外设寄存器（ST 的头文件里寄存器都定义成了 `volatile`）。不加的话，编译器优化时可能把变量缓存在寄存器里，主循环永远读不到中断里的新值。注意 `volatile` 只保证每次都真的去读写内存，不保证“读-改-写”是原子的。

</details>
