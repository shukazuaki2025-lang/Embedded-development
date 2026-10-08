/*
 * printf 重定向到串口
 *
 * newlib 的 printf 最终调用 _write() 把字符输出去。默认的 _write() 什么也不做，
 * 我们自己实现一个，把字符从 LPUART1 发出去，电脑上的串口助手就能看到 printf 的内容。
 */
#include <stdio.h>
#include "main.h"

int _write(int file, char *ptr, int len)
{
  (void)file;
  for (int i = 0; i < len; i++) {
    if (ptr[i] == '\n') {
      uint8_t cr = '\r';  /* 换行前补一个回车，Windows 的串口助手显示才不会错位 */
      HAL_UART_Transmit(&hlpuart1, &cr, 1, HAL_MAX_DELAY);
    }
    HAL_UART_Transmit(&hlpuart1, (uint8_t *)&ptr[i], 1, HAL_MAX_DELAY);
  }
  return len;
}
