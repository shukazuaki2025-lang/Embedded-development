/*
 * 业务逻辑
 *
 *   B1 单击：LD2 亮/灭切换（并停止闪烁）
 *   B1 双击：LD2 开始 2 Hz 闪烁
 *   B1 长按 0.8 s：单击、双击、EXTI 边沿计数清零（长按次数保留）
 *
 * 按键由 TIM6 中断每 10 ms 扫描一次，状态机识别出的事件放进一个小队列，主循环取出来处理。
 * 这样主循环刷 OLED（一帧要二十多毫秒）的时候，按键扫描也不会被耽误。
 */
#include <stdio.h>
#include "main.h"
#include "app.h"
#include "button.h"
#include "u8g2_port.h"

#define BLINK_HALF_PERIOD_MS 250u  /* 2 Hz 闪烁：亮 250 ms，灭 250 ms */

typedef enum { LED_MODE_OFF, LED_MODE_ON, LED_MODE_BLINK } led_mode_t;

/* ---------------- LD2：直接操作寄存器 ----------------
 * 用 HAL 的话就是 MX_GPIO_Init() 里一个 HAL_GPIO_Init() 加 HAL_GPIO_WritePin()。
 * 这里每一步都对应 RM0440 参考手册里 RCC 和 GPIO 章节的一个寄存器。 */
static void led_init_reg(void)
{
  RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;        /* 1. 打开 GPIOA 的时钟，不开时钟写 GPIO 寄存器没有任何效果 */
  (void)RCC->AHB2ENR;                         /*    读回一次，保证时钟已经生效再往下走 */
  GPIOA->MODER = (GPIOA->MODER & ~GPIO_MODER_MODE5) | GPIO_MODER_MODE5_0;  /* 2. PA5 设为通用输出（MODE5 = 01） */
  GPIOA->OTYPER  &= ~GPIO_OTYPER_OT5;         /* 3. 推挽输出 */
  GPIOA->OSPEEDR &= ~GPIO_OSPEEDR_OSPEED5;    /* 4. 低速就够了，翻转越慢干扰越小 */
  GPIOA->PUPDR   &= ~GPIO_PUPDR_PUPD5;        /* 5. 不要上下拉 */
  GPIOA->BRR = LD2_Pin;                       /* 6. 先输出低电平（灭） */
}

/* BSRR 写 1 置高、BRR 写 1 置低，一条指令完成、不用“读-改-写”，所以不怕被中断打断 */
static void led_write_reg(uint8_t on)
{
  if (on) {
    GPIOA->BSRR = LD2_Pin;
  } else {
    GPIOA->BRR = LD2_Pin;
  }
}

static uint8_t led_is_on(void)
{
  return (GPIOA->ODR & LD2_Pin) ? 1u : 0u;  /* ODR 是当前输出的电平 */
}

/* ---------------- 中断里产生、主循环里消费的数据 ---------------- */
#define EVT_QUEUE_LEN 8u  /* 2 的整数次方 */

static button_t          btn;           /* 只在 TIM6 中断里访问 */
static volatile uint8_t  b1_down;       /* 消抖后的按键状态，中断里写、主循环里读 */
static volatile uint32_t exti_edges;    /* B1 引脚的原始边沿数（没有消抖） */
static volatile uint32_t evt_dropped;   /* 队列满了丢掉的事件数 */
static volatile uint8_t  evt_queue[EVT_QUEUE_LEN];
static volatile uint8_t  evt_head;      /* 只有中断写 */
static volatile uint8_t  evt_tail;      /* 只有主循环写 */

/* TIM6 每 10 ms 进来一次（HAL_TIM_IRQHandler 调用） */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance != TIM6) {
    return;
  }
  uint8_t raw = (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_SET) ? 1u : 0u;
  button_event_t evt = button_update(&btn, raw);
  b1_down = button_is_down(&btn);

  if (evt != BUTTON_EVT_NONE) {
    uint8_t next = (uint8_t)((evt_head + 1u) % EVT_QUEUE_LEN);
    if (next == evt_tail) {
      evt_dropped++;
    } else {
      evt_queue[evt_head] = (uint8_t)evt;
      evt_head = next;
    }
  }
}

/* B1 每个上升沿、下降沿都进来一次（HAL_GPIO_EXTI_IRQHandler 调用）。只计数，看看抖动有多少 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == B1_Pin) {
    exti_edges++;
  }
}

static button_event_t evt_pop(void)
{
  if (evt_tail == evt_head) {
    return BUTTON_EVT_NONE;
  }
  button_event_t evt = (button_event_t)evt_queue[evt_tail];
  evt_tail = (uint8_t)((evt_tail + 1u) % EVT_QUEUE_LEN);
  return evt;
}

/* ---------------- 主循环里的状态 ---------------- */
static u8g2_t         u8g2;
static uint8_t        oled_ok;
static led_mode_t     led_mode = LED_MODE_BLINK;
static uint32_t       blink_last_ms;
static uint32_t       cnt_short, cnt_double, cnt_long;
static button_event_t last_evt = BUTTON_EVT_NONE;
static uint32_t       frame_ms;  /* 上一次刷新整屏用了多少毫秒 */

static const char *led_mode_name(void)
{
  switch (led_mode) {
    case LED_MODE_ON:  return "on";
    case LED_MODE_OFF: return "off";
    default:           return "blink 2Hz";
  }
}

static void i2c_scan(void)
{
  uint8_t found = 0;
  printf("I2C scan:\n");
  for (uint8_t addr = 1; addr < 0x7F; addr++) {
    if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(addr << 1), 1, 5) == HAL_OK) {
      printf("  found device at 0x%02X\n", addr);
      found++;
    }
  }
  if (!found) {
    printf("  no I2C device found, check OLED wiring\n");
  }
}

