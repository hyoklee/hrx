#!/usr/bin/env python3
"""h5s3 (ROS3 live) vs dmrpp subset performance, same Synology C2 backend.

Measured 2026-07-02 on host `matlab` against the running Hyrax server.
Subsets of /T2M in MERRA2_200.tavg1_2d_slv_Nx.19970918.nc4, get.dap ->
fileout-netcdf, end-to-end curl. Both paths read the SAME object on Synology C2
(h5s3 via ROS3 VFD; dmrpp via byte-range GETs from the .c2.dmrpp sidecar).
Output data verified identical (np.allclose) for every subset.
"""

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Patch

# subset, element count, h5s3 avg s, dmrpp avg s  (warm; whole-T2M is single-shot)
labels = ["small\n1×100×100\n(1e4 pts)",
          "medium\n1×361×576\n(2.1e5 pts)",
          "whole T2M\n24×361×576\n(5.0e6 pts)"]
h5s3 = [0.736, 2.716, 59.8]
dmrpp = [0.427, 0.484, 7.6]
ratio = [h / d for h, d in zip(h5s3, dmrpp)]   # 1.7x, 5.6x, 7.9x

C_H5S3 = "#d1495b"
C_DMRPP = "#00798c"
C_RATIO = "#8a2740"

fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5.4),
                               gridspec_kw={"width_ratios": [1.55, 1]})
fig.suptitle(
    "h5s3 (ROS3 live) vs dmrpp — subset get.dap, same Synology C2 backend\n"
    "/T2M in MERRA2_200.tavg1_2d_slv_Nx.19970918.nc4  (data verified identical)",
    fontsize=12.5, fontweight="bold",
)

# ---------- Panel 1: grouped bars, log time ----------
x = range(len(labels))
w = 0.38
b1 = ax1.bar([i - w / 2 for i in x], h5s3, w, label="h5s3 (ROS3 live)",
             color=C_H5S3, zorder=3)
b2 = ax1.bar([i + w / 2 for i in x], dmrpp, w, label="dmrpp (C2 sidecar)",
             color=C_DMRPP, zorder=3)
ax1.set_yscale("log")
ax1.set_ylabel("end-to-end time (s, log scale)")
ax1.set_title("Subset download time (lower is better)", fontsize=11)
ax1.set_xticks(list(x))
ax1.set_xticklabels(labels, fontsize=9)
ax1.set_ylim(0.2, 120)
ax1.grid(axis="y", alpha=0.3, which="both", zorder=0)
ax1.legend(loc="upper left", fontsize=9, framealpha=0.9)
for b in list(b1) + list(b2):
    v = b.get_height()
    ax1.text(b.get_x() + b.get_width() / 2, v * 1.06,
             f"{v:.2f}s" if v < 10 else f"{v:.0f}s",
             ha="center", va="bottom", fontsize=8.5, fontweight="bold")
# ratio callouts above each group
for i, r in enumerate(ratio):
    ax1.text(i, max(h5s3[i], dmrpp[i]) * 2.4, f"{r:.1f}×",
             ha="center", va="bottom", fontsize=10, fontweight="bold",
             color=C_RATIO)
ax1.text(0.5, 90, "h5s3 slower ↑", ha="center", fontsize=8.5,
         color=C_RATIO, style="italic")

# ---------- Panel 2: slowdown ratio vs subset size ----------
pts = [1e4, 2.1e5, 5.0e6]
ax2.plot(pts, ratio, "-o", color=C_RATIO, lw=2, ms=9, zorder=3)
ax2.set_xscale("log")
ax2.set_xlabel("subset size (elements, log)")
ax2.set_ylabel("h5s3 / dmrpp  (× slower)")
ax2.set_title("Gap widens with subset size", fontsize=11)
ax2.axhline(1.0, color="#888", ls=":", lw=1.2, zorder=1)
ax2.text(pts[-1], 1.0, " parity", va="bottom", ha="right", fontsize=8,
         color="#666")
ax2.set_ylim(0, max(ratio) * 1.25)
ax2.grid(alpha=0.3, which="both", zorder=0)
for px, r in zip(pts, ratio):
    ax2.annotate(f"{r:.1f}×", (px, r), textcoords="offset points",
                 xytext=(6, 8), fontsize=9, fontweight="bold", color=C_RATIO)

fig.tight_layout(rect=(0, 0, 1, 0.9))
out = "h5s3_dmrpp_subset_perf.png"
fig.savefig(out, dpi=150)
print("wrote", out)
