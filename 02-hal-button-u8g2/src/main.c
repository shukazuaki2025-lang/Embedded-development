/*
 * 第二个项目：HAL 库工程 + 按键状态机 + 把 U8g2 移植到 HAL
 *
 * 硬件：NUCLEO-G474RE + 0.96 寸 SSD1306 128x64 I2C OLED（接线和 01 一样）
 *
 * 这个文件的结构和 STM32CubeMX 生成的 main.c 一样：
 *   SystemClock_Config()    时钟树：内部 16 MHz HSI -> PLL -> 170 MHz
 *   MX_GPIO_Init()          B1 按键：上升沿 + 下降沿都触发 EXTI 中断
 *   MX_LPUART1_UART_Init()  串口 115200 8N1，printf 从这里输出
 *   MX_I2C1_Init()          I2C 400 kHz，接 OLED
 *   MX_TIM6_Init()          基本定时器，每 10 ms 产生一次更新中断
 * 业务代码放在 app.c，以后用 CubeMX 重新生成工程时，只要在 main() 里调用 app_init() / app_loop()。
 */
#include "main.h"
#include "app.h"

UART_HandleTypeDef hlpuart1;
I2C_HandleTypeDef  hi2c1;
TIM_HandleTypeDef  htim6;

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_LPUART1_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM6_Init(void);

int main(void)
{
  HAL_Init();             /* 复位所有外设、配置 SysTick 为 1 ms 中断（HAL_Delay / HAL_GetTick 靠它） */
  SystemClock_Config();   /* 上电默认跑 16 MHz，这里切到 170 MHz */

  MX_GPIO_Init();
  MX_LPUART1_UART_Init();
  MX_I2C1_Init();
  MX_TIM6_Init();

  app_init();
  while (1) {
    app_loop();
  }
}

/*
 * 170 MHz = 16 MHz(HSI) / M(4) * N(85) / R(2)
 * 超过 150 MHz 需要把内核电压调到 Range 1 Boost 模式，Flash 要加 4 个等待周期。
 */
static void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM            = RCC_PLLM_DIV4;
  RCC_OscInitStruct.PLL.PLLN            = 85;
  RCC_OscInitStruct.PLL.PLLP            = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ            = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR            = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    Error_Handler();
  }

  /* AHB、APB1、APB2 都不分频，所以 HCLK = PCLK1 = PCLK2 = 170 MHz */
  RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                   | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK) {
    Error_Handler();
  }
}

/* LD2 不在这里配置：app.c 里故意用直接写寄存器的方式配置它，和这里的 HAL 写法对照 */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();

  GPIO_InitStruct.Pin  = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;  /* 按下、松开都进中断，用来数“原始边沿” */
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /* 抢占优先级数字越小越优先：SysTick 0 > TIM6 2 > EXTI 3 */
  HAL_NVIC_SetPriority(B1_EXTI_IRQn, 3, 0);
  HAL_NVIC_EnableIRQ(B1_EXTI_IRQn);
}

static void MX_LPUART1_UART_Init(void)
{
  hlpuart1.Instance            = LPUART1;
  hlpuart1.Init.BaudRate       = 115200;
  hlpuart1.Init.WordLength     = UART_WORDLENGTH_8B;
  hlpuart1.Init.StopBits       = UART_STOPBITS_1;
  hlpuart1.Init.Parity         = UART_PARITY_NONE;
  hlpuart1.Init.Mode           = UART_MODE_TX_RX;
  hlpuart1.Init.HwFlowCtl      = UART_HWCONTROL_NONE;
  hlpuart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  hlpuart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  hlpuart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&hlpuart1) != HAL_OK) {
    Error_Handler();
  }
}

static void MX_I2C1_Init(void)
{
  hi2c1.Instance              = I2C1;
  hi2c1.Init.Timing           = 0x10802D9B;  /* CubeMX 按 I2C 时钟 170 MHz、Fast Mode 400 kHz 算出来的值 */
  hi2c1.Init.OwnAddress1      = 0;
  hi2c1.Init.AddressingMode   = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode  = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2      = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode  = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode    = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
    Error_Handler();
  }
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK) {
    Error_Handler();
  }
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK) {
    Error_Handler();
  }
}

/* TIM6 时钟 170 MHz，预分频 17000 -> 10 kHz，计 100 个数 -> 100 Hz，也就是 10 ms 中断一次 */
static void MX_TIM6_Init(void)
{
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  htim6.Instance               = TIM6;
  htim6.Init.Prescaler         = 17000 - 1;
  htim6.Init.CounterMode       = TIM_COUNTERMODE_UP;
  htim6.Init.Period            = 100 - 1;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim6) != HAL_OK) {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK) {
    Error_Handler();
  }
}

/* 初始化失败会停在这里。接上调试器暂停，看调用栈就知道是哪个外设没起来 */
void Error_Handler(void)
{
  __disable_irq();
  while (1) {
  }
}
