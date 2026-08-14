// SPDX-FileCopyrightText: Copyright (c) 2026 j-mex. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

/*!
 * @file main.cpp
 * @brief j-mex AgileMaster device plugin: streams a MOXI channel's robot joint angles as
 *        ``JointStateOutput`` over the OpenXR tensor transport.
 *
 * Usage: ``jmex_plugin [channel] [collection_id]`` (defaults: 255, "jmex"). There is no synthetic
 * backend: building needs the MOXI Receiver SDK and running needs MOXI Player. See README.md.
 */

#include <jmex/joint_state_publisher.hpp>
#include <jmex/moxi_session.hpp>
#include <oxr/oxr_session.hpp>
#include <pusherio/schema_pusher.hpp>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

using namespace plugins::jmex;

namespace
{

// MOXI Player runs at 60 Hz. Polling several times faster keeps the delay between a frame landing
// and it being published well under one frame; nothing is published twice.
constexpr int kPollHz = 240;

} // namespace

int main(int argc, char** argv)
try
{
    const int channel = (argc > 1) ? std::atoi(argv[1]) : 255;
    const std::string collection_id = (argc > 2) ? argv[2] : "jmex";

    std::cout << "j-mex AgileMaster plugin (channel: " << channel << ", collection: " << collection_id
              << ", SDK: " << moxi_sdk_version() << ")" << std::endl;

    // OpenXR first: a missing runtime is the common setup mistake, and failing on it before the
    // MOXI receiver starts means we never take the process-global TCP port just to give it back.
    auto oxr_session = std::make_shared<core::OpenXRSession>("JmexPlugin", core::SchemaPusher::get_required_extensions());
    JointStatePublisher publisher(oxr_session->get_handles(), collection_id);

    // The receiver is the TCP server; MOXI Player connects in. Starting before the Player is up is
    // the normal case, not an error.
    MoxiSession moxi(channel);

    std::cout << "Waiting for MOXI Player to pair on channel " << channel << "..." << std::endl;

    const auto poll_interval = std::chrono::nanoseconds(1000000000 / kPollHz);
    const auto program_start = std::chrono::steady_clock::now();
    int64_t poll_count = 0;
    int64_t published = 0;

    while (true)
    {
        if (moxi.poll())
        {
            publisher.publish(moxi);
            if (published == 0)
            {
                std::cout << "Streaming: " << moxi.actuated_joints().size() << " joints on collection '"
                          << collection_id << "'" << std::endl;
            }
            ++published;
        }
        ++poll_count;
        std::this_thread::sleep_until(program_start + poll_interval * poll_count);
    }

    return 0;
}
catch (const std::exception& e)
{
    std::cerr << argv[0] << ": " << e.what() << std::endl;
    return 1;
}
catch (...)
{
    std::cerr << argv[0] << ": Unknown error" << std::endl;
    return 1;
}
