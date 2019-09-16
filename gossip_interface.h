// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <stdint.h>

#include "xbase/xpacket.h"
#include "xtransport/proto/transport.pb.h"
#include "xtransport/transport.h"
#include "xkad/routing_table/node_info.h"
#include "xkad/routing_table/routing_table.h"

namespace top {

namespace  base {
class Uint64BloomFilter;
};

namespace gossip {

class GossipInterface {
public:
    virtual void Broadcast(
            uint64_t local_hash64,
            transport::protobuf::RoutingMessage& message,
            std::shared_ptr<std::vector<kadmlia::NodeInfoPtr>> neighbors) = 0;
    virtual void Broadcast(
            transport::protobuf::RoutingMessage& message,
            kadmlia::RoutingTablePtr& routing_table) {
        return ;
    }

protected:
    GossipInterface(transport::TransportPtr transport_ptr) : transport_ptr_(transport_ptr) {}
    virtual ~GossipInterface() {}

    uint64_t GetDistance(const std::string& src, const std::string& des);
    void Send(
            transport::protobuf::RoutingMessage& message,
            const std::vector<kadmlia::NodeInfoPtr>& nodes);
    uint32_t GetNeighborCount(transport::protobuf::RoutingMessage& message);
    std::vector<kadmlia::NodeInfoPtr> GetRandomNodes(
            std::vector<kadmlia::NodeInfoPtr>& neighbors,
            uint32_t number_to_get) const;
    void SelectNodes(
            transport::protobuf::RoutingMessage& message,
            const std::vector<kadmlia::NodeInfoPtr>& nodes,
            std::vector<kadmlia::NodeInfoPtr>& select_nodes);
    void SelectNodes(
            transport::protobuf::RoutingMessage& message,
            kadmlia::RoutingTablePtr& routing_table,
            std::shared_ptr<base::Uint64BloomFilter>& bloomfilter,
            std::vector<kadmlia::NodeInfoPtr>& select_nodes);
    void SendLayered(
            transport::protobuf::RoutingMessage& message,
            const std::vector<kadmlia::NodeInfoPtr>& nodes);
    void CheckDiffNetwork(transport::protobuf::RoutingMessage& message);

    // TODO(Charlie): for test evil
    bool ThisNodeIsEvil(transport::protobuf::RoutingMessage& message);
    bool IsIpValid(const std::string& ip);

    transport::TransportPtr transport_ptr_;

private:
    DISALLOW_COPY_AND_ASSIGN(GossipInterface);
};

}  // namespace gossip

}  // namespace top
