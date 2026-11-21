#!/usr/bin/env python3
"""Thu dữ liệu SVM từ Node 1 và vẽ đồ thị (minh chứng cho việc đọc MPU6050).

Cách dùng:
  1. Trong include/config.h đặt SERIAL_OUTPUT_MODE 1, nạp firmware.
  2. pip install pyserial matplotlib
  3. Thu từ cổng serial (đứng yên, rồi đi bộ), 20 giây:
        python tools/plot_svm.py --port COM5 --seconds 20 --out svm_walk.csv
     (Linux: /dev/ttyACM0)
  4. Hoặc vẽ lại từ file CSV đã lưu:
        python tools/plot_svm.py --csv svm_walk.csv --png svm_walk.png

Định dạng dòng dữ liệu từ firmware: ms,ax,ay,az,svm (đơn vị g). Các dòng khác (log) bị bỏ qua.
"""
import argparse
import csv
import re
import sys
import time

LINE = re.compile(r"^\d+,-?\d+\.\d+,-?\d+\.\d+,-?\d+\.\d+,-?\d+\.\d+$")


def capture(port, baud, seconds, out):
    import serial  # pip install pyserial
    rows = []
    with serial.Serial(port, baud, timeout=1) as ser:
        end = time.time() + seconds
        while time.time() < end:
            line = ser.readline().decode("utf-8", errors="ignore").strip()
            if LINE.match(line):
                rows.append([float(x) for x in line.split(",")])
    if out:
        with open(out, "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["ms", "ax", "ay", "az", "svm"])
            w.writerows(rows)
        print(f"Da luu {len(rows)} mau vao {out}")
    return rows


def load(path):
    with open(path) as f:
        r = csv.reader(f)
        next(r, None)  # bỏ header
        return [[float(x) for x in row] for row in r if row]


def plot(rows, png):
    import matplotlib.pyplot as plt  # pip install matplotlib
    if not rows:
        sys.exit("Khong co du lieu de ve (kiem tra SERIAL_OUTPUT_MODE=1 va cong COM).")
    t0 = rows[0][0]
    t = [(r[0] - t0) / 1000.0 for r in rows]
    svm = [r[4] for r in rows]
    rate = (len(rows) - 1) / max(t[-1], 1e-6)
    plt.figure(figsize=(10, 4))
    plt.plot(t, svm, lw=1)
    plt.axhline(1.0, color="gray", ls=":", label="1 g")
    plt.xlabel("Thoi gian (s)")
    plt.ylabel("SVM (g)")
    plt.title(f"SVM tu MPU6050 (~{rate:.0f} Hz, {len(rows)} mau)")
    plt.legend()
    plt.grid(alpha=0.3)
    plt.tight_layout()
    if png:
        plt.savefig(png, dpi=150)
        print(f"Da luu anh {png}")
    plt.show()


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--port")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--seconds", type=float, default=20)
    ap.add_argument("--out", help="luu CSV khi thu tu serial")
    ap.add_argument("--csv", help="doc CSV co san thay vi serial")
    ap.add_argument("--png", help="luu anh do thi")
    a = ap.parse_args()
    if a.csv:
        rows = load(a.csv)
    elif a.port:
        rows = capture(a.port, a.baud, a.seconds, a.out)
    else:
        ap.error("can --port hoac --csv")
    plot(rows, a.png)
