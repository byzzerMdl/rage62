# RAGE 9 Native Recompilation Project

**An experimental recompilation and compatibility project aimed at bringing GTA VI's RAGE 9 engine to a RAGE 2015-based codebase, with the ultimate goal of running GTA VI natively.**

> ⚠️ **PROJECT STATUS: EARLY DEVELOPMENT — NOT PLAYABLE**
>
> GTA VI cannot currently be launched using this project. Many engine components are incomplete, incompatible, or still under development.
>
> **The project is actively being developed, and contributions are welcome.**

## 📖 About the Project

This project focuses on adapting and rebuilding engine components to make the older RAGE 2015 codebase compatible with the newer RAGE 9 architecture used by GTA VI.

Our approach involves rewriting engine modules, implementing missing functionality, adapting existing systems, and resolving compatibility issues between different generations of the RAGE engine.

Rather than distributing a complete game engine, this repository provides **rewritten modules, compatibility implementations, patches, and integration code** intended to work with an external RAGE 2015 source tree.

Our long-term goal is to achieve native execution of GTA VI through a modified and recompiled RAGE-based environment.

## ⚠️ Important: Source Code and Distribution

**WE DO NOT DISTRIBUTE THE ORIGINAL LEAKED RAGE SOURCE CODE OR GTA VI BINARIES.**

This repository does not include:

- Original leaked RAGE 2015 source files.
- Original RAGE 9 source code.
- GTA VI game binaries or executable files.
- Original RPF9 archives or game assets.
- Complete proprietary engine source trees.

Instead, this repository contains:

- Rewritten engine modules.
- Custom implementations of engine functionality.
- Compatibility layers between RAGE 2015 and RAGE 9.
- Code designed for native execution.
- Integration code, patches, and related development tools.

**All modules published in this repository are intended to be independently written implementations rather than copies of the original proprietary source files.**

However, these modules are specifically designed to be compiled and integrated into a compatible RAGE 2015 codebase.

**This is not a standalone engine.**

The repository cannot be compiled into a fully functional GTA VI executable without additional engine components and game files that are not provided here.

## 🔧 How Does It Work?

The project uses a RAGE 2015-based environment as its compatibility target.

We are rewriting and adapting engine components to support the architecture, systems, and functionality required by RAGE 9.

The general development process involves:

1. **Module rewriting** — Reimplementing engine modules to support the newer engine requirements.
2. **Compatibility development** — Adapting RAGE 2015 systems to work with RAGE 9 functionality.
3. **Module compilation** — Compiling rewritten modules against a compatible local engine source tree.
4. **Engine integration** — Integrating the compiled modules into the modified RAGE environment.
5. **Testing and debugging** — Identifying unsupported systems, missing functionality, and compatibility issues.

The rewritten modules are intended to replace or extend specific components of the existing engine.

**Simply downloading this repository will not produce a working game executable.**

## 📦 Requirements

To work with the project, developers will need access to a compatible development environment.

Depending on the modules being compiled or tested, this may include:

| Component | Required for |
|---|---|
| RAGE 2015 source tree | Compiling and integrating rewritten engine modules |
| Relevant RAGE 9 / GTA VI binaries | Compatibility research and testing |
| RPF9 game assets/build files | Testing game asset and archive compatibility |
| C++ build environment | Compiling modules |
| Project modules | Implementing engine compatibility |

**Important:** These requirements are not included in this repository.

Some developers may already have access to relevant development materials from previously leaked builds. This project does not host, redistribute, or provide download links for such materials. Contributors are responsible for ensuring they are authorized to use any external source code, binaries, or assets.

## 🚧 Current Development Status

<escape>🔴 **Native GTA VI Launch:** Not working</escape>

<escape>🟡 **RAGE 9 Compatibility:** Work in progress</escape>

<escape>🟡 **Engine Module Rewrites:** In development</escape>

<escape>🟡 **RPF9 Compatibility:** In development</escape>

<escape>🟡 **Engine Integration:** Experimental</escape>

<escape>🟢 **Project Development:** Active</escape>

### What Works?

At the moment, the project is focused on developing and integrating rewritten modules.

Individual components may compile or function independently, but **the engine is not yet capable of launching GTA VI natively.**

Many core systems still require substantial development, testing, and compatibility work.

Please do not expect a playable game or a complete working engine.

## 🛠️ Building and Integration

**This repository is intended primarily for developers.**

The modules provided here are not standalone executables.

They must be compiled and integrated into a compatible RAGE 2015 development environment.

The general workflow is:

1. Prepare a compatible local RAGE 2015 source tree.
2. Obtain the rewritten modules from this repository.
3. Integrate the required modules into the appropriate engine components.
4. Configure the relevant build dependencies.
5. Compile the modified engine components.
6. Resolve any remaining integration or compatibility issues.

**There is currently no universal one-click build process.**

Exact integration steps depend on the module and the local engine configuration.

Additional build documentation will be added as development progresses.

See [the native RPF tools](tools/README.md#native-rpf-tools) for standalone archive inspection and the C host interface.

## 🤝 Contributing

**WE NEED CONTRIBUTORS!**

This is a highly experimental project, and there is still a significant amount of work to do.

If you have experience with RAGE internals, engine recompilation, C++, game engine architecture, or compatibility development, your contributions would be valuable.

We especially welcome developers familiar with:

- RAGE 2015 engine architecture.
- RAGE 9 engine systems.
- RPF9 archive structures.
- Game engine module development.
- C++ reverse engineering and compatibility research.
- Engine compilation and debugging.

### Have Access to Relevant Development Builds?

If you already have authorized access to RAGE-related binaries, an RPF9-based build, or a compatible RAGE 2015 development environment, you may be able to help identify incompatibilities and test rewritten modules.

**Please submit pull requests!**

You can contribute by:

- Rewriting missing engine modules.
- Improving existing module implementations.
- Fixing compilation errors.
- Resolving engine compatibility problems.
- Implementing missing engine functionality.
- Improving documentation and integration instructions.
- Testing modules and reporting reproducible issues.

Please ensure that your pull requests contain only code and materials you have the right to contribute.

**Do not submit leaked proprietary source code, original game binaries, or copyrighted game assets.**

## 🎯 Project Goal

Our ultimate goal is to create a functional native execution environment for GTA VI by adapting a RAGE 2015-based engine through rewritten modules and compatibility implementations.

This is an ambitious experimental effort.

There is no guarantee that full compatibility or native game execution will be achieved.

Nevertheless, development is ongoing, and we welcome anyone interested in helping move the project forward.

## ⚖️ Disclaimer

This is an independent, unofficial research and development project.

It is not affiliated with, sponsored by, or endorsed by Rockstar Games or Take-Two Interactive.

RAGE, Grand Theft Auto, and GTA VI are trademarks or properties of their respective owners.

This repository does not distribute the original proprietary RAGE engine source code, GTA VI binaries, or copyrighted game assets.

The code provided here is intended for compatibility research, experimentation, and independent module development.

**No original proprietary engine code or game files are provided or licensed through this repository.**

---

**⭐ Interested in the project? Consider starring the repository and contributing through pull requests!**

**Development is active. Native execution is the goal.**
