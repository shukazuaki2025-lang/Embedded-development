/*
 * U8g2 移植层
 *
 * U8g2 本身是纯 C 写的，和硬件无关。移植到一个新平台只需要提供两个回调函数：
 *   1. byte 回调：怎么把一串字节发给屏幕（这里是 I2C）
 *   2. gpio_and_delay 回调：怎么延时、怎么控制复位脚等 GPIO
 * 01 项目用的是 Arduino 版 U8g2，这两个回调由库用 Arduino 的 Wire / delay 实现好了；
 * 换成 HAL 以后要自己写，这就是“移植”。
 */
#include "u8g2_port.h"
#include "main.h"

uint8_t u8x8_byte_stm32_hal_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
  /* U8g2 在 START_TRANSFER 和 END_TRANSFER 之间最多发 32 字节，先攒起来，结束时一次性发出去 */
  static uint8_t buf[32];
  static uint8_t len;

  switch (msg) {
    case U8X8_MSG_BYTE_INIT:
      break;  /* I2C1 已经在 MX_I2C1_Init() 里初始化好了 */

    case U8X8_MSG_BYTE_START_TRANSFER:
      len = 0;
      break;

    case U8X8_MSG_BYTE_SEND: {
      const uint8_t *data = (const uint8_t *)arg_ptr;
      while (arg_int-- > 0 && len < sizeof(buf)) {
        buf[len++] = *data++;
      }
      break;
    }

    case U8X8_MSG_BYTE_END_TRANSFER:
      /* u8x8_GetI2CAddress() 返回的是左移过的 8 位地址（0x3C -> 0x78），HAL 要的也正是这个格式 */
      if (HAL_I2C_Master_Transmit(&hi2c1, u8x8_GetI2CAddress(u8x8), buf, len, 100) != HAL_OK) {
        return 0;
      }
      break;

    case U8X8_MSG_BYTE_SET_DC:
      break;  /* I2C 屏没有 D/C 引脚，命令和数据靠控制字节区分，U8g2 会自己处理 */

    default:
      return 0;
  }
  return 1;
}

uint8_t u8x8_gpio_and_delay_stm32_hal(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
  (void)arg_ptr;

  switch (msg) {
    case U8X8_MSG_GPIO_AND_DELAY_INIT:
      break;

    case U8X8_MSG_DELAY_MILLI:
      HAL_Delay(arg_int);
      break;

    case U8X8_MSG_DELAY_10MICRO:
      /* 只有软件模拟 I2C/SPI 才会用到微秒延时，硬件 I2C 用不到；这里给个粗略实现 */
      for (volatile uint32_t i = 0; i < 170u * 10u / 4u; i++) {
      }
      break;

    default:
      /* 4 针 I2C 屏没有复位脚等 GPIO，其余消息都直接返回“成功” */
      u8x8_SetGPIOResult(u8x8, 1);
      break;
  }
  return 1;
}

void oled_init(u8g2_t *u8g2)
{
  u8g2_Setup_ssd1306_i2c_128x64_noname_f(u8g2, U8G2_R0,
                                         u8x8_byte_stm32_hal_i2c,
                                         u8x8_gpio_and_delay_stm32_hal);
  u8x8_SetI2CAddress(&u8g2->u8x8, OLED_I2C_ADDR << 1);
  u8g2_InitDisplay(u8g2);       /* 发送 SSD1306 初始化命令序列，屏幕此时是关着的 */
  u8g2_SetPowerSave(u8g2, 0);   /* 打开显示 */
  u8g2_ClearBuffer(u8g2);
  u8g2_SendBuffer(u8g2);
}
