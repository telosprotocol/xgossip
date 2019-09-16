// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "xtransport/transport.h"
#include "xgossip/gossip_interface.h"

namespace top {

namespace gossip {

class GossipSetLayer : public GossipInterface {
public:
    explicit GossipSetLayer(transport::TransportPtr transport_ptr);
    virtual ~GossipSetLayer();
    virtual void Broadcast(
            uint64_t local_hash64,
            transport::protobuf::RoutingMessage& message,
            std::shared_ptr<std::vector<kadmlia::NodeInfoPtr>> neighbors);

private:

    DISALLOW_COPY_AND_ASSIGN(GossipSetLayer);
};

}  // namespace gossip

}  // namespace top
