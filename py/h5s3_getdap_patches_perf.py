#!/usr/bin/env python3
"""Plot h5s3 get.dap performance AFTER the A1/A2/B1/B3 patches.

Data from H5S3_GETDAP_FINDINGS.md §8 (optimization experiments), original
granule MERRA2_200.tavg1_2d_slv_Nx.19970918.nc4, get.dap -> fileout-netcdf,
curl end-to-end.
"""

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Patch

C_H5S3 = "#d1495b"    # h5s3 (ROS3, live S3)
C_BEST = "#8a2740"    # best h5s3 config (all patches) — darker step of same hue
C_DMRPP = "#00798c"   # dmrpp reference

# --- Panel 1: full-file build time across optimization steps (§8) ---
steps = [
    "baseline\n(as-shipped)",
    "A1\nshared handle",
    "A1+B1\n+read_vector",
    "A1+A2+B1+B3\n(all patches)",
]
times = [3097.5, 2638.4, 2666.9, 2589.5]        # seconds
deltas = [0.0, -14.8, -13.9, -16.4]             # % vs baseline
dmrpp_ref = 491.6                               # dmrpp reference (s)

# --- Panel 2: A2 constraint pushdown on a subset request (§8) ---
subset_labels = ["before A2\n(H5S_ALL)", "after A2\n(hyperslab)"]
subset_times = [57.6, 3.76]                     # seconds
subset_speedup = subset_times[0] / subset_times[1]  # ~15x

fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5.4),
                               gridspec_kw={"width_ratios": [1.7, 1]})
fig.suptitle(
    "h5s3 get.dap performance after patches (A1 / A2 / B1 / B3)\n"
    "MERRA2_200.tavg1_2d_slv_Nx.19970918.nc4  —  ROS3, live from S3",
    fontsize=13, fontweight="bold",
)

# ---------- Panel 1: whole-file build time ----------
x = range(len(steps))
colors = [C_H5S3, C_H5S3, C_H5S3, C_BEST]
bars = ax1.bar(x, [t / 60 for t in times], 0.62, color=colors, zorder=3)

# dmrpp reference line
ax1.axhline(dmrpp_ref / 60, color=C_DMRPP, lw=2, ls="--", zorder=2)
ax1.text(len(steps) - 0.5, dmrpp_ref / 60 + 0.7,
         f"dmrpp reference  {dmrpp_ref/60:.1f} min",
         ha="right", va="bottom", fontsize=9, color=C_DMRPP, fontweight="bold")

ax1.set_ylabel("full-file build time (minutes)")
ax1.set_title("Whole-file .dap.nc4 download (lower is better)", fontsize=11)
ax1.set_xticks(list(x))
ax1.set_xticklabels(steps, fontsize=9)
ax1.set_ylim(0, max(times) / 60 * 1.16)
ax1.grid(axis="y", alpha=0.3, zorder=0)

for b, t, d in zip(bars, times, deltas):
    ax1.text(b.get_x() + b.get_width() / 2, b.get_height(),
             f"{t/60:.1f} min", ha="center", va="bottom", fontsize=9,
             fontweight="bold")
    if d != 0.0:
        ax1.text(b.get_x() + b.get_width() / 2, b.get_height() / 2,
                 f"{d:+.1f}%", ha="center", va="center", fontsize=10,
                 fontweight="bold", color="white")

ax1.legend(handles=[
    Patch(color=C_H5S3, label="h5s3 (patched steps)"),
    Patch(color=C_BEST, label="h5s3 all patches (best)"),
    plt.Line2D([], [], color=C_DMRPP, ls="--", lw=2, label="dmrpp reference"),
], loc="upper right", fontsize=8.5, framealpha=0.9)

# ---------- Panel 2: subset request (A2 constraint pushdown) ----------
xs = range(len(subset_labels))
sbars = ax2.bar(xs, subset_times, 0.55, color=[C_H5S3, C_BEST], zorder=3)
ax2.set_ylabel("subset request time (seconds)")
ax2.set_title("Subset  T2M[0][100:199][100:199]\n(higher is worse)", fontsize=11)
ax2.set_xticks(list(xs))
ax2.set_xticklabels(subset_labels, fontsize=9)
ax2.set_ylim(0, max(subset_times) * 1.2)
ax2.grid(axis="y", alpha=0.3, zorder=0)
for b, t in zip(sbars, subset_times):
    ax2.text(b.get_x() + b.get_width() / 2, b.get_height(),
             f"{t:.2f} s", ha="center", va="bottom", fontsize=9.5,
             fontweight="bold")

# speedup annotation
ax2.annotate("", xy=(1, subset_times[1] + 4), xytext=(0, subset_times[0] * 0.55),
             arrowprops=dict(arrowstyle="->", color=C_BEST, lw=2))
ax2.text(0.5, subset_times[0] * 0.62,
         f"~{subset_speedup:.0f}× faster\n+ fixes wrong-data bug",
         ha="center", va="center", fontsize=9.5, fontweight="bold",
         color="white",
         bbox=dict(boxstyle="round,pad=0.35", fc=C_BEST, ec="none", alpha=0.9))

fig.tight_layout(rect=(0, 0, 1, 0.92))
out = "h5s3_getdap_patches_perf.png"
fig.savefig(out, dpi=150)
print("wrote", out)
