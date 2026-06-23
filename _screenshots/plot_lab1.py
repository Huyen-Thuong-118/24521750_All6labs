import pandas as pd
import matplotlib.pyplot as plt

csv = r"_screenshots\lab1_bench_windows.csv"

df = pd.read_csv(csv, comment="#", encoding="utf-16")

mode_col = "mode"
size_col = "size_bytes"
thr_col = "throughput_mb_s"

df = df[df[mode_col].isin(["ECB", "CBC", "CTR", "GCM"])]

for mode in df[mode_col].unique():
    sub = df[df[mode_col] == mode].sort_values(size_col)
    plt.plot(sub[size_col], sub[thr_col], marker="o", label=str(mode))

plt.xlabel("Payload size (bytes)")
plt.ylabel("Throughput (MB/s)")
plt.title("Lab 1 AES-256 throughput by mode")
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.savefig(r"_screenshots\fig06_lab1_throughput_chart.png", dpi=200)
