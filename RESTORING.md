# Preserving and restoring this repository

This repository includes Shiboleth and the existing `imperius/` instrument:
source code, tests, build scripts, artwork, design notes and documentation.
Downloaded dependencies and generated `build/` directories are intentionally
ignored. Our dependency notes, third-party notices and wrapper patch are tracked.

## Restore dependencies in a fresh checkout

Run from the repository root in PowerShell. These commands assume the dependency
directories do not yet exist. Git, Python and the MinGW-w64 UCRT64 C++ toolchain
are prerequisites; the build scripts default to `C:/msys64/ucrt64/bin/g++.exe`.

```powershell
git clone https://github.com/free-audio/clap.git third_party/clap
git -C third_party/clap checkout a47f6badb49d948fd009998f28309cdab78979c9

git clone https://github.com/free-audio/clap-wrapper.git third_party/clap-wrapper
git -C third_party/clap-wrapper checkout 806bba0a5f792e3966b99c1561ab9952d3316d7c
git -C third_party/clap-wrapper apply ../shiboleth-wrapper.patch

git clone https://github.com/steinbergmedia/vst3sdk.git third_party/vst3sdk
git -C third_party/vst3sdk checkout 9fad9770f2ae8542ab1a548a68c1ad1ac690abe0
git -C third_party/vst3sdk submodule update --init --depth 1 base public.sdk pluginterfaces cmake

python -m pip install --target build/tools cmake==4.4.4 ninja==1.13.2
```

For CLAP-only builds, only the CLAP dependency is needed.

## Build

```powershell
./build.ps1                 # Shiboleth CLAP + tests
./build-vst3.ps1            # Both formats, tests, VST3 validator, sharing ZIP
./imperius/build.ps1        # Imperius CLAP + tests
```

See `README.md` and `third_party/SHIBOLETH-DEPENDENCIES.md` for details.
Generated binaries and sharing ZIPs are not part of Git history. Save releases
separately if you want to preserve the exact binary you used in a DAW project.

## Save changes and back up

```powershell
git status
git add .
git commit -m "Describe the changes"
```

Git history is local until backed up or pushed to a remote. You can make a portable
backup of all committed history with:

```powershell
git bundle create ../abdicant-syn-backup.bundle --all
```

Store that bundle on another drive or backup service. It contains committed files
and history, not ignored builds, dependencies or uncommitted changes. No remote is
configured automatically.
