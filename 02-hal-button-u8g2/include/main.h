/*
 * 和 CubeMX 生成的 main.h 一样：放引脚定义和外设句柄声明
 */
#ifndef MAIN_H
#define MAIN_H

#include "stm32g4xx_hal.h"

/* 板载绿灯 LD2：PA5，高电平亮 */
#define LD2_Pin        GPIO_PIN_5
#define LD2_GPIO_Port  GPIOA

/* 板载蓝色按键 B1：PC13，板上有外部电阻，不用开内部上下拉 */
#define B1_Pin         GPIO_PIN_13
#define B1_GPIO_Port   GPIOC
#define B1_EXTI_IRQn   EXTI15_10_IRQn

/* 外设句柄，在 main.c 里定义 */
extern UART_HandleTypeDef hlpuart1;  /* LPUART1：接板载 ST-LINK 虚拟串口（PA2 TX / PA3 RX） */
extern I2C_HandleTypeDef  hi2c1;     /* I2C1：接 OLED（PB8 SCL / PB9 SDA），400 kHz */
extern TIM_HandleTypeDef  htim6;     /* TIM6：每 10 ms 中断一次，扫描按键 */

void Error_Handler(void);

#endif /* MAIN_H */
