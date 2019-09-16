// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <memory>

#include "xtransport/transport.h"
#include "xkad/routing_table/node_info.h"
#include "xgossip/gossip_interface.h"

namespace top {

namespace kadmlia {
class RoutingTable;
typedef std::shared_ptr<RoutingTable> RoutingTablePtr;
};

namespace gossip {

class BroadcastLayered : public GossipInterface {
public:
    explicit BroadcastLayered(transport::TransportPtr transport_ptr);
    virtual ~BroadcastLayered();
    virtual void Broadcast(
            uint64_t local_hash64,
            transport::protobuf::RoutingMessage& message,
            std::shared_ptr<std::vector<kadmlia::NodeInfoPtr>> neighbors) { return; }

    virtual void Broadcast(
            transport::protobuf::RoutingMessage& message,
            kadmlia::RoutingTablePtr& routing_table);

private:
    void GetNextBroadcastNodes(
            transport::protobuf::RoutingMessage& message,
            kadmlia::RoutingTablePtr& routing_table,
            std::vector<kadmlia::NodeInfoPtr>& broadcast_nodes);
    int32_t FindSelfSortIndex(
            transport::protobuf::RoutingMessage& message,
            kadmlia::RoutingTablePtr& routing_table);
    std::vector<uint32_t> choose_nodes(
            int hop_num,
            uint32_t self_sort_index,
            uint32_t neighber_count,
            const std::vector<uint32_t>& sort_nodes_index_vec);
    uint32_t find_hop_num(uint32_t self_sort_index, uint32_t neighber_count);
    std::vector<uint32_t> choose_next_nodes(
            uint32_t count,
            uint32_t self_node_index,
            uint32_t neighber_count);
    uint32_t FindLogicalSelfIndex(
            uint32_t real_self_index,
            uint32_t random_step,
            uint32_t nodes_size);
    uint32_t FindRealSelfIndex(
            uint32_t logical_self_index,
            uint32_t random_step,
            uint32_t nodes_size);
    void GetRangeNodes(
            kadmlia::RoutingTablePtr& routing_table,
            const uint32_t& min_index,
            const uint32_t& max_index,
            std::vector<kadmlia::NodeInfoPtr>& vec);
 

};

}  // namespace gossip

}  // namespace top
