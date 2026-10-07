/*
 * 第一个项目：电脑 <-> 开发板 串口通信测试 + OLED 显示
 *
 * 硬件：NUCLEO-G474RE + 0.96 寸 SSD1306 128x64 I2C OLED
 *
 * 功能：
 *   - 通过板载 ST-LINK 虚拟串口 (USB 线) 与电脑通信，115200 8N1
 *   - 电脑发来的每一行文字都会回显 (ECHO) 并显示在 OLED 上
 *   - 支持几条简单命令：ping / info / led on|off|blink|fast|slow / led blink <ms> / clear / help
 *   - OLED 显示：收发计数、运行时间、最近收到的 3 条消息
 *   - 按下板载蓝色按键 B1，板子会主动向电脑发送一条消息
 *
 * 接线 (OLED -> Nucleo Arduino 排针)：
 *   VCC -> 3V3    GND -> GND    SCL -> D15 (PB8)    SDA -> D14 (PB9)
 */
#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

// ---------------- 可修改的配置 ----------------
static const uint32_t SERIAL_BAUD   = 115200;
static const uint8_t  OLED_I2C_ADDR = 0x3C;   // 有些模块是 0x3D
static const uint8_t  LED_PIN       = LED_BUILTIN;  // LD2 绿灯 (PA5)
static const uint8_t  BUTTON_PIN    = USER_BTN;     // B1 蓝色按键 (PC13)

// 如果你的屏是 1.3 寸 SH1106，把下面这行换成：
// U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// ---------------- 状态 ----------------
static const size_t RX_LINE_MAX    = 64;   // 单行最大长度
static const size_t HISTORY_LEN = 3;    // OLED 上显示的历史条数
static const size_t SHOW_CHARS  = 21;   // 6x10 字体，128 像素一行最多 21 个字符

enum LedMode { LED_BLINK, LED_ON, LED_OFF };

static char     rxLine[RX_LINE_MAX + 1];
static size_t   rxLen = 0;
static bool     rxOverflow = false;
static char     history[HISTORY_LEN][RX_LINE_MAX + 1];
static uint32_t rxCount = 0;   // 收到的行数
static uint32_t txCount = 0;   // 发出的行数
static bool     oledOk = false;
static bool     oledDirty = true;
static LedMode  ledMode = LED_BLINK;
static uint32_t ledHalfMs = 100;   // 闪烁半周期：亮 100 ms + 灭 100 ms = 5 Hz
static int      buttonIdle;    // 上电时按键的电平，视为“未按下”
static int      buttonLast;

// ---------------- 工具函数 ----------------
static void sendLine(const char *s) {
  Serial.println(s);
  txCount++;
}

static void pushHistory(const char *s) {
  for (size_t i = HISTORY_LEN - 1; i > 0; i--) {
    strcpy(history[i], history[i - 1]);
  }
  strncpy(history[0], s, RX_LINE_MAX);
  history[0][RX_LINE_MAX] = '\0';
  oledDirty = true;
}

static void formatUptime(char *buf, size_t n) {
  uint32_t s = millis() / 1000;
  snprintf(buf, n, "%02lu:%02lu:%02lu",
           (unsigned long)(s / 3600), (unsigned long)(s / 60 % 60), (unsigned long)(s % 60));
}

static bool i2cDevicePresent(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

static void scanI2C() {
  char buf[48];
  int found = 0;
  sendLine("I2C scan:");
  for (uint8_t addr = 1; addr < 127; addr++) {
    if (i2cDevicePresent(addr)) {
      snprintf(buf, sizeof(buf), "  found device at 0x%02X", addr);
      sendLine(buf);
      found++;
    }
  }
  if (found == 0) {
    sendLine("  no I2C device found, check OLED wiring");
  }
}

// ---------------- OLED ----------------
static void drawOled() {
  if (!oledOk) return;
  char buf[32];
  char up[12];

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);

  u8g2.drawStr(0, 9, "UART <-> OLED 115200");

  formatUptime(up, sizeof(up));
  snprintf(buf, sizeof(buf), "RX%-4lu TX%-4lu%s",
           (unsigned long)(rxCount % 10000), (unsigned long)(txCount % 10000), up);
  u8g2.drawStr(0, 20, buf);

  u8g2.drawHLine(0, 23, 128);

  for (size_t i = 0; i < HISTORY_LEN; i++) {
    if (history[i][0] == '\0') continue;
    char shown[SHOW_CHARS + 1];
    // 第一条加 ">" 表示最新
    snprintf(shown, sizeof(shown), "%c%s", i == 0 ? '>' : ' ', history[i]);
    u8g2.drawStr(0, 35 + i * 10, shown);
  }
  if (rxCount == 0) {
    u8g2.drawStr(0, 35, "waiting for PC...");
  }

  if (ledMode == LED_BLINK) {
    snprintf(buf, sizeof(buf), "LED:blink %lums", (unsigned long)ledHalfMs);
    u8g2.drawStr(0, 63, buf);
  } else {
    u8g2.drawStr(0, 63, ledMode == LED_ON ? "LED:on" : "LED:off");
  }
  u8g2.sendBuffer();
  oledDirty = false;
}

