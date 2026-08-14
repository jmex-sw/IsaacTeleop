<!--
SPDX-FileCopyrightText: Copyright (c) 2026 j-mex. All rights reserved.
SPDX-License-Identifier: Apache-2.0
-->

# j-mex MOXI plugin

Streams a [MOXI](https://www.j-mex.com/) motion capture channel into Isaac Teleop as
`JointStateOutput` on the [generic joint-space device path](../../../docs/source/device/joint_space.rst).

This file is the short version of building and running the binaries, beside the installed plugin.
The [device page](../../../docs/source/device/jmex.rst) is the full guide: what the plugin
publishes, how to run MOXI Player, how to consume the data, and what to check when it does not
work.

## Prerequisites

1. **MOXI Receiver SDK** 1.1 or newer — a separate download from j-mex; not bundled here.
2. **MOXI Player**, on this machine or another on the same subnet, started in either order — the
   device page covers the host layout.
3. **CloudXR runtime**, started with a concrete device profile — the tensor transport is an OpenXR
   runtime feature, so it is required even though no headset is:

   ```bash
   NV_DEVICE_PROFILE=Quest3 python -m isaacteleop.cloudxr
   ```

## Building

Unpack the SDK into this directory as `MOXIReceiverSDK/`, so that
`src/plugins/jmex/MOXIReceiverSDK/sdk/linux-x64/lib/cmake/MOXIReceiverSDK/` exists. Nothing else is
needed:

```bash
cmake -B build
cmake --build build --parallel
cmake --install build
```

`.gitignore` keeps that directory out of the repository — the SDK is yours to obtain and is not
Apache-2.0, so it must not be committed.

To keep the SDK elsewhere, point either `CMAKE_PREFIX_PATH` or `MOXI_SDK_ROOT` (a CMake variable or
an environment variable) at the SDK's platform directory — the one containing
`lib/cmake/MOXIReceiverSDK`. `MOXI_SDK_ROOT` is searched first, and warns instead of falling back
silently if it holds no SDK the plugin can use:

```bash
cmake -B build -DMOXI_SDK_ROOT=/path/to/MOXIReceiverSDK-1.1.0/sdk/linux-x64
```

Without an SDK the plugin is skipped and the rest of the tree still builds — look for
`Skipping jmex plugin build:` in the configure output.

## Running

```bash
# Defaults: channel 255, collection id "jmex"
./install/plugins/jmex/jmex_plugin

# Explicit channel and collection id
./install/plugins/jmex/jmex_plugin 255 jmex
```

`jmex_joint_state_printer` reads back what the plugin publishes with no Python and no retargeting
graph in the way, so it separates "the device path works" from "my pipeline is misconfigured":

```bash
./install/plugins/jmex/jmex_joint_state_printer jmex
```
