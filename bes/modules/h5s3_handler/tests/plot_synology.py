#!/usr/bin/env python3
"""Synology C2 performance: dmrpp_module vs h5s3_handler (ROS3).

Data measured against https://us-003.s3.synologyc2.net (bucket iowarp) over the
WAN. See PERFORMANCE.md "Real cloud endpoint" section.
"""
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

# ---- Panel A: metadata (DMR) latency per request, ms ----
files = ["nava.h5\n83 KB / 3 vars",
         "MERRA2_200\n407 MB / 50 vars",
         "MERRA2_400\n4.16 GB / 27 vars"]
ros3_dmr  = [631, 5084, 3048]   # h5s3_handler: live read via ROS3
dmrpp_dmr = [8, 8, 8]           # dmrpp_module: served from local .dmrpp sidecar

# ---- Panel B: data read of /CLDPRS (8.56 MB, 384 chunks), ms ----
read_labels = ["h5s3 ROS3\n(warm)", "h5s3 ROS3\n(cold)",
               "dmrpp\n1 conn", "dmrpp\n8 threads", "dmrpp\n16 threads"]
read_ms     = [18200, 26400, 80600, 22200, 15500]
read_colors = ["#1f77b4", "#5599cc", "#d62728", "#ff9933", "#2ca02c"]

fig, (axA, axB) = plt.subplots(1, 2, figsize=(13, 5.5))
fig.suptitle("Synology C2 (real cloud S3, WAN): dmrpp_module vs h5s3_handler",
             fontsize=14, fontweight="bold")

# Panel A — grouped bars, log scale
x = np.arange(len(files)); w = 0.38
bA1 = axA.bar(x - w/2, ros3_dmr,  w, label="h5s3_handler (ROS3, live)", color="#1f77b4")
bA2 = axA.bar(x + w/2, dmrpp_dmr, w, label="dmrpp_module (local sidecar)", color="#2ca02c")
axA.set_yscale("log")
axA.set_ylabel("DMR latency per request (ms, log scale)")
axA.set_title("(a) Metadata (DMR) — recurring per request")
axA.set_xticks(x); axA.set_xticklabels(files, fontsize=9)
axA.legend(loc="upper left", fontsize=9)
axA.grid(axis="y", which="both", ls=":", alpha=0.5)
for b in list(bA1) + list(bA2):
    axA.annotate(f"{int(b.get_height())}", (b.get_x()+b.get_width()/2, b.get_height()),
                 ha="center", va="bottom", fontsize=8)

# Panel B — data read bars
xb = np.arange(len(read_labels))
bB = axB.bar(xb, read_ms, color=read_colors)
axB.set_ylabel("Read time (ms)")
axB.set_title("(b) Data read: /CLDPRS  (8.56 MB, 384 chunks)")
axB.set_xticks(xb); axB.set_xticklabels(read_labels, fontsize=9)
axB.grid(axis="y", ls=":", alpha=0.5)
for b in bB:
    axB.annotate(f"{b.get_height()/1000:.1f} s", (b.get_x()+b.get_width()/2, b.get_height()),
                 ha="center", va="bottom", fontsize=8)

# footnote about dmrpp one-time build cost
fig.text(0.5, 0.005,
         "Note: dmrpp_module also pays a one-time ~50 s sidecar build per file "
         "(~40 s download + ~10 s process for MERRA2_200); h5s3_handler needs no preprocessing.",
         ha="center", fontsize=8, style="italic")

fig.tight_layout(rect=[0, 0.03, 1, 0.95])
out = __file__.rsplit("/", 1)[0] + "/synology_performance.png"
fig.savefig(out, dpi=130)
print("wrote", out)
