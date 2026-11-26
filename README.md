# rage62

rage62 adds RAGE 9 compatibility to a RAGE 2015 (RAGE 7) host on Windows x64.
Independently written modules extend or replace specific subsystems in the
RAGE 2015 host so it can handle the data and runtime interfaces expected by
RAGE 9. The long-term goal is native GTA VI startup in the adapted host.

**Early development. Native GTA VI startup remains unverified.**

| Component | Status |
| --- | --- |
| RPF7 baseline | The standalone reader lists little-endian `OPEN` and `NONE` catalogs and streams unencrypted binary entries. zlib enables raw DEFLATE. |
| Native host bridges | Windows DLLs expose a C ABI for RPF7 and RPF9. The RPF9 adapter supports bounded, offset-based reads from physical entry spans. |
| RPF9 | Reads the 64-byte header, lists location records from a supplied plaintext catalog, and copies raw payload spans. Verified with synthetic fixtures. |
| RAGE 7 integration | The first milestone is loading one RAGE 9 resource through a RAGE 2015 host subsystem. |
| Game startup | Requires verified asset loading, runtime interfaces, and renderer integration. |

## Build the current tools

Use a C and C++17 toolchain, CMake 3.20 or newer, and Ninja. On Windows,
run these commands at the repository root in an x64 Developer PowerShell
for Visual Studio.

```powershell
cmake -S tools/rpf7 -B ../rage62-rpf-build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build ../rage62-rpf-build
ctest --test-dir ../rage62-rpf-build --output-on-failure
```

CMake detects zlib when its development files are installed. The tests generate
their own archives. See the [tools guide](tools/README.md) for CLI examples,
reader limits, and details on calling the DLL.

## Compatibility roadmap

Engine-side tests require access to a compatible RAGE 2015 development
environment with permission to modify it.

1. **Implement the required RAGE 9 data support.** Use the RPF9 reader with
   a synthetic catalog to select an entry. Decode one resource type and
   verify its version, metadata, and payload bytes independently.
2. **Integrate the module into RAGE 2015.** Build it against the compatible
   host and connect it to the relevant filesystem or resource-loading
   subsystem. Verify that the host can consume the synthetic RAGE 9 resource.
   Use RPF7 as a regression check for the existing archive path.
3. **Extend the host's runtime and rendering support.** Test the platform services,
   streaming, and renderer behavior needed for a controlled startup. Record the
   first reproducible milestone for each subsystem. Game launch is the final
   integration milestone.

## Future Rust migration

We plan to migrate the independently developed rage62 codebase to Rust
incrementally, starting with archive parsing and streaming I/O. The first
Rust implementation should preserve the current C ABI so existing host
callers can use it without changes. The C++ implementation and synthetic
fixtures provide a baseline for behavior during the transition.

The optimization goal is to reduce archive-open latency, improve streaming
and decompression throughput, and reduce peak memory. Compare Release builds
on the same machine and fixtures before replacing each implementation.
Performance gains will be reported with measurements.

## Contributing

Issues and focused pull requests are welcome. Useful starting points include
a reproducible archive-format fixture, a native filesystem adapter tested
with synthetic data, or a benchmark of the current C++ implementation.

1. Describe the subsystem and what success looks like in your issue or PR.
2. Include the platform, toolchain, and exact verification commands. For
   archive changes, include a small synthetic case or a generator.
3. Explain which boundary you verified, such as a standalone parser, a native
   host, or an engine integration test.

Contributor and agent instructions are in [AGENTS.md](AGENTS.md).
Claude Code loads the same instructions through [CLAUDE.md](CLAUDE.md).

## Project scope

rage62 is an independent project. Contributions must consist of independently
written code and materials the contributor is authorized to share. The
repository accepts synthetic test data and public format references. Keep
proprietary engine source, game binaries, assets, and keys out of the
repository, issues, and pull requests.

RAGE, Grand Theft Auto, and GTA VI are trademarks or properties of their
respective owners. The project is unaffiliated with Rockstar Games and
Take-Two Interactive.
