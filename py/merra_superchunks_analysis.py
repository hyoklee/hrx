"""
Analyze MERRA2 .dmrpp files to count superchunks per variable (all types).

Superchunk definition: consecutive chunks (sorted by offset) are merged into
one superchunk when offset2 == offset1 + nBytes1.
"""

import xml.etree.ElementTree as ET

NS = {
    'dap': 'http://xml.opendap.org/ns/DAP/4.0#',
    'dmrpp': 'http://xml.opendap.org/dap/dmrpp/1.0.0#',
}

# DAP4 numeric types that may contain dmrpp:chunks
VAR_TYPES = ['Byte', 'Int8', 'UInt8', 'Int16', 'UInt16', 'Int32', 'UInt32',
             'Int64', 'UInt64', 'Float32', 'Float64']

FILES = [
    'MERRA2_200.tavg1_2d_slv_Nx.19970918.nc4.dmrpp',
    'MERRA2_200.tavg1_2d_slv_Nx.19970918.nc4.dmrpp.fonc.nc4.dmrpp',
    'MERRA2_200.tavg1_2d_slv_Nx.19970918.repack.nc4.dmrpp',
]


def count_superchunks(chunks):
    """Return number of superchunks in a list of (offset, nBytes) tuples."""
    if not chunks:
        return 0
    chunks = sorted(chunks, key=lambda c: c[0])
    superchunks = 1
    for i in range(1, len(chunks)):
        prev_offset, prev_nbytes = chunks[i - 1]
        curr_offset, _ = chunks[i]
        if curr_offset != prev_offset + prev_nbytes:
            superchunks += 1
    return superchunks


def analyze_file(path):
    """Return list of (type, name, total_chunks, superchunks) for all variables."""
    tree = ET.parse(path)
    root = tree.getroot()
    results = []
    for vtype in VAR_TYPES:
        for var in root.findall(f'dap:{vtype}', NS):
            name = var.get('name')
            chunks_elem = var.find('dmrpp:chunks', NS)
            if chunks_elem is None:
                continue
            chunk_list = [
                (int(c.get('offset')), int(c.get('nBytes')))
                for c in chunks_elem.findall('dmrpp:chunk', NS)
            ]
            results.append((vtype, name, len(chunk_list), count_superchunks(chunk_list)))
    return results


def print_table(file_results):
    files = list(file_results.keys())
    short_names = [f.split('/')[-1] for f in files]

    # Build unified variable list (type, name) from first file
    vars_in_order = [(vtype, name) for vtype, name, _, _ in file_results[files[0]]]

    # Build lookup: file -> (type, name) -> (total, sc)
    lookup = {}
    for fname, rows in file_results.items():
        lookup[fname] = {(vtype, name): (total, sc) for vtype, name, total, sc in rows}

    name_w = max(len(name) for _, name in vars_in_order)
    type_w = max(len(vtype) for vtype, _ in vars_in_order)

    header = f"{'Type':<{type_w}}  {'Variable':<{name_w}}"
    for sn in short_names:
        header += f"  {sn}"
    print(header)
    print('-' * len(header))

    totals = [0] * len(files)
    for vtype, name in vars_in_order:
        row = f"{vtype:<{type_w}}  {name:<{name_w}}"
        for i, fname in enumerate(files):
            total, sc = lookup[fname][(vtype, name)]
            row += f"  total={total}, superchunks={sc}, merged_away={total - sc}"
            totals[i] += sc
        print(row)

    print('-' * len(header))
    total_row = f"{'':>{type_w}}  {'TOTAL':<{name_w}}"
    for t in totals:
        total_row += f"  superchunks={t}"
    print(total_row)


if __name__ == '__main__':
    file_results = {}
    for fname in FILES:
        print(f"Analyzing {fname} ...")
        file_results[fname] = analyze_file(fname)

    print()
    print_table(file_results)
