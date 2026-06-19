# h5s3_handler sample configs and indexes

Example `h5s3.conf` configurations and cached `index.parquet` files for the two
endpoints this module was tested against.

## Conf samples

Install the chosen one as `{prefix}/etc/bes/modules/h5s3.conf` (the BES includes
`modules/*.conf`).

| file | endpoint | bucket | notes |
|------|----------|--------|-------|
| `h5s3.conf.localstack` | `http://localhost:4566` | `h5s3` | localstack; dummy `test`/`test` credentials work as-is |
| `h5s3.conf.synology` | `https://us-003.s3.synologyc2.net` | `iowarp` | Synology C2; **replace** `YOUR_SYNOLOGY_ACCESS_KEY_ID` / `YOUR_SYNOLOGY_SECRET_KEY` with real credentials |

Both set `H5S3.ForcePathStyle = true` (required by localstack and Synology C2)
and a catalog `TypeMatch` covering `h5|he5|HDF5|nc4`.

> The Synology sample has its credentials redacted to placeholders — never commit
> real keys.

## Index samples

`index.parquet` is the cached file listing the browse catalog reads (columns:
`key`, `size`, `last_modified`). Install as
`{prefix}/share/hyrax/data/h5s3/index.parquet`.

| file | from bucket | entries |
|------|-------------|---------|
| `index.localstack.parquet` | `h5s3` (localstack) | 12 small HDF5 samples |
| `index.synology.parquet` | `iowarp` (Synology C2) | 6 files incl. large MERRA-2 `.nc4` (one ~4 GB) |

Regenerate an index for a bucket with the helper executables:

```bash
H5S3_BUCKET=<bucket> H5S3_ENDPOINT=<endpoint> \
AWS_ACCESS_KEY_ID=... AWS_SECRET_ACCESS_KEY=... AWS_DEFAULT_REGION=us-east-1 \
  h5s3_list | h5s3_index write index.parquet
```
