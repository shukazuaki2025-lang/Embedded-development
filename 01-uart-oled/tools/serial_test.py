#!/usr/bin/env python3
"""
电脑端串口自动测试脚本：检查电脑 <-> NUCLEO-G474RE 的串口通信是否正常。

用法：
    pip install pyserial
    python serial_test.py --list            # 列出电脑上的串口
    python serial_test.py COM5              # Windows
    python serial_test.py /dev/ttyACM0      # Linux
    python serial_test.py /dev/cu.usbmodem* # macOS
    python serial_test.py COM5 --chat       # 测试后进入手动聊天模式
"""
import argparse
import sys
import time

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    sys.exit("缺少 pyserial，请先运行：pip install pyserial")


def read_lines(ser, timeout):
    """读取 timeout 秒内收到的所有行。"""
    lines = []
    end = time.time() + timeout
    while time.time() < end:
        raw = ser.readline()
        if raw:
            line = raw.decode("ascii", errors="replace").rstrip("\r\n")
            print(f"  <- {line}")
            lines.append(line)
    return lines


def check(ser, send, expect, timeout=1.0):
    print(f"  -> {send}")
    ser.write((send + "\n").encode("ascii"))
    lines = read_lines(ser, timeout)
    ok = any(expect in line for line in lines)
    print(f"  [{'PASS' if ok else 'FAIL'}] 期望收到包含 '{expect}' 的回复\n")
    return ok


def chat(ser):
    print("进入聊天模式：输入文字回车发送，Ctrl+C 退出。")
    try:
        while True:
            text = input("> ")
            ser.write((text + "\n").encode("ascii", errors="replace"))
            read_lines(ser, 0.5)
    except (KeyboardInterrupt, EOFError):
        print()


def main():
    parser = argparse.ArgumentParser(description="NUCLEO-G474RE 串口通信测试")
    parser.add_argument("port", nargs="?", help="串口名，例如 COM5 或 /dev/ttyACM0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--list", action="store_true", help="列出可用串口")
    parser.add_argument("--chat", action="store_true", help="测试后进入手动聊天模式")
    args = parser.parse_args()

    if args.list or not args.port:
        ports = list(serial.tools.list_ports.comports())
        if not ports:
            print("没有找到串口。检查 USB 线是否插好、ST-LINK 驱动是否安装。")
        for p in ports:
            hint = "  <- 很可能是 Nucleo" if "STLink" in (p.description or "") or "STM" in (p.description or "") else ""
            print(f"{p.device}\t{p.description}{hint}")
        return

    with serial.Serial(args.port, args.baud, timeout=0.1) as ser:
        print(f"已打开 {args.port} @ {args.baud}\n")
        time.sleep(0.3)
        ser.reset_input_buffer()

        tests = [
            ("ping", "pong"),
            ("hello nucleo 123", "ECHO: hello nucleo 123"),
            ("info", "board : NUCLEO-G474RE"),
            ("led on", "OK led on"),
            ("led blink", "OK led blink"),
        ]
        passed = sum(check(ser, s, e) for s, e in tests)
        print(f"结果：{passed}/{len(tests)} 通过")
        if passed == len(tests):
            print("串口通信正常！OLED 上应该能看到刚才发送的消息。")
        else:
            print("有测试失败：检查串口号、波特率 (115200)，或按一下板子上的黑色复位键后重试。")

        if args.chat:
            chat(ser)
        sys.exit(0 if passed == len(tests) else 1)


if __name__ == "__main__":
    main()
