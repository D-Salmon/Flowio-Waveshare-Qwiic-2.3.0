#pragma once

#include "Board/FlowIODINBoards.h"

namespace BoardCapacityProfile {

inline constexpr const BoardSpec& buildBoard()
{
    return BoardProfiles::kFlowIOS3;
}

inline constexpr IoCapacitySpec kIoCapacity = buildBoard().ioCapacity;
inline constexpr MqttCapacitySpec kMqttCapacity = buildBoard().mqttCapacity;
inline constexpr MqttBufferSpec kMqttBuffers = buildBoard().mqttBuffers;
inline constexpr HaCapacitySpec kHaCapacity = buildBoard().haCapacity;

}  // namespace BoardCapacityProfile
