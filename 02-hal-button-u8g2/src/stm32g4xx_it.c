/*
 * 中断服务函数。函数名必须和启动文件 startup_stm32g474xx.s 里中断向量表的名字一模一样，
 * 写错一个字母就不会被调用（会掉进 Default_Handler 死循环）。
 */
#include "main.h"

void NMI_Handler(void)
{
  while (1) {
  }
}

/* 访问非法地址、栈溢出等都会进这里。用调试器暂停，看 SCB->CFSR 和栈里的 PC 就能找到出错的那行 */
void HardFault_Handler(void)
{
  while (1) {
  }
}

void MemManage_Handler(void)
{
  while (1) {
  }
}

void BusFault_Handler(void)
{
  while (1) {
  }
}

void UsageFault_Handler(void)
{
  while (1) {
  }
}

void SVC_Handler(void) {}
void DebugMon_Handler(void) {}
void PendSV_Handler(void) {}

/* 1 ms 一次，HAL_GetTick() 的计数就是在这里加的 */
void SysTick_Handler(void)
{
  HAL_IncTick();
}

/* EXTI10~15 共用一个中断入口，HAL 会检查是哪根线触发的，再调用 HAL_GPIO_EXTI_Callback() */
void EXTI15_10_IRQHandler(void)
{
  HAL_GPIO_EXTI_IRQHandler(B1_Pin);
}

/* TIM6 和 DAC 共用一个中断入口，HAL 处理完标志位后调用 HAL_TIM_PeriodElapsedCallback() */
void TIM6_DAC_IRQHandler(void)
{
  HAL_TIM_IRQHandler(&htim6);
}
