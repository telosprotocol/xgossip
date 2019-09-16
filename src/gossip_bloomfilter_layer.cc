// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xgossip/include/gossip_bloomfilter_layer.h"

#include "xbase/xhash.h"
#include "xbase/xcontext.h"
#include "xbase/xbase.h"
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

GossipBloomfilterLayer::GossipBloomfilterLayer(transport::TransportPtr transport_ptr)
        : GossipInterface(transport_ptr) {}

GossipBloomfilterLayer::~GossipBloomfilterLayer() {}

void GossipBloomfilterLayer::Broadcast(
        uint64_t local_hash64,
        transport::protobuf::RoutingMessage& message,
        std::shared_ptr<std::vector<kadmlia::NodeInfoPtr>> prt_neighbors) {
    /*
    CheckDiffNetwork(message);
    BlockSyncManager::Instance()->NewBroadcastMessage(message);
    if (message.gossip().max_hop_num() > 0 &&
            message.gossip().max_hop_num() <= message.hop_num()) {
        TOP_WARN2("message.type(%d) hop_num(%d) larger than gossip_max_hop_num(%d)",
                message.type(),
                message.hop_num(),
                message.gossip().max_hop_num());
        return;
    }

    bool stop_gossip = false;
    auto bloomfilter = MessageWithBloomfilter::Instance()->GetMessageBloomfilter(
            message,
            stop_gossip);
    if (stop_gossip) {
        TOP_DEBUG("stop gossip for message.type(%d) hop_num(%d)", message.type(), message.hop_num());
        return;
    }

    assert(bloomfilter);
    if (!bloomfilter) {
        TOP_WARN2("bloomfilter invalid");
        return;
    }
    std::vector<kadmlia::NodeInfoPtr> tmp_neighbors;
    
    auto gossip_param = message.mutable_gossip();
    uint32_t filtered = 0;
    for (auto iter = prt_neighbors->begin(); iter != prt_neighbors->end(); ++iter) {
        if (!IsIpValid((*iter)->public_ip)) {
            continue;
        }

        if ((*iter)->hash64 == 0) {
            continue;
        }

        if (bloomfilter->Contain((*iter)->hash64)) {
            ++filtered;
#ifdef TOP_TESTING_PERFORMANCE
            TOP_NETWORK_DEBUG_FOR_PROTOMESSAGE(
                    std::string("message filterd: ") + (*iter)->public_ip +
                    ":" + std::to_string((*iter)->public_port), message);
#endif
            continue;
        }

        if ((*iter)->public_ip == gossip_param->pre_ip() &&
                (*iter)->public_port == gossip_param->pre_port()) {
            if (message.hop_num() > message.gossip().ign_bloomfilter_level()) {
                bloomfilter->Add((*iter)->hash64);
            }
            continue;
        }
        tmp_neighbors.push_back(*iter);
    }

    TOP_DEBUG("GossipBloomfilterLayer Broadcast tmp_neighbors size %d, filtered %d nodes",
            tmp_neighbors.size(),
            filtered);
    std::random_shuffle(tmp_neighbors.begin(), tmp_neighbors.end());

    if (message.hop_num() > message.gossip().ign_bloomfilter_level()) {
        bloomfilter->Add(local_hash64);
    }
#ifdef TOP_TESTING_PERFORMANCE
    std::string goed_ids;
    for (uint32_t i = 0; i < message.hop_nodes_size(); ++i) {
        goed_ids += HexEncode(message.hop_nodes(i).node_id()) + ",";
    }
    std::string data = base::StringUtil::str_fmt(
            "GossipBloomfilterLayer Broadcast tmp_neighbors size %d,"
            "filtered %d nodes [hop num: %d][%s][%s]",
            tmp_neighbors.size(),
            filtered,
            message.hop_num(),
            goed_ids.c_str(),
            bloomfilter->string().c_str());
    TOP_NETWORK_DEBUG_FOR_PROTOMESSAGE(data, message);
#endif
    std::vector<kadmlia::NodeInfoPtr> rest_random_neighbors;
    if (message.hop_num() >= message.gossip().switch_layer_hop_num()) {
        SelectNodes(message, tmp_neighbors, rest_random_neighbors);
    } else {
        rest_random_neighbors = GetRandomNodes(tmp_neighbors, GetNeighborCount(message));
    }

    if ((message.hop_num() + 1) > message.gossip().ign_bloomfilter_level()) {
        for (auto iter = rest_random_neighbors.begin();
                iter != rest_random_neighbors.end(); ++iter) {
            bloomfilter->Add((*iter)->hash64);
        }
    }
    const std::vector<uint64_t>& bloomfilter_vec = bloomfilter->Uint64Vector();
    message.clear_bloomfilter();
    for (uint32_t i = 0; i < bloomfilter_vec.size(); ++i) {
        message.add_bloomfilter(bloomfilter_vec[i]);
    }
    if (rest_random_neighbors.empty()) {
        TOP_WARN2("rest_random_neighbors empty, broadcast failed");
        return;
    }

    gossip_param->clear_pre_ip();
    gossip_param->clear_pre_port();
    if (message.hop_num() >= message.gossip().switch_layer_hop_num()) {
        SendLayered(message, rest_random_neighbors);
    } else {
        Send(message, rest_random_neighbors);
    }
    */
}

