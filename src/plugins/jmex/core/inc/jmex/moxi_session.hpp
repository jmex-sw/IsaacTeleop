// SPDX-FileCopyrightText: Copyright (c) 2026 j-mex. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace plugins
{
namespace jmex
{

//! One actuated degree of freedom on a MOXI channel: a joint whose mechanism type is revolute.
//! Root and fixed joints are not actuated joints -- they carry no angle -- and never appear here.
struct ActuatedJoint
{
    //! Index into the channel's joint array; the key for every per-joint SDK call.
    int bone_index;
    //! The name MOXI reports on the wire, used verbatim as the cross-system lookup key.
    std::string name;
};

/*!
 * @brief The device half of the plugin: one MOXI channel, from pairing to per-frame reads.
 *
 * Owns the process-global MOXI receiver. The SDK's entry points are free functions over a single
 * internal receiver, and on Linux its broadcast socket binds port 10100, so **only one instance may
 * exist per process, and only one such process per machine**. The constructor enforces the first
 * half of that.
 *
 * MOXI Player is the TCP *client*: the SDK listens and the Player connects in. So a freshly
 * constructed session is normally not streaming yet -- callers poll and wait rather than treating
 * the initial silence as a failure. The same is true after a disconnect; the session keeps waiting
 * for the Player to come back.
 *
 * This class is dialect-agnostic on purpose. It exposes what the Robot dialect needs today
 * (actuated joint angles); the general dialect's bone poses will be additional readers on the same
 * connection, not a second connection.
 */
class MoxiSession
{
public:
    //! Default TCP port the SDK's server binds and advertises to MOXI Player.
    static constexpr int DEFAULT_TCP_PORT = 7000;

    /*!
     * @param channel MOXI channel id to open (one skeleton per channel).
     * @param mtype Player dialect, e.g. ``MOXI_LOCAL_MOTION_ROBOT``. Must match the Player's
     *        product line or the stream will not decode.
     * @param tcp_port Port the SDK's TCP server binds.
     * @throws std::runtime_error if another MoxiSession already exists in this process, or if the
     *         SDK refuses to start (almost always: the TCP port is in use).
     */
    explicit MoxiSession(int channel, int mtype, int tcp_port = DEFAULT_TCP_PORT);
    ~MoxiSession();

    MoxiSession(const MoxiSession&) = delete;
    MoxiSession& operator=(const MoxiSession&) = delete;

    /*!
     * @brief Pump the receiver once and report whether a *new* frame arrived.
     *
     * Returns true only when the channel's sequence id moved, so callers can push exactly one
     * sample per delivered frame instead of re-publishing stale values at the poll rate. Discovers
     * the actuated joints on the first frame of a pairing.
     */
    bool poll();

    //! Is the channel currently delivering motion data? False for a channel that is merely open, or
    //! paired but not yet sending.
    bool is_streaming() const;

    //! The channel's actuated joints, discovered on the first frame. Empty before then.
    const std::vector<ActuatedJoint>& actuated_joints() const
    {
        return actuated_joints_;
    }

    //! Joint angle about its own rotation axis [rad], as the last frame reported it.
    float angle(const ActuatedJoint& joint) const;

    //! Joint angular velocity [rad/s], as the last frame reported it.
    float angular_velocity(const ActuatedJoint& joint) const;

    //! Sequence id of the frame currently being reported. Zero before the first frame.
    uint32_t sequence_id() const
    {
        return sequence_id_;
    }

    //! Frames the Player sent that never reached us, counted from sequence id gaps.
    int64_t dropped_frames() const
    {
        return dropped_frames_;
    }

    int channel() const
    {
        return channel_;
    }

private:
    //! Read the channel's joint table and keep the revolute ones. Idempotent per pairing.
    void discover_joints();

    int channel_;
    uint32_t sequence_id_ = 0;
    int64_t dropped_frames_ = 0;
    bool joints_discovered_ = false;
    std::vector<ActuatedJoint> actuated_joints_;
};

} // namespace jmex
} // namespace plugins
