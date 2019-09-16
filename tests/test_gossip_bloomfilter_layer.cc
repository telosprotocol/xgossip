// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <gtest/gtest.h>

#include <iostream>
#include <algorithm>

#include "xpbase/base/top_utils.h"
#include "xpbase/base/line_parser.h"
#include "xpbase/base/check_cast.h"
#include "xpbase/base/xid/xid_def.h"
#include "xpbase/base/xid/xid_generator.h"
#include "xpbase/base/kad_key/platform_kadmlia_key.h"
#define private public
#include "xtransport/udp_transport/udp_transport.h"
#include "xtransport/message_manager/multi_message_handler.h"
#include "xkad/routing_table/routing_table.h"
#include "xkad/routing_table/local_node_info.h"
#include "xwrouter/multi_routing/multi_routing.h"
#include "xwrouter/register_routing_table.h"
#include "xgossip/include/broadcast_layered.h"
#include "xgossip/include/gossip_bloomfilter_layer.h"

namespace top {

using namespace kadmlia;

namespace gossip {

namespace test {

class TestGossipBloomfilterLayer : public testing::Test {
public:
    enum TestRoutingType {

    };
    static void SetUpTestCase() {
    }

    static void TearDownTestCase() {
    }

    virtual void SetUp() {
    }

    virtual void TearDown() {
    }

    RoutingTablePtr CreateRoutingTable(const std::string& peer) {
        std::string idtype(top::kadmlia::GenNodeIdType("CN", "VPN"));
        LocalNodeInfoPtr local_node_info;
        local_node_info.reset(new LocalNodeInfo());
        auto kad_key = std::make_shared<base::PlatformKadmliaKey>();
        kad_key->set_xnetwork_id(kEdgeXVPN);
        kad_key->set_zone_id(check_cast<uint8_t>(26));
        local_node_info->Init(
            "0.0.0.0", 0, false, false, idtype, kad_key, kad_key->xnetwork_id(), kRoleEdge);
        local_node_info->set_public_ip("127.0.0.1");
        local_node_info->set_public_port(10000);

        top::transport::TransportPtr udp_transport;
        udp_transport.reset(new top::transport::UdpTransport());
        auto thread_message_handler = std::make_shared<transport::MultiThreadHandler>();
        thread_message_handler->Init();
        udp_transport->Start(
                "0.0.0.0",
                0,
                thread_message_handler.get());

        RoutingTablePtr routing_table_ptr;
        routing_table_ptr.reset(new top::kadmlia::RoutingTable(
                udp_transport, kNodeIdSize, local_node_info));
        std::string bootstrap_path = "../../conf.ut/bootstrap.data";
        top::wrouter::UnregisterRoutingTable(100);
        top::wrouter::RegisterRoutingTable(100, routing_table_ptr);
        return routing_table_ptr;
    }
};

TEST_F(TestGossipBloomfilterLayer, GetBeginIndex) {
}

}  // namespace test

}  // namespace gossip

}  // namespace top
