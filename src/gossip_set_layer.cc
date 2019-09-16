// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xgossip/include/gossip_set_layer.h"

#include <unordered_set>

#include "xbase/xhash.h"
#include "xbase/xcontext.h"
#include "xbase/xbase.h"
#include "xbasic/xhash.hpp"
#include "xpbase/base/top_log.h"
#include "xpbase/base/top_utils.h"
#include "xpbase/base/uint64_bloomfilter.h"
#include "xpbase/base/redis_client.h"
#include "xgossip/include/gossip_utils.h"
#include "xgossip/include/mesages_with_bloomfilter.h"
#include "xgossip/include/block_sync_manager.h"
#include "xpbase/base/redis_utils.h"

namespace top {

namespace gossip {

GossipSetLayer::GossipSetLayer(transport::TransportPtr transport_ptr)
        : GossipInterface(transport_ptr) {}

GossipSetLayer::~GossipSetLayer() {}

void GossipSetLayer::Broadcast(
        uint64_t local_hash64,
        transport::protobuf::RoutingMessage& message,
        std::shared_ptr<std::vector<kadmlia::NodeInfoPtr>> prt_neighbors) {
    BlockSyncManager::Instance()->NewBroadcastMessage(message);
    if (message.gossip().max_hop_num() > 0 &&
            message.gossip().max_hop_num() <= message.hop_num()) {
        TOP_WARN("message.type(%d) hop_num(%d) larger than gossip_max_hop_num(%d)",
                message.type(),
                message.hop_num(),
                message.gossip().max_hop_num());
        return;
    }

    bool stop_gossip = false;
    auto hash64 = base::xhash64_t::digest(message.xid() + std::to_string(message.id()));
    auto bloomfilter = MessageWithBloomfilter::Instance()->StopGossip(
            hash64,
            message.gossip().stop_times());
    if (stop_gossip) {
        TOP_DEBUG("stop gossip for message.type(%d)", message.type());
        return;
    }

    auto gossip_param = message.mutable_gossip();
    std::unordered_set<uint32_t> passed_set(32);
    for (int i = 0; i < message.gossip().pass_node_size(); ++i) {
        passed_set.insert(message.gossip().pass_node(i));
    }

    std::vector<kadmlia::NodeInfoPtr> tmp_neighbors;
    
    for (auto iter = prt_neighbors->begin(); iter != prt_neighbors->end(); ++iter) {
        if ((*iter)->xid.empty()) {
            continue;
        }

        if (passed_set.find(static_cast<uint32_t>((*iter)->hash64)) != passed_set.end()) {
#ifdef TOP_TESTING_PERFORMANCE
            uint32_t filtered = 0;
            ++filtered;
            TOP_NETWORK_DEBUG_FOR_PROTOMESSAGE(
                    std::string("message filterd: ") + (*iter)->public_ip +
                    ":" + std::to_string((*iter)->public_port), message);
#endif
            continue;
        }
        tmp_neighbors.push_back(*iter);
    }
    std::random_shuffle(tmp_neighbors.begin(), tmp_neighbors.end());

    if (passed_set.find(static_cast<uint32_t>(local_hash64)) == passed_set.end()) {
        gossip_param->add_pass_node(static_cast<uint32_t>(local_hash64));
    }
#ifdef TOP_TESTING_PERFORMANCE
    std::string goed_ids;
    for (int i = 0; i < message.hop_nodes_size(); ++i) {
        goed_ids += HexEncode(message.hop_nodes(i).node_id()) + ",";
    }
    std::string data = base::StringUtil::str_fmt(
            "GossipSetLayer Broadcast tmp_neighbors size %d,"
            "filtered %d nodes [hop num: %d][%s]",
            tmp_neighbors.size(),
            filtered,
            message.hop_num(),
            goed_ids.c_str());
    TOP_NETWORK_DEBUG_FOR_PROTOMESSAGE(data, message);
#endif
    std::vector<kadmlia::NodeInfoPtr> rest_random_neighbors;
    std::vector<uint64_t> dis_vec;
    if (message.hop_num() >= message.gossip().switch_layer_hop_num()) {
        SelectNodes(message, tmp_neighbors, rest_random_neighbors);
    } else {
        rest_random_neighbors = GetRandomNodes(tmp_neighbors, GetNeighborCount(message));
    }

    for (auto iter = rest_random_neighbors.begin(); iter != rest_random_neighbors.end(); ++iter) {
        if (passed_set.find(static_cast<uint32_t>((*iter)->hash64)) == passed_set.end()) {
            gossip_param->add_pass_node(static_cast<uint32_t>((*iter)->hash64));
        }
    }

    if (message.hop_num() >= message.gossip().switch_layer_hop_num()) {
        SendLayered(message, rest_random_neighbors);
    } else {
        Send(message, rest_random_neighbors);
    }
}

}  // namespace gossip

}  // namespace top
