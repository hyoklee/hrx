# S5P CO subsetting failure ("NetCDF: HDF error") — investigation report

Date: 2026-10-05
Forum thread: https://forum.earthdata.nasa.gov/viewtopic.php?t=8273
Sample file: `build/share/hyrax/data/hdf5/S5P_RPRO_L2__CO_____20180430T001950_20180430T020120_02818_03_020400_20220901T170054.nc.h5`

## User report

```
.../S5P_RPRO_L2__CO_____...nc.dap.nc4?dap4.ce=/PRODUCT/carbonmonoxide_total_column_corrected[0:1:0][0:1:2905][0:1:214]
```

The download succeeds, but `ncview`/`ncdump` fails with `NetCDF: HDF error`.
- `[0:1:2904][0:1:214]` or `[0:1:2905][0:1:213]` works.
- An HCHO granule requested the same way works.
- Other S5P variables (aerosol index, aerosol layer height, ...) fail the same way.

The array size (624,790 values) is not what matters. The failure happens only when the
constraint asks for the **whole variable**.

## Source variable layout

```
/PRODUCT/carbonmonoxide_total_column_corrected  Float32 (1, 2906, 215)
CHUNKED (1, 2906, 215)          <- one chunk = the whole variable
FILTERS: SHUFFLE -> DEFLATE(3) -> FLETCHER32
```

All 61 chunked variables in the file use the same chain
(`compressionType="shuffle deflate fletcher32"` in the DMR++).

## Reproduction (local, besstandalone + DMR++, same path as Earthdata Cloud OPeNDAP)

1. Make the DMR++: `get_dmrpp -b <dir> -u file://<dir>/s5p.h5 s5p.h5 > s5p.h5.dmrpp`
2. Send a `get dap` request with `returnAs="netcdf-4"` on the `.dmrpp` container.

| Request | DMRPP direct IO | Result |
|---|---|---|
| `[0:1:0][0:1:2905][0:1:214]` (whole var) | on (default) | **FAIL**: `ncdump` → `NetCDF: HDF error`; h5py → `filter returned failure during read` |
| `[0:1:0][0:1:2904][0:1:214]` (subset) | on | OK |
| `[0:1:0][0:1:2905][0:1:214]` (whole var) | `DMRPP.DisableDirectIO=true` | OK, and values are bit-identical to the source |

This matches the user's report exactly.

## Root cause

When the whole variable is requested, FONc uses **direct chunk IO**. It copies the
already-compressed chunk bytes from the source unchanged and sets the source's filter chain
on the output variable (`FONcArray::define_dio_filters` /
`allocate_dio_nc4_def_filters` in `bes/modules/fileout_netcdf/FONcArray.cc`).

For `shuffle deflate fletcher32`, FONc correctly finds that fletcher32 comes *last*. It calls
`nc_def_var_deflate(shuffle=1)` first and then `nc_def_var_fletcher32()`. However,
netCDF-C (4.9.3, `libhdf5/hdf5filter.c`, `NC4_hdf5_addfilter`) always moves Fletcher32 to
position 0:

```c
if(id == H5Z_FILTER_FLETCHER32)
    pos = 0; /* alway first filter */
```

So the output file ends up with this pipeline:

```
source chunk encoded with : SHUFFLE -> DEFLATE -> FLETCHER32
output declares           : FLETCHER32 -> SHUFFLE -> DEFLATE   (h5dump -p of the response)
```

When reading, HDF5 runs the declared pipeline in reverse. It tries to inflate a buffer that
still ends in the 4-byte Fletcher32 checksum, so the filter fails and you get `NetCDF: HDF error`.

Why the user's other observations fit:
- A subset (one fewer row or column) is not the whole variable, so direct IO is skipped.
  FONc decodes and re-encodes with plain deflate, and the file is valid.
- The HCHO granule works because its variables apparently don't use a trailing Fletcher32,
  or the request doesn't go through direct IO. This is inferred; the HCHO file was not tested.
- Aerosol index, layer height, etc. fail because every S5P L2 variable uses the same
  `shuffle deflate fletcher32` chain.

netCDF-C cannot express "Fletcher32 after deflate" through its API. Any variable whose
source chain doesn't match netCDF-C's fixed order (`[fletcher32] [shuffle] deflate...`)
therefore must not use direct IO.

## Workarounds (no code change)

- Server: set `DMRPP.DisableDirectIO=true`. This turns off direct IO everywhere and costs
  performance on other collections.
- Per-variable: add `DIO="off"` to the `dmrpp:chunks` element in the DMR++
  (`DMZ::set_up_direct_io_flag_phase_2` honors it).
- User side: request any subset that isn't the whole variable, or use `.dap.nc4` with a
  stride/range that excludes one element. Not recommended; it loses data.

## Proposed fix

In `DMZ::set_up_direct_io_flag_phase_2` (`bes/modules/dmrpp_module/DMZ.cc`), turn off direct
IO when the `compressionType` order can't be reproduced by netCDF-C. That is, allow it only
when the chain matches `^(fletcher32 )?(shuffle )?deflate( deflate)?$`. In particular, skip it
if `fletcher32` appears anywhere other than first, or if `shuffle` comes after `deflate`.

A second check could go in `FONcArray::obtain_dio_filters_order`: throw, or fall back to the
non-DIO path, when `has_fle_last` is true, instead of quietly writing an unreadable file.

A regression test should use this granule's DMR++ (or a small HDF5 file with a
`shuffle+deflate+fletcher32` single-chunk dataset). It should request the whole variable as
netCDF-4 and confirm the output reads back.

## Notes on the local environment

- In the current `build/etc/bes` configuration, `.h5` files are served by `h5s3_handler`
  (`H5Fopen failed for s3://iowarp/s5p.h5`), so the plain HDF5-handler path was not tested.
  The DMR++ path is the one Earthdata Cloud OPeNDAP uses.
- netCDF-C version: 4.9.3. BES HEAD: `564aa583a`.

## Fix implemented (2026-10-05)

`bes/modules/dmrpp_module/DMZ.{h,cc}`:
- New `static bool DMZ::is_dio_filter_order_supported(const std::string &filters)`. It accepts
  only the orders netCDF-C can reproduce: `[fletcher32] [shuffle] deflate [deflate]`.
- `DMZ::set_up_direct_io_flag_phase_2()` calls it after reading `compressionType`. When it
  returns false, the variable doesn't get the direct IO flag, so FONc decodes and re-encodes it.
- Unit test `DMZTest::test_is_dio_filter_order_supported`. `DMZTest` passes (44 tests).

Verification with besstandalone and the rebuilt `libdmrpp_module.so`, direct IO left enabled:

| Input | Output layout | Result |
|---|---|---|
| S5P CO, whole `carbonmonoxide_total_column_corrected` | re-encoded (DEFLATE 4, chunk 1×1024×215) | readable, bit-identical to source |
| test file, `shuffle deflate` | direct IO kept (chunk 100×50, SHUFFLE+DEFLATE 5) | identical to source |
| test file, `shuffle deflate fletcher32` (h5py's default order) | re-encoded | identical to source |

Note: h5py (and so many Python-written HDF5 files) puts Fletcher32 last by default, so
the bug probably affects more than S5P.