// ---------------- 命令处理 ----------------
static void printInfo() {
  char buf[64];
  char up[12];
  formatUptime(up, sizeof(up));
  sendLine("board : NUCLEO-G474RE");
  snprintf(buf, sizeof(buf), "cpu   : %lu MHz", (unsigned long)(SystemCoreClock / 1000000));
  sendLine(buf);
  snprintf(buf, sizeof(buf), "uptime: %s", up);
  sendLine(buf);
  snprintf(buf, sizeof(buf), "rx/tx : %lu/%lu lines", (unsigned long)rxCount, (unsigned long)txCount);
  sendLine(buf);
  snprintf(buf, sizeof(buf), "oled  : %s (0x%02X)", oledOk ? "ok" : "NOT FOUND", OLED_I2C_ADDR);
  sendLine(buf);
}

static void printHelp() {
  sendLine("commands: ping | info | led on | led off | led blink | clear | help");
  sendLine("          led fast | led slow | led blink <ms>  (blink half-period 20-2000 ms)");
  sendLine("anything else is echoed back and shown on the OLED");
}

static void setBlink(uint32_t halfMs) {
  char buf[40];
  ledMode = LED_BLINK;
  ledHalfMs = halfMs;
  snprintf(buf, sizeof(buf), "OK led blink %lums", (unsigned long)halfMs);
  sendLine(buf);
}

static void handleLine(char *line) {
  // 去掉行尾的 \r（Windows 串口助手常发 \r\n）和首尾空格
  size_t n = strlen(line);
  while (n > 0 && (line[n - 1] == '\r' || line[n - 1] == ' ')) line[--n] = '\0';
  while (*line == ' ') line++;
  if (*line == '\0') return;

  // OLED 字库只有 ASCII，其他字节（比如中文）显示为 '?'
  for (char *p = line; *p; p++) {
    if ((uint8_t)*p < 0x20 || (uint8_t)*p > 0x7E) *p = '?';
  }

  rxCount++;
  pushHistory(line);

  if (strcmp(line, "ping") == 0) {
    sendLine("pong");
  } else if (strcmp(line, "info") == 0) {
    printInfo();
  } else if (strcmp(line, "help") == 0) {
    printHelp();
  } else if (strcmp(line, "led on") == 0) {
    ledMode = LED_ON;
    sendLine("OK led on");
  } else if (strcmp(line, "led off") == 0) {
    ledMode = LED_OFF;
    sendLine("OK led off");
  } else if (strcmp(line, "led blink") == 0) {
    setBlink(ledHalfMs);
  } else if (strcmp(line, "led fast") == 0) {
    setBlink(50);
  } else if (strcmp(line, "led slow") == 0) {
    setBlink(500);
  } else if (strncmp(line, "led blink ", 10) == 0) {
    char *end;
    long ms = strtol(line + 10, &end, 10);
    if (*end != '\0' || ms < 20 || ms > 2000) {
      sendLine("ERR usage: led blink <20-2000 ms>");
    } else {
      setBlink((uint32_t)ms);
    }
  } else if (strcmp(line, "clear") == 0) {
    for (size_t i = 0; i < HISTORY_LEN; i++) history[i][0] = '\0';
    rxCount = 0;
    txCount = 0;
    sendLine("OK cleared");
  } else {
    char buf[RX_LINE_MAX + 8];
    snprintf(buf, sizeof(buf), "ECHO: %s", line);
    sendLine(buf);
  }
  oledDirty = true;
}

static void pollSerial() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      // 同时兼容 \n、\r、\r\n 三种行尾
      if (rxOverflow) {
        sendLine("ERR line too long (max 64 chars)");
      } else if (rxLen > 0) {
        rxLine[rxLen] = '\0';
        handleLine(rxLine);
      }
      rxLen = 0;
      rxOverflow = false;
    } else if (rxLen < RX_LINE_MAX) {
      rxLine[rxLen++] = c;
    } else {
      rxOverflow = true;
    }
  }
}

static void pollButton() {
  static uint32_t lastChange = 0;
  int now = digitalRead(BUTTON_PIN);
  if (now != buttonLast && millis() - lastChange > 30) {  // 30 ms 消抖
    lastChange = millis();
    buttonLast = now;
    if (now != buttonIdle) {
      sendLine("BTN: B1 pressed (board -> PC test)");
      pushHistory("[B1 pressed]");
    }
  }
}

static void updateLed() {
  switch (ledMode) {
    case LED_ON:  digitalWrite(LED_PIN, HIGH); break;
    case LED_OFF: digitalWrite(LED_PIN, LOW);  break;
    case LED_BLINK:
      digitalWrite(LED_PIN, (millis() / ledHalfMs) % 2 ? HIGH : LOW);
      break;
  }
}

// ---------------- 主程序 ----------------
void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);
  buttonIdle = buttonLast = digitalRead(BUTTON_PIN);

  Serial.begin(SERIAL_BAUD);
  Wire.begin();  // 默认 SDA = D14 (PB9)，SCL = D15 (PB8)

  delay(100);  // 等 OLED 上电稳定
  sendLine("");
  sendLine("=== 01-uart-oled: NUCLEO-G474RE ready ===");
  scanI2C();

  oledOk = i2cDevicePresent(OLED_I2C_ADDR);
  if (oledOk) {
    u8g2.setI2CAddress(OLED_I2C_ADDR << 1);  // U8g2 使用 8 位地址
    u8g2.begin();
    sendLine("OLED ok");
  } else {
    sendLine("OLED not found, running serial-only");
  }
  printHelp();
  drawOled();
}

void loop() {
  static uint32_t lastDraw = 0;

  pollSerial();
  pollButton();
  updateLed();

  // 有新内容立即刷新，否则每秒刷新一次运行时间
  if (oledDirty || millis() - lastDraw >= 1000) {
    lastDraw = millis();
    drawOled();
  }
}
