# PE inspector

Install `pefile` for Python 3.12 or newer, then run:

```powershell
python -I tools/inspect_pe.py path/to/image.exe
```

The tool reads the file and prints its SHA-256, architecture, entry point, sections, import-library counts and unwind-function count as JSON. It does not execute or modify the input. Generated reports belong outside the repository because they can contain information from the user's executable.

## Native RPF tools

The C++17 reader lists the catalog of little-endian RPF7 archives with an `OPEN`
or `NONE` index and streams unencrypted binary entries to stdout. It recognizes
directory, binary and resource records. Resource payloads and nested archives
are listed as entries. The one-argument form probes RPF7 and RPF9 headers.

Build and test from the repository root using the
[quick start](../README.md#build-the-current-tools).

Install zlib development files to enable raw DEFLATE decoding. If CMake cannot
locate them, set `-DZLIB_ROOT=...` when configuring. Catalogs and uncompressed
binary files work with the base build. Both index bytes and total materialized
paths have a 64 MiB limit each.

```powershell
../rage62-rpf-build/rage62_rpf.exe --list sample.rpf
../rage62-rpf-build/rage62_rpf.exe --cat sample.rpf mods/message.txt
```

Use the spelling printed by `--list` for `--cat`. Redirect binary output with
PowerShell 7.4+ or `cmd.exe` to preserve bytes. Check the exit code before using
the output because a read or decode failure can occur after some bytes have been
written.

### Calling the reader from a native host

The `rage62_rpf7_bridge` DLL exposes the C interface in
[`rpf7.h`](rpf7/include/rage62/rpf7.h). A legacy host can include this header and
link the matching-architecture import library. Build the DLL with a C++17
toolchain. The host opens the archive by its UTF-8 filesystem path, enumerates
entries, streams the selected binary entry through a callback, and closes the
handle. Entry paths and the memory behind the handle belong to the DLL. This
interface was tested with a standalone C client. Engine integration requires a
host-side file adapter.

The record layout was cross-checked against the public
[CodeWalker RPF implementation](https://github.com/dexyfex/CodeWalker/blob/master/CodeWalker.Core/GameFiles/RpfFile.cs).

### RPF9 container inspection

`--rpf9-info` validates the 64-byte header and reports both table boundaries,
the payload region, the trailer length and the declared record count. The
payload region can start with alignment padding.

```powershell
../rage62-rpf-build/rage62_rpf.exe --rpf9-info sample.rpf
../rage62-rpf-build/rage62_rpf.exe --rpf9-list sample.rpf sample.catalog
../rage62-rpf-build/rage62_rpf.exe --rpf9-copy-span sample.rpf sample.catalog 0 > entry.bin
```

The last two commands take a separate, plaintext table you supply. Its size
must be the declared count multiplied by 24. Each row contains six little-
endian 32-bit words in this order.

| Byte offset | Field |
| --- | --- |
| 0 | Size hint |
| 4 | Zero |
| 8 | Archive offset in 16-byte units |
| 12 | Flags |
| 16 | Opaque identifier |
| 20 | Zero |

Rows keep their original order. Each span extends to the next distinct
offset or to the trailer. The raw output can contain padding, and the size
hint can differ from the logical file size. The 64 MiB catalog limit applies
to this input table. Use PowerShell 7.4+ or `cmd.exe` for binary redirection.

The C++ interface is in [`rpf9.h`](rpf7/include/rage62/rpf9.h). The on-disk
catalog and trailer are treated as opaque regions. Header geometry follows
the [published measurements](https://github.com/JoshRob297/rpf9-research/blob/main/docs/RPF9.md).
The supported location words follow the [public record description](https://github.com/zeozeozeo/rpf9/blob/main/src/entry.rs).

### Calling RPF9 from a native host

Link the import library for `rage62_rpf9_bridge` and include
[`rpf9_bridge.h`](rpf7/include/rage62/rpf9_bridge.h). Open the archive and the
supplied plaintext catalog, then use an entry index to query metadata or read
into a caller-owned buffer. Reads use offsets relative to the physical span.
They return fewer bytes at span end and zero bytes at end of span. The standalone
C host test checks that reads stay clear of the next entry and the archive
trailer. Resource decoding and the engine-side file adapter are separate steps.

### Starting the standalone PSIN host

Run `rage62_host` with the archive, a supplied plaintext catalog, the decimal
entry index and the exact stored byte length. Provide the length before padding.
An RPF9 size hint may describe a different size than the stored payload.

```powershell
../rage62-rpf-build/rage62_host.exe sample.rpf sample.catalog 0 59
```

The host reads a raw PSIN resource or one zlib member when built with zlib. It
validates the `LSCH` or `PMAP` section chain through `CHKS` and reports each
section's offset and length, plus schema and data-block counts. The
[`PsinResource` API](rpf7/include/rage62/psin.h) exposes the `LSCH` structures
and field descriptors and the `LENM` enums and signed values. Both tables
require the headers-first layout and exact record counts. Recognized `LSCH`
descriptors and `PMAP` block tables yield bounded data views. `PSCH` value
ranges in the `PMAP` variant expose opaque schema bytes. Earlier framing-only
`LSCH` resources without a recognized descriptor marker yield no data blocks.
Input and inflated data each have a 64 MiB limit. Typed instance decoding,
script execution, and RAGE 2015 integration remain separate milestones. See the
[PSIN format measurements](https://github.com/JoshRob297/rpf9-research/blob/main/docs/PSIN.md) and
[data-block layout](https://github.com/leonidaforge/research/blob/main/formats/unknown/psin/README.md).

For a chunked entry, add the path to the supplied plaintext chunk-table value
as the final argument. The stored byte length includes the encoding-2 prefix
and all padding described by the table. The input table can be up to 1 MiB.

```powershell
../rage62-rpf-build/rage62_host.exe sample.rpf sample.catalog 0 96 sample.chunks
```

The decoder supports table encodings 2, 3, and 4. It checks stored and decoded
sizes, skips table-declared padding, and reads each zlib block independently.
Copy blocks work in builds without zlib. C++ callers can read the decoded
payload through [`read_rpf9_chunks`](rpf7/include/rage62/rpf9_chunks.h).
The supported formats follow the [RPF9 payload measurements](https://github.com/leonidaforge/research/blob/main/formats/containers/rpf9/payloads.md#chunk-table).

Native C callers can use `rage62_rpf9_load_psin` from
[`rpf9_bridge.h`](rpf7/include/rage62/rpf9_bridge.h). Supply a null table and
zero length for raw or single-zlib entries, or the plaintext chunk table bytes
for a chunked entry. `rage62_psin_get_block` returns a borrowed view of the
resource data. Release the resource with `rage62_psin_close` after using it.
