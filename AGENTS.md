# Contributing to rage62

Read the [project overview](README.md) for the RAGE 7 host target, current
verification status, and Rust roadmap. See the [tools guide](tools/README.md)
for build commands and CLI usage.

## Find the relevant code

- `tools/inspect_pe.py` inspects a PE file and reports image metadata.
- `tools/rpf7/src/archive.cpp` handles common header probing, the open RPF7
  catalog, and binary payload streaming.
- `tools/rpf7/include/rage62/rpf9.h` and `tools/rpf7/src/rpf9.cpp` handle the
  RPF9 header, supplied plaintext location records, and raw spans.
- `tools/rpf7/include/rage62/rpf7.h` and `tools/rpf7/src/bridge.cpp` define the
  C ABI used by native host callers.
- `tools/rpf7/include/rage62/rpf9_bridge.h` and `tools/rpf7/src/rpf9_bridge.cpp`
  expose the RPF9 entry metadata and bounded read API to a native host.
- `tools/rpf7/src/host.cpp` and `tools/rpf7/src/psin.cpp` load PSIN framing
  from an RPF9 entry in the standalone host.
- `tools/rpf7/tests/` contains the synthetic archive reader and C host tests.

## Develop and verify a change

- For compatibility work, identify the RAGE 9 requirement and the RAGE 2015
  host subsystem the module extends. Use RPF7 checks to verify the legacy
  archive path during integration.
- Keep each change focused on a single module or host interface. Preserve
  existing caller behavior. If a change affects the host ABI, explain how
  callers built with older compilers can upgrade.
- Use independent implementations and synthetic fixtures. Cite the public
  description or specification used for any new format assumption. Keep
  proprietary source, binaries, game assets, and keys out of repository files
  and GitHub discussions.
- For archive changes, exercise the relevant success and error paths with a
  generated fixture. When touching compression, verify builds with and
  without zlib. When touching the C ABI, run the standalone C host test.
- Build out of tree. Run the CMake and CTest commands from the README and
  report the compiler, configuration, commands, and observed results.
- Describe the boundary actually tested in the PR. State engine integration
  or game startup results only when verified at runtime.

## Rust contributions

Port one module at a time. Reuse synthetic fixtures to compare behavior with
the C++ implementation, keep FFI ownership explicit, and measure the relevant
Release workload before making a performance claim.
