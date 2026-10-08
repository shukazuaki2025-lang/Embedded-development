/* 从 U8g2 的 u8g2_d_memory.c 里只摘出 128x64 全缓冲（1 KB 显存）这一个函数 */
#include "u8g2.h"

uint8_t *u8g2_m_16_8_f(uint8_t *page_cnt)
{
  #ifdef U8G2_USE_DYNAMIC_ALLOC
  *page_cnt = 8;
  return 0;
  #else
  static uint8_t buf[1024];
  *page_cnt = 8;
  return buf;
  #endif
}
