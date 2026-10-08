/*
 * 按键状态机：消抖 + 识别短按 / 双击 / 长按
 *
 * 这个模块不碰任何硬件：调用者每 BUTTON_TICK_MS 毫秒读一次引脚电平，传给 button_update()。
 * 所以它可以直接在电脑上编译测试，也可以拿去给任何单片机、任何引脚用。
 */
#ifndef BUTTON_H
#define BUTTON_H

#include <stdint.h>

#define BUTTON_TICK_MS        10u   /* button_update() 的调用周期 */
#define BUTTON_DEBOUNCE_TICKS 2u    /* 电平连续 2 次（20 ms）一致才认为真的变了 */
#define BUTTON_LONG_TICKS     80u   /* 按住 800 ms 算长按 */
#define BUTTON_DOUBLE_TICKS   25u   /* 松开后 250 ms 内再按一次算双击 */

typedef enum {
  BUTTON_EVT_NONE = 0,
  BUTTON_EVT_SHORT,   /* 单击：松开后 250 ms 内没有第二次按下 */
  BUTTON_EVT_DOUBLE,  /* 双击：第二次按下的瞬间就报告 */
  BUTTON_EVT_LONG,    /* 长按：按住满 800 ms 的瞬间就报告，不用等松开 */
} button_event_t;

typedef struct {
  uint8_t  idle_level;    /* 没按时引脚的电平（0 或 1），上电时读一次 */
  uint8_t  stable_level;  /* 消抖后的电平 */
  uint8_t  bounce_cnt;    /* 和 stable_level 不同的连续采样次数 */
  uint8_t  state;         /* 状态机当前状态，取值见 button.c */
  uint16_t ticks;         /* 在当前状态里待了多少个 tick */
} button_t;

void button_init(button_t *btn, uint8_t idle_level);

/* 每 BUTTON_TICK_MS 调一次，raw_level 是这次读到的引脚电平。有事件就返回事件，否则返回 BUTTON_EVT_NONE */
button_event_t button_update(button_t *btn, uint8_t raw_level);

/* 消抖后按键是否处于按下状态 */
uint8_t button_is_down(const button_t *btn);

const char *button_event_name(button_event_t evt);

#endif /* BUTTON_H */
