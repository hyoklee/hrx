#!/usr/bin/env python3
"""Plot h5s3 vs dmrpp get.dap performance from H5S3_GETDAP_FINDINGS.md."""

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

# Data from H5S3_GETDAP_FINDINGS.md (full-file get.dap -> fileout-netcdf, curl end-to-end)
datasets = ["Original\n(§4)", "Repack\n(§4b)"]
h5s3_time = [3097.5, 2774.3]          # seconds
dmrpp_time = [491.6, 127.2]           # seconds
h5s3_rate = [160, 179]               # KB/s
dmrpp_rate = [808, 3200]             # KB/s
speedups = [h / d for h, d in zip(h5s3_time, dmrpp_time)]  # ~6.3x, ~21.8x

C_H5S3 = "#d1495b"   # h5s3
C_DMRPP = "#00798c"  # dmrpp

fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(11, 5.2))
fig.suptitle(
    "h5s3 (ROS3, live S3) vs dmrpp (local sidecar) — full-file get.dap\n"
    "MERRA2_200.tavg1_2d_slv_Nx.19970918  (47 vars, 24×361×576)",
    fontsize=12, fontweight="bold",
)

x = range(len(datasets))
w = 0.36

# --- Panel 1: total time (minutes) ---
h5s3_min = [t / 60 for t in h5s3_time]
dmrpp_min = [t / 60 for t in dmrpp_time]
b1 = ax1.bar([i - w / 2 for i in x], h5s3_min, w, label="h5s3", color=C_H5S3)
b2 = ax1.bar([i + w / 2 for i in x], dmrpp_min, w, label="dmrpp", color=C_DMRPP)
ax1.set_ylabel("time_total (minutes)")
ax1.set_title("End-to-end build time (lower is better)")
ax1.set_xticks(list(x))
ax1.set_xticklabels(datasets)
ax1.legend()
ax1.grid(axis="y", alpha=0.3)
ax1.set_ylim(0, max(h5s3_min) * 1.22)
for b in b1:
    ax1.text(b.get_x() + b.get_width() / 2, b.get_height(), f"{b.get_height():.1f}",
             ha="center", va="bottom", fontsize=9)
for b in b2:
    ax1.text(b.get_x() + b.get_width() / 2, b.get_height(), f"{b.get_height():.1f}",
             ha="center", va="bottom", fontsize=9)
# speedup annotations (centered between the two bars, below the bar tops)
for i, s in enumerate(speedups):
    ax1.text(i, h5s3_min[i] * 0.55,
             f"{s:.1f}×\nslower", ha="center", va="center",
             fontsize=9, fontweight="bold", color="white",
             bbox=dict(boxstyle="round,pad=0.3", fc=C_H5S3, ec="none", alpha=0.85))

# --- Panel 2: transfer rate (KB/s, log scale) ---
b3 = ax2.bar([i - w / 2 for i in x], h5s3_rate, w, label="h5s3", color=C_H5S3)
b4 = ax2.bar([i + w / 2 for i in x], dmrpp_rate, w, label="dmrpp", color=C_DMRPP)
ax2.set_ylabel("avg transfer rate (KB/s)")
ax2.set_title("Effective throughput (higher is better)")
ax2.set_yscale("log")
ax2.set_xticks(list(x))
ax2.set_xticklabels(datasets)
ax2.legend()
ax2.grid(axis="y", alpha=0.3, which="both")
for b in list(b3) + list(b4):
    ax2.text(b.get_x() + b.get_width() / 2, b.get_height(), f"{int(b.get_height())}",
             ha="center", va="bottom", fontsize=9)

fig.tight_layout(rect=(0, 0, 1, 0.93))
out = "h5s3_getdap_perf.png"
fig.savefig(out, dpi=150)
print("wrote", out)
