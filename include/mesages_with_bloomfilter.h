// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <queue>
#include <unordered_map>
#include <memory>

#include "xpbase/base/uint64_bloomfilter.h"
#include "xpbase/base/top_timer.h"
#include "xtransport/proto/transport.pb.h"

namespace top {

namespace gossip {

struct GossipItem {
    std::shared_ptr<base::Uint64BloomFilter> bloom_filter;
    uint32_t sendout_times;
};

class MessageWithBloomfilter {
public:
    static MessageWithBloomfilter* Instance();
    std::shared_ptr<base::Uint64BloomFilter> GetMessageBloomfilter(
            transport::protobuf::RoutingMessage& message,
            bool& stop_gossip);
    bool StopGossip(const uint32_t&, uint32_t);

private:
    MessageWithBloomfilter() {}
    ~MessageWithBloomfilter() {}

    std::shared_ptr<base::Uint64BloomFilter> MergeBloomfilter(
            const uint32_t&,
            std::shared_ptr<base::Uint64BloomFilter>& bloomfilter,
            const uint32_t& stop_times,
            bool& stop_gossip);

    // static const uint32_t kMaxMessageQueueSize = 1048576u;
    static const uint32_t kMaxMessageQueueSize = 508576u;

    std::unordered_map<uint32_t, uint8_t> messsage_bloomfilter_map_;
    std::mutex messsage_bloomfilter_map_mutex_;

    DISALLOW_COPY_AND_ASSIGN(MessageWithBloomfilter);
};

}  // namespace gossip

}  // namespace top
