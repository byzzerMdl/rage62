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

From the repository root, build and test with CMake 3.20 or newer.

```powershell
cmake -S tools/rpf7 -B ../rage62-rpf-build
cmake --build ../rage62-rpf-build --config Release
ctest --test-dir ../rage62-rpf-build -C Release --output-on-failure
```

Install zlib development files to enable raw DEFLATE decoding. If CMake cannot
locate them, set `-DZLIB_ROOT=...` when configuring. Catalogs and uncompressed
binary files work with the base build. Both index bytes and total materialized
paths have a 64 MiB limit each.

```powershell
../rage62-rpf-build/Release/rage62_rpf.exe --list sample.rpf
../rage62-rpf-build/Release/rage62_rpf.exe --cat sample.rpf mods/message.txt
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
