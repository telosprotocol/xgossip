// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <stdint.h>
#include <string>

#include "xkad/routing_table/routing_utils.h"

namespace top {

namespace gossip {

enum ReliableLevel {
    kGossipReliableInvalid = 0,
    kGossipReliableHigh = 1,
    kGossipReliableMiddle = 2,
    kGossipReliableLow = 3,
};

enum GossipType {
    kGossipInvalid = 0,
    kGossipBloomfilter = 1,
    kGossipLayeredBroadcast = 2,
    kGossipBloomfilterAndLayered = 3,
    kGossipSetFilterAndLayered = 4,
};

/*
enum GossipBlockSyncType {
    kGossipBlockSyncInvalid = kadmlia::kKadMessageTypeMax + 1,
    kGossipBlockSyncAsk,
    kGossipBlockSyncAck,
    kGossipBlockSyncRequest,
    kGossipBlockSyncResponse,

    kGossipMaxMessageType,
};
*/

static const uint32_t kGossipSendoutMaxTimes = 3u;
static const uint32_t kGossipSendoutMaxNeighbors = 3u;
static const uint32_t kGossipSwitchLayerCount = 2u;
static const uint32_t kGossipBloomfilterSize = 256u;
static const uint32_t kGossipBloomfilterHashNum = 3u;
static const uint32_t kGossipBloomfilterIgnoreLevel = 1u;

uint32_t GetRandomNeighbersCount(uint32_t reliable_level);

}  // namespace gossip

}  // namespace top
