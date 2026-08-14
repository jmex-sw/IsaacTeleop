// SPDX-FileCopyrightText: Copyright (c) 2026 j-mex. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#include "inc/jmex/joint_state_publisher.hpp"

#include "inc/jmex/moxi_session.hpp"

#include <flatbuffers/flatbuffers.h>
#include <oxr_utils/os_time.hpp>
#include <schema/joint_state_generated.h>

#include <iostream>
#include <memory>

namespace plugins
{
namespace jmex
{

JointStatePublisher::JointStatePublisher(const core::OpenXRSessionHandles& handles, const std::string& collection_id)
    : collection_id_(collection_id),
      pusher_(handles,
              core::SchemaPusherConfig{ .collection_id = collection_id,
                                        .max_flatbuffer_size = MAX_FLATBUFFER_SIZE,
                                        // Must match what JointStateTracker reads.
                                        .tensor_identifier = "joint_state",
                                        .localized_name = "MOXI Motion Capture",
                                        .app_name = "JmexPlugin" })
{
}

void JointStatePublisher::publish(const MoxiSession& moxi)
{
    core::JointStateOutputT out;
    out.device_id = collection_id_;
    out.has_velocity = true; // the SDK reports angular velocity per joint at no extra cost
    out.has_effort = false;
    out.ee_pose_valid = false; // no device-side FK; the retargeter computes it when it needs it

    for (const auto& joint : moxi.actuated_joints())
    {
        auto entry = std::make_shared<core::JointStateT>();
        entry->name = joint.name;
        entry->position = moxi.angle(joint);
        entry->velocity = moxi.angular_velocity(joint);
        entry->valid = true;
        out.joints.push_back(std::move(entry));
    }

    // MOXI carries no device clock -- the sequence id is a counter, not a timestamp -- so the local
    // common clock is the only real one we have and it stands in for both.
    const auto sample_time_ns = core::os_monotonic_now_ns();

    flatbuffers::FlatBufferBuilder builder(MAX_FLATBUFFER_SIZE);
    auto offset = core::JointStateOutput::Pack(builder, &out);
    builder.Finish(offset);

    if (builder.GetSize() > MAX_FLATBUFFER_SIZE)
    {
        // Silently dropping here would look exactly like "the Player stopped sending", so say what
        // happened and what the operator can do about it.
        std::cerr << "jmex: serialized JointStateOutput is " << builder.GetSize() << " bytes, over the "
                  << MAX_FLATBUFFER_SIZE << "-byte budget shared with the consumer's JointStateTracker ("
                  << out.joints.size() << " joints). Sample dropped." << std::endl;
        return;
    }

    pusher_.push_buffer(builder.GetBufferPointer(), builder.GetSize(), sample_time_ns, sample_time_ns);
}

} // namespace jmex
} // namespace plugins