static void oled_draw(void)
{
  char line[24];
  uint32_t start = HAL_GetTick();
  uint32_t up_s  = start / 1000u;

  u8g2_ClearBuffer(&u8g2);
  u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);

  snprintf(line, sizeof(line), "02 HAL+U8g2  B1:%s", b1_down ? "down" : "up");
  u8g2_DrawStr(&u8g2, 0, 9, line);
  u8g2_DrawHLine(&u8g2, 0, 11, 128);

  snprintf(line, sizeof(line), "short:%-4lu dbl :%-4lu", cnt_short, cnt_double);
  u8g2_DrawStr(&u8g2, 0, 22, line);
  snprintf(line, sizeof(line), "long :%-4lu EXTI:%-4lu", cnt_long, exti_edges);
  u8g2_DrawStr(&u8g2, 0, 32, line);
  snprintf(line, sizeof(line), "last : %s", button_event_name(last_evt));
  u8g2_DrawStr(&u8g2, 0, 42, line);
  snprintf(line, sizeof(line), "LD2  : %s", led_mode_name());
  u8g2_DrawStr(&u8g2, 0, 52, line);
  snprintf(line, sizeof(line), "up %02lu:%02lu:%02lu f:%lums",
           up_s / 3600u, (up_s / 60u) % 60u, up_s % 60u, frame_ms);
  u8g2_DrawStr(&u8g2, 0, 63, line);

  /* 右下角 8x8 方块和 LD2 同步：实心 = 亮，空心 = 灭 */
  if (led_is_on()) {
    u8g2_DrawBox(&u8g2, 120, 56, 8, 8);
  } else {
    u8g2_DrawFrame(&u8g2, 120, 56, 8, 8);
  }

  u8g2_SendBuffer(&u8g2);  /* 把 1 KB 显存通过 I2C 发给屏幕，这一步最花时间 */
  frame_ms = HAL_GetTick() - start;
}

static void handle_event(button_event_t evt)
{
  last_evt = evt;
  switch (evt) {
    case BUTTON_EVT_SHORT:
      cnt_short++;
      led_mode = led_is_on() ? LED_MODE_OFF : LED_MODE_ON;
      led_write_reg(led_mode == LED_MODE_ON);
      break;
    case BUTTON_EVT_DOUBLE:
      cnt_double++;
      led_mode = LED_MODE_BLINK;
      blink_last_ms = HAL_GetTick();
      break;
    case BUTTON_EVT_LONG:
      cnt_long++;
      cnt_short = 0;
      cnt_double = 0;
      exti_edges = 0;
      break;
    default:
      break;
  }
  printf("[%8lu ms] %-6s  short=%lu double=%lu long=%lu exti_edges=%lu  LD2=%s\n",
         HAL_GetTick(), button_event_name(evt), cnt_short, cnt_double, cnt_long,
         exti_edges, led_mode_name());
}

void app_init(void)
{
  setvbuf(stdout, NULL, _IONBF, 0);  /* printf 不缓存，调用就立刻发出去 */
  led_init_reg();

  /* 上电时按键是松开的，读一次当作“没按”的电平，这样不用关心按键是高电平还是低电平有效 */
  button_init(&btn, HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_SET);

  printf("\n=== 02-hal-button-u8g2: NUCLEO-G474RE (STM32 HAL) ===\n");
  printf("SYSCLK = %lu MHz\n", HAL_RCC_GetSysClockFreq() / 1000000u);
  i2c_scan();

  oled_ok = (HAL_I2C_IsDeviceReady(&hi2c1, OLED_I2C_ADDR << 1, 3, 10) == HAL_OK);
  if (oled_ok) {
    oled_init(&u8g2);
    printf("OLED ok (0x%02X)\n", OLED_I2C_ADDR);
  } else {
    printf("OLED NOT FOUND at 0x%02X, running without display\n", OLED_I2C_ADDR);
  }
  printf("B1: click = toggle LD2, double-click = blink, long press 0.8 s = clear counters\n");

  blink_last_ms = HAL_GetTick();
  if (HAL_TIM_Base_Start_IT(&htim6) != HAL_OK) {  /* 开始每 10 ms 扫描按键 */
    Error_Handler();
  }
}

void app_loop(void)
{
  static uint32_t last_draw_s = 0xFFFFFFFFu;
  static uint8_t  last_led = 0xFFu;
  static uint8_t  last_b1 = 0xFFu;
  uint8_t dirty = 0;

  button_event_t evt;
  while ((evt = evt_pop()) != BUTTON_EVT_NONE) {
    handle_event(evt);
    dirty = 1;
  }

  if (led_mode == LED_MODE_BLINK && HAL_GetTick() - blink_last_ms >= BLINK_HALF_PERIOD_MS) {
    blink_last_ms += BLINK_HALF_PERIOD_MS;
    led_write_reg(!led_is_on());
  }

  /* 只在内容变了的时候才刷新 OLED，不然一直刷会白白占用 CPU 和 I2C 总线 */
  uint8_t led_now = led_is_on();
  uint8_t b1_now  = b1_down;
  uint32_t now_s  = HAL_GetTick() / 1000u;
  if (led_now != last_led || b1_now != last_b1 || now_s != last_draw_s) {
    dirty = 1;
  }

  if (dirty && oled_ok) {
    last_led    = led_now;
    last_b1     = b1_now;
    last_draw_s = now_s;
    oled_draw();
  }
}
