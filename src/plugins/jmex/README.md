<!--
SPDX-FileCopyrightText: Copyright (c) 2026 j-mex. All rights reserved.
SPDX-License-Identifier: Apache-2.0
-->

# j-mex MOXI plugin

Streams a [MOXI](https://www.j-mex.com/) motion capture channel into Isaac Teleop as
`JointStateOutput` on the [generic joint-space device path](../../../docs/source/device/joint_space.rst).

MOXI Player speaks two dialects. In the **robot dialect** it has already retargeted the operator's
motion into the target robot's own named joint angles, which is exactly what `JointStateOutput`
describes — so this plugin maps rather than converts: MOXI's joint name becomes `JointState.name`
verbatim, its rotation angle becomes `position` [rad], its angular velocity becomes `velocity`
[rad/s]. Only **revolute** joints are published; the root and fixed joints in the stream carry no
angle and are not actuated degrees of freedom.

## Prerequisites

1. **MOXI Receiver SDK** (a separate download from j-mex; not bundled here).
2. **MOXI Player**, running on a second machine and paired with this one over the local network.

The SDK is found by CMake at configure time. Without it the plugin is skipped and the rest of the
tree still builds — look for `Skipping jmex plugin build:` in the configure output.

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
`lib/cmake/MOXIReceiverSDK`. Both take precedence over the local copy:

```bash
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/MOXIReceiverSDK-1.0.0/sdk/linux-x64
```

## Running

### The CloudXR device profile is not optional

The tensor transport is an OpenXR runtime feature, so the CloudXR runtime must be running — and it
must be running with a **concrete device profile**:

```bash
NV_DEVICE_PROFILE=Quest3 python -m isaacteleop.cloudxr
```

No headset is required. But the default profile, `auto-webrtc`, resolves the device from whichever
client connects, so with no client there is no system and this plugin dies at startup with:

```
Failed to get OpenXR system: -35        # XR_ERROR_FORM_FACTOR_UNAVAILABLE
```

A concrete profile (`Quest3`, `AppleVisionPro`) declares the device up front and the runtime hands
out a system immediately — the plugin and the printer both run in headless mode and never touch the
XR frame loop. Set it as an environment variable; a `--cloudxr-env-config` file did not take effect.

```bash
# Defaults: channel 255, collection id "jmex"
./install/plugins/jmex/jmex_plugin

# Explicit channel and collection id
./install/plugins/jmex/jmex_plugin 255 jmex
```

**The receiver is the TCP server and MOXI Player is the client** — the direction is the opposite of
what the data flow suggests. Starting the plugin before the Player is up is the normal case: it
waits, and it keeps waiting if the Player disconnects and comes back.

Only **one instance per machine**: the SDK's receiver is process-global and its broadcast socket
binds a fixed port, so a second one has nothing to bind to.

**Run MOXI Player on a different machine than this plugin** (Receiver SDK 1.0.0). Player binds UDP
10100 to receive the receiver's advertisements, and the C++ SDK binds the same port as its broadcast
*source* port, so on
one host the second one loses. The failure is silent: `MxRStartSystem` still returns true, the SDK
prints `bind socker error: Address already in use`, no advertisement goes out, Player never learns
where to connect, and the plugin waits for a pairing that cannot happen. If the plugin sits at
"Waiting for MOXI Player to pair" forever, check for a local Player first:

```bash
ss -lnup | grep 10100
```

### What you need to see it run

This plugin has **no synthetic backend**, unlike `so101_leader` and `rebot_devarm_leader`. Nothing
here produces data on its own: the SDK is required to build it and MOXI Player is required to feed
it, so without the vendor stack the plugin builds (or is skipped) but cannot be exercised.

What replay removes is the *suit and the operator*, not the vendor software. Player can load and
replay a recorded motion file, which streams real data down the real wire protocol, so everything
downstream — this plugin, the tracker, the retargeting graph — runs exactly as it does with someone
wearing the suit. That is how this plugin was verified.

### Verifying the transport

`jmex_joint_state_printer` reads what the plugin publishes with no Python and no retargeting graph
in the way, so it separates "the device path works" from "my pipeline is misconfigured":

```bash
# Terminal 1
./install/plugins/jmex/jmex_plugin
# Terminal 2
./install/plugins/jmex/jmex_joint_state_printer jmex
```

## Consuming the data

Consumer side, on the same `collection_id`:

```python
from isaacteleop.retargeting_engine.deviceio_source_nodes import JointStateSource

source = JointStateSource(
    name="moxi",
    collection_id="jmex",
    joint_names=[...],  # the robot's DOF names, as MOXI reports them
)
```

`joint_names` is matched **by name**, so wire order does not matter and a subset is fine — list the
DOFs your retargeting graph drives. Run `jmex_joint_state_printer` to see the exact names your
Player is sending.

### The 4 KB budget

One serialized `JointStateOutput` must fit in 4096 bytes, because `JointStateSource` builds its
`JointStateTracker` with the default size and offers no way to raise it. A full humanoid's actuated
joints fit comfortably; adding the non-actuated joints would not. If the budget is ever exceeded the
plugin says so on stderr and drops the sample rather than failing silently.
