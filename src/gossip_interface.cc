// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xgossip/gossip_interface.h"

#include <unordered_set>

#include "xpbase/base/top_log.h"
#include "xpbase/base/uint64_bloomfilter.h"
#include "xgossip/include/gossip_utils.h"

namespace top {

namespace gossip {

// TODO(Charlie): for test evil
bool GossipInterface::ThisNodeIsEvil(transport::protobuf::RoutingMessage& message) {
    if (message.gossip().evil_rate() <= 0) {
        TOP_DEBUG("gossip evil_rate(%d) not work", message.gossip().evil_rate());
        return false;
    }
    TOP_INFO("gossip evil_rate(%d) work", message.gossip().evil_rate());

    static uint32_t all_node_num = 0;
    if (all_node_num == 0) {
        static std::mutex tmp_mutex;
        std::unique_lock<std::mutex> lock(tmp_mutex);
        if (all_node_num == 0) {
            uint32_t hash_num = base::xhash32_t::digest(global_xid->Get());
            srand(hash_num);
            int32_t rand_num = rand() % 10000;
            if (rand_num <= (message.gossip().evil_rate() * 1000)) {
                all_node_num = 1;
            } else {
                all_node_num = 2;
            }
        }
    }
    return all_node_num == 1;
}

void GossipInterface::CheckDiffNetwork(transport::protobuf::RoutingMessage& message) {
    if (message.gossip().diff_net()) {
        auto gossip_param = message.mutable_gossip();
        gossip_param->set_diff_net(false);
        message.clear_bloomfilter();
        message.set_hop_num(0);
        TOP_INFO("message from diff network and arrive the des network at the first time. %s",
                message.debug().c_str());
    }
}

uint64_t GossipInterface::GetDistance(const std::string& src, const std::string& des) {
    assert(src.size() >= sizeof(uint64_t));
    assert(des.size() >= sizeof(uint64_t));
    assert(src.size() == des.size());
    uint64_t dis = 0;
    uint32_t index = src.size() - 1;
    uint32_t rollleft_num = 56;
    for (uint32_t i = 0; i < 8; ++i) {
        dis += (static_cast<uint64_t>(static_cast<uint8_t>(src[index]) ^
            static_cast<uint8_t>(des[index])) << rollleft_num);
        --index;
        rollleft_num -= 8;
    }
    return dis;
}

void GossipInterface::Send(
        transport::protobuf::RoutingMessage& message,
        const std::vector<kadmlia::NodeInfoPtr>& nodes) {
    std::string body;
    if (!message.SerializeToString(&body)) {
        TOP_WARN2("wrouter message SerializeToString failed");
        return;
    }
    _xip2_header xip2_header;
    memset(&xip2_header, 0, sizeof(xip2_header));
    std::string header((const char*)&xip2_header, sizeof(xip2_header));
    std::string xdata = header + body;
    base::xpacket_t packet(
            base::xcontext_t::instance(),
            (uint8_t*)xdata.c_str(),
            xdata.size(),
            0,0,
            false);
    auto each_call = [this, &message, &packet, &xdata] (kadmlia::NodeInfoPtr node_info_ptr) {
        // TODO(Charlie): just for test, delete it
        if (!node_info_ptr) {
            TOP_WARN2("kadmlia::NodeInfoPtr null");
            return false;
        }

        if (node_info_ptr->xid.empty()) {
            TOP_ERROR("node xid is empty.");
            return false;
        }
        if ((node_info_ptr->xid).compare(global_xid->Get()) == 0) {
            TOP_ERROR("node xid equal self.");
            return false;
        }

        packet.reset();
        packet.get_body().push_back((uint8_t*)xdata.data(), xdata.size());
        packet.set_to_ip_addr(node_info_ptr->public_ip);
        packet.set_to_ip_port(node_info_ptr->public_port);

        if (kadmlia::kKadSuccess != transport_ptr_->SendData(packet)) {
            TOP_WARN2("SendData to  endpoint(%s:%d) failed",
                    node_info_ptr->public_ip.c_str(),
                    node_info_ptr->public_port);
            return false;
        }
#ifdef TOP_TESTING_PERFORMANCE
        TOP_DEBUG("SendData size(%d) to endpoint(%s:%d) success",
                packet.get_body().size(),
                node_info_ptr->public_ip.c_str(),
                node_info_ptr->public_port);
        TOP_NETWORK_DEBUG_FOR_PROTOMESSAGE(
                std::string("send to: ") +
                node_info_ptr->public_ip + ":" +
                check_cast<std::string>(node_info_ptr->public_port),
                message);
#endif
        return true;
    };

    std::for_each(nodes.begin(), nodes.end(), each_call);
}

uint32_t GossipInterface::GetNeighborCount(transport::protobuf::RoutingMessage& message) {
    if (message.gossip().neighber_count() > 0) {
        return message.gossip().neighber_count();
    }

    if (message.gossip().reliable_level() > 0) {
        return GetRandomNeighbersCount(message.gossip().reliable_level());
    }
    return GetRandomNeighbersCount(kGossipReliableLow);
}

std::vector<kadmlia::NodeInfoPtr> GossipInterface::GetRandomNodes(
        std::vector<kadmlia::NodeInfoPtr>& neighbors,
        uint32_t number_to_get) const {
    if (neighbors.size() <= number_to_get) {
        return neighbors;
    }
    std::random_shuffle(neighbors.begin(), neighbors.end());
    return std::vector<kadmlia::NodeInfoPtr> {
            neighbors.begin(),
            neighbors.begin() + number_to_get};
}

void GossipInterface::SelectNodes(
        transport::protobuf::RoutingMessage& message,
        const std::vector<kadmlia::NodeInfoPtr>& nodes,
        std::vector<kadmlia::NodeInfoPtr>& select_nodes) {
    uint64_t min_dis = message.gossip().min_dis();
    uint64_t max_dis = message.gossip().max_dis();
    if (max_dis <= 0) {
        max_dis = std::numeric_limits<uint64_t>::max();
    }
    uint64_t left_min = message.gossip().left_min();
    uint64_t right_max = message.gossip().right_max();
    uint32_t left_overlap = message.gossip().left_overlap();
    uint32_t right_overlap = message.gossip().right_overlap();
    if (left_overlap > 0 && left_overlap < 20) {
        uint64_t tmp_min_dis = min_dis;
        double rate = (double)left_overlap / 10;
        uint64_t step = static_cast<uint64_t>((tmp_min_dis - left_min) *  rate);
        if (step > tmp_min_dis) {
            min_dis = 0;
        } else {
            min_dis = tmp_min_dis - step;
        }
    }
    if (right_overlap > 0 && right_overlap < 20) {
        uint64_t tmp_max_dis = max_dis;
        double rate = (double)right_overlap / 10;
        uint64_t step = static_cast<uint64_t>((right_max -  max_dis) * rate);
        if (std::numeric_limits<uint64_t>::max() - step > tmp_max_dis) {
            max_dis = tmp_max_dis + step;
        } else {
            max_dis = std::numeric_limits<uint64_t>::max();
        }
    }

    uint32_t select_num = GetNeighborCount(message);
    for (auto iter = nodes.begin(); iter != nodes.end(); ++iter) {
        if (select_nodes.size() >= select_num) {
            break;
        }

        if ((*iter)->hash64 > min_dis && (*iter)->hash64 < max_dis) {
            select_nodes.push_back(*iter);
        }
    }

    std::sort(
            select_nodes.begin(),
            select_nodes.end(),
            [](const kadmlia::NodeInfoPtr& left, const kadmlia::NodeInfoPtr& right) -> bool{
        return left->hash64 < right->hash64;
    });
}

void GossipInterface::SelectNodes(
        transport::protobuf::RoutingMessage& message,
        kadmlia::RoutingTablePtr& routing_table,
        std::shared_ptr<base::Uint64BloomFilter>& bloomfilter,
        std::vector<kadmlia::NodeInfoPtr>& select_nodes) {
    uint64_t min_dis = message.gossip().min_dis();
    uint64_t max_dis = message.gossip().max_dis();
    if (max_dis <= 0) {
        max_dis = std::numeric_limits<uint64_t>::max();
    }
    uint64_t left_min = message.gossip().left_min();
    uint64_t right_max = message.gossip().right_max();
    uint32_t left_overlap = message.gossip().left_overlap();
    uint32_t right_overlap = message.gossip().right_overlap();
    if (left_overlap > 0 && left_overlap < 20) {
        uint64_t tmp_min_dis = min_dis;
        double rate = (double)left_overlap / 10;
        uint64_t step = static_cast<uint64_t>((tmp_min_dis - left_min) *  rate);
        if (step > tmp_min_dis) {
            min_dis = 0;
        } else {
            min_dis = tmp_min_dis - step;
        }
    }
    if (right_overlap > 0 && right_overlap < 20) {
        uint64_t tmp_max_dis = max_dis;
        double rate = (double)right_overlap / 10;
        uint64_t step = static_cast<uint64_t>((right_max -  max_dis) * rate);
        if (std::numeric_limits<uint64_t>::max() - step > tmp_max_dis) {
            max_dis = tmp_max_dis + step;
        } else {
            max_dis = std::numeric_limits<uint64_t>::max();
        }
    }

    uint32_t select_num = GetNeighborCount(message);
    std::vector<kadmlia::NodeInfoPtr> nodes;
    routing_table->GetRangeNodes(min_dis, max_dis, nodes);
    if (nodes.empty()) {
        return;
    }

    // uint32_t filtered = 0;
    auto gossip_param = message.mutable_gossip();
    std::random_shuffle(nodes.begin(), nodes.end());
    for (auto iter = nodes.begin(); iter != nodes.end(); ++iter) {
        if (select_nodes.size() >= select_num) {
            break;
        }
        if (!IsIpValid((*iter)->public_ip)) {
            continue;
        }
        if ((*iter)->hash64 == 0) {
            continue;
        }
        if (((*iter)->xid).compare(global_xid->Get()) == 0) {
            continue;
        }

        if (bloomfilter->Contain((*iter)->hash64)) {
#ifdef TOP_TESTING_PERFORMANCE
            ++filtered;
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
        select_nodes.push_back(*iter);
    }
    gossip_param->clear_pre_ip();
    gossip_param->clear_pre_port();

    std::sort(
            select_nodes.begin(),
            select_nodes.end(),
            [](const kadmlia::NodeInfoPtr& left, const kadmlia::NodeInfoPtr& right) -> bool{
        return left->hash64 < right->hash64;
    });
}

void GossipInterface::SendLayered(
        transport::protobuf::RoutingMessage& message,
        const std::vector<kadmlia::NodeInfoPtr>& nodes) {
    uint64_t min_dis = message.gossip().min_dis();
    uint64_t max_dis = message.gossip().max_dis();
    if (max_dis <= 0) {
        max_dis = std::numeric_limits<uint64_t>::max();
    }

    uint8_t local_buf[kUdpPacketBufferSize];
    base::xpacket_t packet(base::xcontext_t::instance(), local_buf, sizeof(local_buf), 0, false);
    _xip2_header xip2_header;
    memset(&xip2_header, 0, sizeof(xip2_header));
    std::string header((const char*)&xip2_header, sizeof(xip2_header));
    std::string xdata;

    for (uint32_t i = 0; i < nodes.size(); ++i) {
        auto gossip = message.mutable_gossip();
        if (i == 0) {
            gossip->set_min_dis(min_dis);
            gossip->set_left_min(min_dis);

            if (nodes.size() == 1) {
                gossip->set_max_dis(max_dis);
                gossip->set_right_max(max_dis);
            } else {
                gossip->set_max_dis(nodes[0]->hash64);
                gossip->set_right_max(nodes[1]->hash64);
            }
        }
        
        if (i > 0 && i < (nodes.size() - 1)) {
            gossip->set_min_dis(nodes[i - 1]->hash64);
            gossip->set_max_dis(nodes[i]->hash64);
            
            if (i == 1) {
                gossip->set_left_min(min_dis);
            } else {
                gossip->set_left_min(nodes[i - 2]->hash64);
            }
            gossip->set_right_max(nodes[i + 1]->hash64);
        } 

        if (i > 0 && i == (nodes.size() - 1)) {
            gossip->set_min_dis(nodes[i - 1]->hash64);
            gossip->set_max_dis(max_dis);

            if (i == 1) {
                gossip->set_left_min(min_dis);
            } else {
                gossip->set_left_min(nodes[i - 2]->hash64);
            }
            gossip->set_right_max(max_dis);
        }

        std::string body;
        if (!message.SerializeToString(&body)) {
            TOP_WARN2("wrouter message SerializeToString failed");
            return;
        }
        xdata = header + body;
        packet.reset();
        packet.get_body().push_back((uint8_t*)xdata.data(), xdata.size());
        packet.set_to_ip_addr(nodes[i]->public_ip);
        packet.set_to_ip_port(nodes[i]->public_port);

        if (kadmlia::kKadSuccess != transport_ptr_->SendData(packet)) {
            TOP_WARN2("SendData to  endpoint(%s:%d) failed",
                    nodes[i]->public_ip.c_str(),
                    nodes[i]->public_port);
            continue;
        }
#ifdef TOP_TESTING_PERFORMANCE
        TOP_NETWORK_DEBUG_FOR_PROTOMESSAGE(
                std::string("send to: ") +
                nodes[i]->public_ip + ":" +
                check_cast<std::string>(nodes[i]->public_port),
                message);
#endif
    };
}

#define OUT_NETWORK_IPS
#ifdef OUT_NETWORK_IPS
static const std::unordered_set<std::string> test_for_valid_ip_set{
    "104.248.176.228",
    "104.248.176.48",
    "104.248.176.8",

    "104.248.178.100",
    "104.248.178.119",
    "104.248.178.123",

    "104.248.178.135",
    "104.248.178.148",
    "104.248.178.182",

    "104.248.178.186",
    "104.248.178.193",
    "104.248.178.194",

    "104.248.178.3",
    "104.248.178.64",
    "104.248.178.99",

    "104.248.180.196",
    "104.248.180.199",
    "104.248.180.24",

    "104.248.180.242",
    "104.248.180.246",
    "104.248.180.38",

    "104.248.180.52",
    "104.248.180.72",
    "104.248.180.74",

    "104.248.180.98",
    "104.248.180.99",
    "104.248.184.0"
};
#else
static const std::unordered_set<std::string> test_for_valid_ip_set{
    "192.168.50.81",
    "192.168.50.82",
    "192.168.50.84",

    "192.168.50.85",
    "192.168.50.86",
    "192.168.50.91",

    "192.168.50.92",
    "192.168.50.94",
    "192.168.50.95",

    "192.168.50.96",
    "192.168.50.97",
    "192.168.50.98",

    "192.168.50.99",
    "192.168.50.100",
    "192.168.50.101",

    "192.168.50.102",
    "192.168.50.104",
    "192.168.50.105"
};
#endif

bool GossipInterface::IsIpValid(const std::string& ip) {
#ifdef TOP_TESTING_PERFORMANCE_IP_TEST
    return test_for_valid_ip_set.find(ip) != test_for_valid_ip_set.end();
#else
    return true;
#endif
}

}  // namespace gossip

}  // namespace top
