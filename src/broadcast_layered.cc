// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xgossip/include/broadcast_layered.h"

#include <algorithm>

#include "xpbase/base/redis_client.h"
#include "xkad/routing_table/routing_table.h"
#include "xgossip/include/gossip_utils.h"
#include "xgossip/include/block_sync_manager.h"

namespace top {

using namespace kadmlia;

namespace gossip {

BroadcastLayered::BroadcastLayered(transport::TransportPtr transport_ptr)
        : GossipInterface(transport_ptr) {}

BroadcastLayered::~BroadcastLayered() {}

void BroadcastLayered::Broadcast(
        transport::protobuf::RoutingMessage& message,
        kadmlia::RoutingTablePtr& routing_table) {
    if (message.hop_num() >= kadmlia::kHopToLive) {
        TOP_WARN2("message hop_num(%d) beyond max_hop", message.hop_num());
        return;
    }

    if (ThisNodeIsEvil(message)) {
        TOP_WARN2("this node(%s) is evil", HexEncode(global_xid->Get()).c_str());
        return;
    }
    BlockSyncManager::Instance()->NewBroadcastMessage(message);

    std::vector<kadmlia::NodeInfoPtr> broadcast_nodes;
    GetNextBroadcastNodes(
            message,
            routing_table,
            broadcast_nodes);
    Send(message, broadcast_nodes);
}

uint32_t BroadcastLayered::FindLogicalSelfIndex(
        uint32_t real_self_index,
        uint32_t random_step,
        uint32_t nodes_size) {
    assert(real_self_index > 0 && real_self_index <= nodes_size);
    uint32_t logical_self_index = (real_self_index + random_step) % nodes_size;
    logical_self_index = (logical_self_index == 0)?nodes_size:logical_self_index;
    return logical_self_index;
}

uint32_t BroadcastLayered::FindRealSelfIndex(
        uint32_t logical_self_index,
        uint32_t random_step,
        uint32_t nodes_size) {
    assert(logical_self_index > 0 && logical_self_index <= nodes_size);
    if (logical_self_index <= random_step) {
        return (logical_self_index + nodes_size) - random_step;
    } else {
        return logical_self_index - random_step;
    }
}

void BroadcastLayered::GetRangeNodes(
        kadmlia::RoutingTablePtr& routing_table,
        const uint32_t& min_index,
        const uint32_t& max_index,
        std::vector<kadmlia::NodeInfoPtr>& vec) {
    uint32_t nodes_size = routing_table->nodes_size() + 1;
    if (min_index <= max_index) {
        return routing_table->GetRangeNodes(min_index, max_index, vec);
    }
    // may be on a ring loop
    routing_table->GetRangeNodes(0, max_index, vec);
    routing_table->GetRangeNodes(min_index, nodes_size - 1, vec);
    return;
}

void BroadcastLayered::GetNextBroadcastNodes(
        transport::protobuf::RoutingMessage& message,
        kadmlia::RoutingTablePtr& routing_table,
        std::vector<kadmlia::NodeInfoPtr>& next_broadcast_nodes) {
    uint32_t nodes_size = routing_table->nodes_size() + 1; // including self
    //uint32_t random_step = 0;
    uint32_t random_step = message.id() % nodes_size; // keep same random_step when this broadcast message alive
    int32_t real_self_index = FindSelfSortIndex(message, routing_table);
    if (real_self_index == -1) {
        TOP_WARN2("real_self_index: -1 error");
        //std::cout << "real_self_index: -1 error" << std::endl;
        return;
    }
    real_self_index = static_cast<uint32_t>(real_self_index);
    //std::cout << "real_self_index:" << real_self_index <<" random_step:" << random_step << " nodes_size:" << nodes_size << " msgid:" << message.id() << std::endl;
    if (real_self_index <= 0 || real_self_index > nodes_size) {
        TOP_WARN2("real_self_index invalid, layerbroadcast failed");
        return;
    }

    uint32_t logical_self_index = FindLogicalSelfIndex(real_self_index, random_step, nodes_size);
    //std::cout << "logically self_index:" << logical_self_index << std::endl;

    uint32_t neighber_count = GetNeighborCount(message);
    if (message.hop_num() == 0) {
        // meaning this is the first broadcast packet, should choose the first K nodes
        uint32_t min_real_index = FindRealSelfIndex(1, random_step, nodes_size) - 1;
        uint32_t max_real_index = FindRealSelfIndex(neighber_count, random_step, nodes_size) - 1;
        //std::cout << "first min_real_index:" << min_real_index << " max_real_index:" << max_real_index << std::endl;
        GetRangeNodes(routing_table, min_real_index, max_real_index, next_broadcast_nodes);
        TOP_DEBUG("the first nodes of layerbroadcast");
        //std::cout << "first: next_broadcast_nodes size: " << next_broadcast_nodes.size() << std::endl;
        if (logical_self_index > neighber_count) {
            return;
        }
    }

    // base self_sort_index choose next nodes sort_index
    auto logical_result = choose_next_nodes(nodes_size, logical_self_index, neighber_count);
    if (logical_result.empty()) {
        return;
    }
    /*
    for (auto& item: logical_result) {
        std::cout << "item:" << item << "\t";
    }
    std::cout << std::endl;
    */

    // index start from 1 to nodes_size
    
    uint32_t real_min_index = FindRealSelfIndex(logical_result[0], random_step, nodes_size);
    uint32_t real_max_index = FindRealSelfIndex(logical_result[logical_result.size() - 1], random_step, nodes_size);

    real_min_index -= 1;
    real_max_index -= 1;

    //std::cout << "real_min_index: " << real_min_index << " real_max_index:" << real_max_index << std::endl;

    // attention the boundary value
    GetRangeNodes(routing_table, real_min_index, real_max_index, next_broadcast_nodes);
    //std::cout << "next_broadcast_nodes size:" << next_broadcast_nodes.size() << std::endl;
    return;
}

int32_t BroadcastLayered::FindSelfSortIndex(
        transport::protobuf::RoutingMessage& message,
        kadmlia::RoutingTablePtr& routing_table) {
    // start from 1
    int32_t index = routing_table->GetSelfIndex();
    if (index == -1) {
        return -1;
    }
    return index + 1;
}

std::vector<uint32_t> BroadcastLayered::choose_nodes(
        int hop_num,
        uint32_t self_sort_index,
        uint32_t neighber_count,
        const std::vector<uint32_t>& sort_nodes_index_vec) {
    uint32_t sum1 = 0;
    uint32_t K = neighber_count;
    for (int i = 1; i < hop_num; ++i) {
        // max i is hop_num -1
        sum1 += pow(K, i);
    }
    uint32_t sum2 = sum1 + pow(K, hop_num);
    if (self_sort_index <= sum1 || self_sort_index > sum2) {
        // not right
        return {};
    }

    uint32_t front_level_size = self_sort_index - sum1 - 1;
    uint32_t begin  =  sum2 + front_level_size * K + 1;
    uint32_t end = begin + K;
    std::vector<uint32_t> result;
    for ( uint32_t i = begin; i < end; ++i) {
        if (i > sort_nodes_index_vec.size()) {
            break;
        }
        result.push_back(sort_nodes_index_vec[i-1]);
    }
    return result;
}

uint32_t BroadcastLayered::find_hop_num(uint32_t self_sort_index, uint32_t neighber_count) {
    uint32_t K = neighber_count;
    uint32_t sum1 = 0;
    uint32_t sum2 = 0;
    uint32_t hop_num = 0;
    for (uint32_t i = 0; ; ++i) {
        sum1 += pow(K, i);
        if (i == 0) {
            sum1 = 0;
        }
        sum2 = sum1 + pow(K, i+1);
        if (self_sort_index > sum1 && self_sort_index <= sum2) {
            hop_num = i + 1;
            break;
        }
    }
    return hop_num;
}

std::vector<uint32_t> BroadcastLayered::choose_next_nodes(
        uint32_t count,
        uint32_t self_node_index,
        uint32_t neighber_count) {
    if (self_node_index > count) {
        return {};
    }
    std::vector<uint32_t> sort_nodes_index_vec;
    // 1~64
    for (uint32_t i = 0; i < count; ++i) {
        sort_nodes_index_vec.push_back(i+1);
    }
    uint32_t hop_num = find_hop_num(self_node_index, neighber_count);
    TOP_DEBUG("find_hop_num:%d self_index:%d", hop_num, self_node_index);
    //std::cout << "find_hop:" << hop_num << std::endl;

    auto result = choose_nodes(hop_num, self_node_index, neighber_count, sort_nodes_index_vec);
    return result;
}


}  // namespace gossip

}  // namespace top
