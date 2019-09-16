// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xgossip/include/gossip_utils.h"

namespace top {

namespace gossip {

static const uint32_t kReliableHighNeighbersCount = 8u;
static const uint32_t kReliableMiddleNeighbersCount = 5u;
static const uint32_t kReliableLowNeighbersCount = 3u;

uint32_t GetRandomNeighbersCount(uint32_t reliable_level) {
    switch (reliable_level) {
    case kGossipReliableInvalid:
        return kReliableHighNeighbersCount;
    case kGossipReliableHigh:
        return kReliableHighNeighbersCount;
    case kGossipReliableMiddle:
        return kReliableMiddleNeighbersCount;
    case kGossipReliableLow:
        return kReliableLowNeighbersCount;
    default:
        return kReliableLowNeighbersCount;
    }
}

}  // namespace gossip

}  // namespace top
