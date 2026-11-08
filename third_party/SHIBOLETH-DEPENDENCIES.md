# VST3 build dependencies

- clap-wrapper 0.17.0, commit 806bba0a5f792e3966b99c1561ab9952d3316d7c (MIT), https://github.com/free-audio/clap-wrapper
- VST3 SDK v3.8.0_build_66, commit 9fad9770f2ae8542ab1a548a68c1ad1ac690abe0 (MIT), https://github.com/steinbergmedia/vst3sdk
- CLAP headers retain their existing pinned revision.
- Build-local tools: CMake 4.4.4, Ninja 1.13.2 (installed under build/tools).

The wrapper has one local, conditional patch: SHIBOLETH_SAMPLE_RATE_LIMITS rejects sample rates outside 8-384 kHz and invalid block limits during setupProcessing, before changing its last valid configuration. Shiboleth activate() already enforces that sample-rate range. Without this check, the wrapper accepts unsupported formats at setup and fails later at activation. The patch is preserved in shiboleth-wrapper.patch; apply it after checking out the pinned wrapper when restoring dependencies.

Recreate dependencies using git clone for the URLs above, checkout the pinned commits, and run `git submodule update --init --depth 1 base public.sdk pluginterfaces cmake` in the SDK directory. Apply the patch with `git -C third_party/clap-wrapper apply ../shiboleth-wrapper.patch`. Install project-local tools with `python -m pip install --target build/tools cmake==4.4.4 ninja==1.13.2`.