void GossipBloomfilterLayer::Broadcast(
        transport::protobuf::RoutingMessage& message,
        kadmlia::RoutingTablePtr& routing_table) {
    CheckDiffNetwork(message);
    BlockSyncManager::Instance()->NewBroadcastMessage(message);
    if (message.gossip().max_hop_num() > 0 &&
            message.gossip().max_hop_num() <= message.hop_num()) {
        TOP_WARN2("message.type(%d) hop_num(%d) larger than gossip_max_hop_num(%d)",
                message.type(),
                message.hop_num(),
                message.gossip().max_hop_num());
        return;
    }

    bool stop_gossip = false;
    auto bloomfilter = MessageWithBloomfilter::Instance()->GetMessageBloomfilter(
            message,
            stop_gossip);
    if (stop_gossip) {
        TOP_DEBUG("stop gossip for message.type(%d) hop_num(%d)", message.type(), message.hop_num());
        return;
    }

    assert(bloomfilter);
    if (!bloomfilter) {
        TOP_WARN2("bloomfilter invalid");
        return;
    }

    if (message.hop_num() >= message.gossip().ign_bloomfilter_level()) {
        bloomfilter->Add(routing_table->get_local_node_info()->hash64());
    }

    std::vector<kadmlia::NodeInfoPtr> select_nodes;
    SelectNodes(message, routing_table, bloomfilter, select_nodes);
    if (select_nodes.empty()) {
        TOP_WARN2("stop broadcast, select_nodes empty, msg.hop_num(%d), msg.type(%d)",
                message.hop_num(),
                message.type());
        return;
    }

#ifdef TOP_TESTING_PERFORMANCE
    std::string goed_ids;
    for (uint32_t i = 0; i < message.hop_nodes_size(); ++i) {
        goed_ids += HexEncode(message.hop_nodes(i).node_id()) + ",";
    }
    std::string data = base::StringUtil::str_fmt(
            "GossipBloomfilterLayer Broadcast select_node size %d,"
            "filtered %d nodes [hop num: %d][%s][%s]",
            select_nodes.size(),
            filtered,
            message.hop_num(),
            goed_ids.c_str(),
            bloomfilter->string().c_str());
    TOP_NETWORK_DEBUG_FOR_PROTOMESSAGE(data, message);
#endif

    if ((message.hop_num() + 1) > message.gossip().ign_bloomfilter_level()) {
        for (auto iter = select_nodes.begin();
                iter != select_nodes.end(); ++iter) {
            bloomfilter->Add((*iter)->hash64);
        }
    }
    const std::vector<uint64_t>& bloomfilter_vec = bloomfilter->Uint64Vector();
    message.clear_bloomfilter();
    for (uint32_t i = 0; i < bloomfilter_vec.size(); ++i) {
        message.add_bloomfilter(bloomfilter_vec[i]);
    }

    if (message.hop_num() >= message.gossip().switch_layer_hop_num()) {
        SendLayered(message, select_nodes);
    } else {
        Send(message, select_nodes);
    }
}

}  // namespace gossip

}  // namespace top
