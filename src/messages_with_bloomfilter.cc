// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <queue>
#include <set>
#include <mutex>

#include "xbase/xpacket.h"
#include "xtransport/proto/transport.pb.h"
#include "xpbase/base/top_timer.h"
#include "xgossip/include/header_block_data.h"
#include "xtransport/message_manager/message_manager_intf.h"

namespace top {

namespace kadmlia {

class RoutingTable;
typedef std::shared_ptr<RoutingTable> RoutingTablePtr;

}

namespace gossip {

struct SyncBlockItem {
    uint64_t routing_service_type;
    std::string header_hash;
    std::chrono::steady_clock::time_point time_point;
};

class BlockSyncManager {
public:
    static BlockSyncManager* Instance();
    void SetLeagerFace(std::shared_ptr<top::ledger::xledger_face_t> ledger_face);
    void SetRoutingTablePtr(kadmlia::RoutingTablePtr& routing_table);
    void NewBroadcastMessage(transport::protobuf::RoutingMessage& message);

private:
    BlockSyncManager();
    ~BlockSyncManager();

#include "xgossip/include/mesages_with_bloomfilter.h"

#include "xbase/xhash.h"
#include "xgossip/include/gossip_utils.h"
#include "xpbase/base/top_log.h"

namespace top {

namespace gossip {

MessageWithBloomfilter* MessageWithBloomfilter::Instance() {
    static MessageWithBloomfilter ins;
    return &ins;
}

std::shared_ptr<base::Uint64BloomFilter> MessageWithBloomfilter::GetMessageBloomfilter(
        transport::protobuf::RoutingMessage& message,
        bool& stop_gossip) {
    auto hash32 = message.gossip().msg_hash();
    if (StopGossip(hash32, message.gossip().stop_times())) {
        stop_gossip = true;
        return nullptr;
    }

    std::vector<uint64_t> new_bloomfilter_vec;
    for (auto i = 0; i < message.bloomfilter_size(); ++i) {
        new_bloomfilter_vec.push_back(message.bloomfilter(i));
    }

    std::shared_ptr<base::Uint64BloomFilter> new_bloomfilter;
    if (new_bloomfilter_vec.empty()) {
        // readonly, mark static can improve performance
        static std::vector<uint64_t> construct_vec(gossip::kGossipBloomfilterSize / 64, 0ull);
        new_bloomfilter = std::make_shared<base::Uint64BloomFilter>(
                construct_vec,
                gossip::kGossipBloomfilterHashNum);
    } else {
        new_bloomfilter = std::make_shared<base::Uint64BloomFilter>(
                new_bloomfilter_vec,
                gossip::kGossipBloomfilterHashNum);
    }
    // (Charlie): avoid evil
    // MergeBloomfilter(hash32, new_bloomfilter, message.gossip().stop_times(), stop_gossip);
    return new_bloomfilter;
}

bool MessageWithBloomfilter::StopGossip(const uint32_t& gossip_key, uint32_t stop_times) {
    if (stop_times <= 0) {
        stop_times = kGossipSendoutMaxTimes;
    }
    std::unique_lock<std::mutex> lock(messsage_bloomfilter_map_mutex_);
    auto iter = messsage_bloomfilter_map_.find(gossip_key);
    if (iter != messsage_bloomfilter_map_.end()) {
        TOP_DEBUG("msg.hash:%d stop_times:%d", gossip_key, iter->second);
        if (iter->second >= stop_times) {
            return true;
        }
        ++(iter->second);
    } else {
        messsage_bloomfilter_map_[gossip_key] = 1;
    }


    // (Charlie): avoid memory crash
    if (messsage_bloomfilter_map_.size() >= kMaxMessageQueueSize) {
        messsage_bloomfilter_map_.clear();
    }
    return false;
}

}  // namespace gossip

}  // namespace top
