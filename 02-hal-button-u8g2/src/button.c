/*
 * 按键状态机
 *
 *            按下                         松开
 *   IDLE ───────────> PRESSED ──────────────────────> WAIT_SECOND
 *    ^                   │                              │      │
 *    │                   │ 按满 800 ms                  │      │ 250 ms 内又按下
 *    │                   │ → 报告 LONG                  │      │ → 报告 DOUBLE
 *    │                   v                              │      v
 *    │              WAIT_RELEASE <──────────────────────┼──────┘
 *    │                   │                              │
 *    └───────────────────┘ 松开                         │ 250 ms 内没再按
 *    ^                                                  │ → 报告 SHORT
 *    └──────────────────────────────────────────────────┘
 *
 * 状态机只看消抖后的电平，所以抖动不会被当成多次按下。
 */
#include "button.h"

enum {
  ST_IDLE = 0,      /* 没按 */
  ST_PRESSED,       /* 第一次按下，还没松开，也还没到长按 */
  ST_WAIT_SECOND,   /* 第一次松开了，等看看有没有第二次按下 */
  ST_WAIT_RELEASE,  /* 已经报告过 LONG 或 DOUBLE，等松开后回到 IDLE */
};

void button_init(button_t *btn, uint8_t idle_level)
{
  btn->idle_level   = idle_level ? 1u : 0u;
  btn->stable_level = btn->idle_level;
  btn->bounce_cnt   = 0;
  btn->state        = ST_IDLE;
  btn->ticks        = 0;
}

uint8_t button_is_down(const button_t *btn)
{
  return btn->stable_level != btn->idle_level;
}

/* 消抖：新电平必须连续出现 BUTTON_DEBOUNCE_TICKS 次才接受 */
static void debounce(button_t *btn, uint8_t raw_level)
{
  if ((raw_level ? 1u : 0u) == btn->stable_level) {
    btn->bounce_cnt = 0;
    return;
  }
  if (++btn->bounce_cnt >= BUTTON_DEBOUNCE_TICKS) {
    btn->stable_level = raw_level ? 1u : 0u;
    btn->bounce_cnt   = 0;
  }
}

button_event_t button_update(button_t *btn, uint8_t raw_level)
{
  debounce(btn, raw_level);
  uint8_t down = button_is_down(btn);

  if (btn->ticks < 0xFFFFu) {
    btn->ticks++;
  }

  switch (btn->state) {
    case ST_IDLE:
      if (down) {
        btn->state = ST_PRESSED;
        btn->ticks = 0;
      }
      break;

    case ST_PRESSED:
      if (!down) {
        btn->state = ST_WAIT_SECOND;
        btn->ticks = 0;
      } else if (btn->ticks >= BUTTON_LONG_TICKS) {
        btn->state = ST_WAIT_RELEASE;
        return BUTTON_EVT_LONG;
      }
      break;

    case ST_WAIT_SECOND:
      if (down) {
        btn->state = ST_WAIT_RELEASE;
        return BUTTON_EVT_DOUBLE;
      }
      if (btn->ticks >= BUTTON_DOUBLE_TICKS) {
        btn->state = ST_IDLE;
        return BUTTON_EVT_SHORT;
      }
      break;

    case ST_WAIT_RELEASE:
      if (!down) {
        btn->state = ST_IDLE;
      }
      break;

    default:
      btn->state = ST_IDLE;
      break;
  }
  return BUTTON_EVT_NONE;
}

const char *button_event_name(button_event_t evt)
{
  switch (evt) {
    case BUTTON_EVT_SHORT:  return "SHORT";
    case BUTTON_EVT_DOUBLE: return "DOUBLE";
    case BUTTON_EVT_LONG:   return "LONG";
    default:                return "NONE";
  }
}
