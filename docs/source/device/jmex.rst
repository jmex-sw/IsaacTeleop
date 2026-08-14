.. SPDX-FileCopyrightText: Copyright (c) 2026 j-mex. All rights reserved.
.. SPDX-License-Identifier: Apache-2.0

.. _jmex-moxi-plugin:

j-mex MOXI Motion Capture
=========================

A plugin for streaming a `MOXI <https://www.j-mex.com/>`_ inertial motion capture suit into Isaac
Teleop over the network. Unlike the leader arms on the
:doc:`generic joint-space path <joint_space>`, the operator wears the device and the joint angles
arrive over the network from a vendor application rather than from a mechanism on the desk.

MOXI Player, the vendor application, does the capture. In its **robot dialect** it has already
retargeted the operator's motion onto a robot URDF, so the channel this plugin opens carries the
target robot's own named joint angles, ready for the joint-space path.

.. contents:: On this page
   :local:
   :depth: 2

Components
----------

- **Core library** (``jmex_plugin_core``) — owns the MOXI receiver session and turns one channel's
  frames into the published payload.
- **Plugin executable** (``jmex_plugin``) — the plugin binary; one OpenXR session, one publisher.
- **CLI tool** (``jmex_joint_state_printer``) — reads back what the plugin publishes with no Python
  and no retargeting graph in the way.

What the plugin publishes
-------------------------

.. list-table::
   :header-rows: 1
   :widths: 25 75

   * - Collection
     - ``jmex`` by default; the plugin's second positional argument
   * - Schema
     - ``JointStateOutput``
   * - Consumer
     - ``JointStateSource``
   * - Contents
     - Every **revolute** joint of the loaded robot: name, angle [rad], angular velocity [rad/s]

This is a **mapping, not a conversion**: MOXI's joint name becomes ``JointState.name`` verbatim, its
rotation angle becomes ``position``, its angular velocity becomes ``velocity``. The consumer's
``joint_names`` are therefore matched by name and wire order does not matter. Root and fixed joints
carry no angle and are not actuated degrees of freedom, so they are not published.

Because Player has already solved the retargeting, the joint angles describe **the robot Player
loaded**, not the operator. Load the same robot on both ends.

Prerequisites
-------------

- **MOXI Receiver SDK**, 1.1 or newer — a separate download from j-mex; not bundled here. CMake
  finds it at configure time and skips the whole plugin when it is absent, so a tree without the SDK
  still builds.
- **MOXI Player for Robot** — the vendor application, running **on a second machine** (see
  :ref:`jmex-player-separate-host`).
- **CloudXR runtime** — the tensor transport is an OpenXR runtime feature, so the runtime is a hard
  requirement even though nothing here is a headset.

Building
--------

Unpack the SDK into the plugin's own directory as ``src/plugins/jmex/MOXIReceiverSDK/``, so that
``src/plugins/jmex/MOXIReceiverSDK/sdk/linux-x64/lib/cmake/MOXIReceiverSDK/`` exists. No build flags
are then needed:

.. code-block:: bash

   cmake -B build
   cmake --build build --parallel
   cmake --install build

That directory is covered by the plugin's ``.gitignore``: the SDK is obtained from j-mex and is not
Apache-2.0 licensed, so it must not be committed.

To keep the SDK elsewhere, point ``CMAKE_PREFIX_PATH`` or ``MOXI_SDK_ROOT`` (a CMake or environment
variable) at the SDK's platform directory — the one containing ``lib/cmake/MOXIReceiverSDK``. Both
take precedence over a copy unpacked in the plugin directory; ``MOXI_SDK_ROOT`` is searched first
and warns rather than falling back silently when it holds no SDK the plugin can use. Either way, the
configure output names the SDK that answered:

.. code-block:: bash

   cmake -B build -DMOXI_SDK_ROOT=/path/to/MOXIReceiverSDK-1.1.0/sdk/linux-x64
   # -- jmex plugin: MOXIReceiverSDK 1.1.0 from /path/to/MOXIReceiverSDK-1.1.0/sdk/linux-x64/lib/cmake/MOXIReceiverSDK

If the plugin is missing from your build, look for ``Skipping jmex plugin build:`` in the configure
output.

MOXI Player
-----------

Player captures the motion, retargets it onto the robot URDF, and connects to this plugin. Its own
README and glove guide ship with the application and cover the workflow in full; what follows is
what matters for driving Isaac Teleop.

First-time setup on the Player host
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   ./init_env.sh     # runtime libraries, plus a udev rule for the glove dongle
   ./check_env.sh    # read-only verification; prints PASS / WARN / FAIL

``init_env.sh`` installs the runtime libraries and a udev rule so the glove dongle is accessible
without ``sudo``. **Unplug and replug the dongle once afterwards** — udev applies new permissions
only when a device is reconnected. This is a one-time step per machine.

Every session
~~~~~~~~~~~~~

1. Plug the dongle **directly** into the Player host, not through a hub or extension cable.
2. Put on and power on the gloves; they pair with the dongle automatically.
3. Launch Player — **without** ``sudo``.
4. Select the motion source in the Player UI and start streaming on the channel the plugin is
   opening (the plugin defaults to channel ``255``).

