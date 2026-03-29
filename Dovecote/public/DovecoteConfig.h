// Source file name: DovecoteConfig.h
// Author: Igor Matiushin
// Brief description: Defines standalone relay configuration defaults and interaction timing constants.

#pragma once

#include <cstddef>
#include <cstdint>

namespace DovecoteConfig {

constexpr uint16_t Port             = 7777;
constexpr size_t   MaxPeers         = 128;
constexpr size_t   ChannelCount     = 2;
constexpr uint32_t ServiceTimeoutMs = 500;
constexpr float GridCellSize = 1000.f;

constexpr float InteractionEventTtlSeconds = 1.f;
constexpr float InteractionConfirmWindowSeconds = 1.0f;
constexpr float InteractionRejectAfterSeconds = 0.85f;

}
