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
#define protected public
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

class TestGossipInterface : public testing::Test {
public:
    enum TestRoutingType {};

    static void SetUpTestCase() {}

    static void TearDownTestCase() {}

    virtual void SetUp() {}

    virtual void TearDown() {}
    void Chooselayer(
            std::vector<NodeInfoPtr>& nodes,
            transport::protobuf::RoutingMessage message,
            std::vector<transport::protobuf::RoutingMessage>& msg_vec) {
        std::vector<kadmlia::NodeInfoPtr> select_nodes;
        GossipBloomfilterLayer bloom_layer(nullptr);
        bloom_layer.SelectNodes(message, nodes, select_nodes);
        std::cout << "select nodes: ";
        for (uint32_t i = 0; i < select_nodes.size(); ++i) {
            std::cout << select_nodes[i]->hash64 << " ";
        }
        std::cout << std::endl;

        uint64_t min_dis = message.gossip().min_dis();
        uint64_t max_dis = message.gossip().max_dis();
        if (max_dis <= 0) {
            max_dis = std::numeric_limits<uint64_t>::max();
        }

        for (uint32_t i = 0; i < select_nodes.size(); ++i) {
            auto gossip = message.mutable_gossip();
            if (i == 0) {
                gossip->set_min_dis(min_dis);
                gossip->set_left_min(min_dis);

                if (select_nodes.size() == 1) {
                    gossip->set_max_dis(max_dis);
                    gossip->set_right_max(max_dis);
                } else {
                    gossip->set_max_dis(select_nodes[0]->hash64);
                    gossip->set_right_max(select_nodes[1]->hash64);
                }
            }
        
            if (i > 0 && i < (select_nodes.size() - 1)) {
                gossip->set_min_dis(select_nodes[i - 1]->hash64);
                gossip->set_max_dis(select_nodes[i]->hash64);
            
                if (i == 1) {
                    gossip->set_left_min(min_dis);
                } else {
                    gossip->set_left_min(select_nodes[i - 2]->hash64);
                }
                gossip->set_right_max(select_nodes[i + 1]->hash64);
            } 

            if (i > 0 && i == (select_nodes.size() - 1)) {
                gossip->set_min_dis(select_nodes[i - 1]->hash64);
                gossip->set_max_dis(max_dis);

                if (i == 1) {
                    gossip->set_left_min(min_dis);
                } else {
                    gossip->set_left_min(select_nodes[i - 2]->hash64);
                }
                gossip->set_right_max(max_dis);
            }

            std::cout << i << ": " << gossip->min_dis() << ":" << gossip->max_dis()
                << " , " << gossip->left_min() << ":" << gossip->right_max() << std::endl;
            msg_vec.push_back(message);
        }
    }
};

TEST_F(TestGossipInterface, SelectNodesNoOverlap) {
    std::vector<NodeInfoPtr> nodes;
    uint64_t local_hash = base::xhash64_t::digest(RandomString(kNodeIdSize));
    for (uint32_t i = 0; i < 10240; ++i) {
        auto node_ptr = std::make_shared<NodeInfo>(RandomString(kNodeIdSize));
        node_ptr->hash64 = i;
        nodes.push_back(node_ptr);
    }
    std::random_shuffle(nodes.begin(), nodes.end());
    transport::protobuf::RoutingMessage message;
    auto gossip_param = message.mutable_gossip();
    gossip_param->set_min_dis(0);
    gossip_param->set_max_dis(std::numeric_limits<uint64_t>::max());
    gossip_param->set_neighber_count(3);
    std::vector<transport::protobuf::RoutingMessage> msg_vec;
    Chooselayer(nodes, message, msg_vec);
    int layer_num = 1;
    for (uint32_t layer_num = 1; layer_num < 10; ++layer_num) {
        std::cout << "layer: " << layer_num++ << std::endl;
        std::vector<transport::protobuf::RoutingMessage> tmp_msg_vec;
        for (uint32_t i = 0; i < msg_vec.size(); ++i) {
            std::vector<transport::protobuf::RoutingMessage> tmp_vec;
            Chooselayer(nodes, msg_vec[i], tmp_vec);
            for (uint32_t i = 0; i < tmp_vec.size(); ++i) {
                tmp_msg_vec.push_back(tmp_vec[i]);
            }
        }
        msg_vec = tmp_msg_vec;
        std::cout << std::endl;
    }
}

}  // namespace test

}  // namespace gossip

}  // namespace top