Glove calibration is not done in Player, which only reads glove data; it is performed with the
Manus Core application on a Windows PC. The gloves ship calibrated, so day-to-day use needs no
per-session calibration.

.. _jmex-player-separate-host:

Run Player on a separate host
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

With **Receiver SDK 1.0.0**, Player and the plugin **cannot share a machine**. Player binds UDP
``10100`` to receive the receiver's advertisements, and the Receiver SDK binds the same port as its
broadcast *source* port, so on one host the second one loses. The failure is silent: startup still
reports success, the SDK prints ``bind socker error: Address already in use``, no advertisement goes
out, Player never learns where to connect, and the plugin waits for a pairing that cannot happen. If
the plugin sits at ``Waiting for MOXI Player to pair`` forever, check for a local Player first:

.. code-block:: bash

   ss -lnup | grep 10100

A later SDK release may drop that bind and lift the restriction, but the failure gives no clue that
the SDK version is what matters, so check it against the version you have.

Both hosts must be on the same subnet: the advertisement is a ``255.255.255.255`` limited
broadcast, which routers do not forward. On wireless networks, access-point client isolation blocks
it too — suspect that before suspecting the plugin.

The **product line must match**: this plugin opens the robot dialect, and a robot-line Player does
not downgrade. Pointing it at a general-line Player desynchronizes the parse rather than failing
cleanly.

What you need to see it run
~~~~~~~~~~~~~~~~~~~~~~~~~~~

This plugin has **no synthetic backend**, unlike the leader arms. Nothing here produces data on its
own: the SDK is required to build it and Player is required to feed it, so without the vendor stack
the plugin cannot be exercised at all.

What replay removes is the *suit and the operator*, not the vendor software. Player can load and
replay a recorded motion file, which streams real data down the real wire protocol, so everything
downstream runs exactly as it does with someone wearing the suit.

Running the plugin
------------------

The CloudXR runtime must be running with a **concrete device profile**. No headset is required, but
the default ``auto-webrtc`` profile resolves the device from whichever client connects, so with no
client there is no system and the plugin dies at startup with
``Failed to get OpenXR system: -35`` (``XR_ERROR_FORM_FACTOR_UNAVAILABLE``):

.. code-block:: bash

   NV_DEVICE_PROFILE=Quest3 python -m isaacteleop.cloudxr

Then start the plugin. **The receiver is the TCP server and Player is the client** — the direction
is the opposite of what the data flow suggests, so starting the plugin before Player is up is the
normal case: it waits, and keeps waiting if Player disconnects and comes back.

.. code-block:: bash

   # Defaults: channel 255, collection "jmex"
   ./install/plugins/jmex/jmex_plugin

   # Explicit channel and collection id
   ./install/plugins/jmex/jmex_plugin 255 jmex

Only **one instance per machine**: the receiver is process-global, and this plugin always takes the
same TCP and UDP ports, so a second one has nothing to bind to.

Verifying the transport
~~~~~~~~~~~~~~~~~~~~~~~

``jmex_joint_state_printer`` separates "the device path works" from "my pipeline is misconfigured":

.. code-block:: bash

   ./install/plugins/jmex/jmex_joint_state_printer jmex

Consuming the data
------------------

On the same ``collection_id`` as the plugin:

.. code-block:: python

   from isaacteleop.retargeting_engine.deviceio_source_nodes import JointStateSource

   source = JointStateSource(
       name="moxi",
       collection_id="jmex",
       joint_names=[...],  # the robot's DOF names, as MOXI reports them
   )

``joint_names`` is matched by name, so a subset is fine — list the DOFs your retargeting graph
drives. Run ``jmex_joint_state_printer`` to see the exact names your Player is sending.

Troubleshooting
---------------

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - Symptom
     - What to check
   * - Plugin waits at ``Waiting for MOXI Player to pair`` forever
     - A Player on the same host (``ss -lnup | grep 10100``); both hosts on the same subnet;
       access-point client isolation on wireless
   * - ``Failed to get OpenXR system: -35``
     - The CloudXR runtime is not running, or is running with an ``auto-*`` device profile
   * - ``Skipping jmex plugin build:`` at configure time
     - No SDK in ``src/plugins/jmex/MOXIReceiverSDK/``, and ``CMAKE_PREFIX_PATH`` /
       ``MOXI_SDK_ROOT`` does not point at an SDK platform directory either
   * - Joint names do not match the robot asset
     - Player is loaded with a different robot; run ``jmex_joint_state_printer`` to see what is
       actually being sent
   * - ``no revolute joints on channel`` warning
     - Player is on the general product line; this plugin opens the robot dialect
   * - Player only sees the gloves with ``sudo``
     - The udev rule has not taken effect: re-run ``init_env.sh``, then unplug and replug the dongle

See Player's own README and glove guide for the capture workflow, and the
:code-file:`plugin README <src/plugins/jmex/README.md>` for the build and run steps beside the
installed binary.
