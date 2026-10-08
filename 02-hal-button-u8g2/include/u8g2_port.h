/*
 * U8g2 移植层：把 U8g2 需要的“发 I2C 数据”和“延时”两件事，用 STM32 HAL 实现
 */
#ifndef U8G2_PORT_H
#define U8G2_PORT_H

#include "u8g2.h"

#define OLED_I2C_ADDR 0x3C  /* 7 位地址；有些模块是 0x3D */

/* 回调 1：U8g2 要发字节时调用，我们用 HAL_I2C_Master_Transmit 发出去 */
uint8_t u8x8_byte_stm32_hal_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);

/* 回调 2：U8g2 要延时或控制 GPIO（复位脚等）时调用；硬件 I2C 只需要毫秒延时 */
uint8_t u8x8_gpio_and_delay_stm32_hal(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);

/* 初始化 SSD1306 128x64（全缓冲模式，显存 1 KB 放在 RAM 里） */
void oled_init(u8g2_t *u8g2);

#endif /* U8G2_PORT_H */
