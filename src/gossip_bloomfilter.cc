// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xgossip/include/gossip_bloomfilter.h"

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

GossipBloomfilter::GossipBloomfilter(transport::TransportPtr transport_ptr)
        : GossipInterface(transport_ptr) {}

GossipBloomfilter::~GossipBloomfilter() {}

void GossipBloomfilter::Broadcast(
        uint64_t local_hash64,
        transport::protobuf::RoutingMessage& message,
        std::shared_ptr<std::vector<kadmlia::NodeInfoPtr>> prt_neighbors) {
    auto neighbors = *prt_neighbors;
    TOP_NETWORK_DEBUG_FOR_REDIS(message, "recv_count");

    TOP_DEBUG("GossipBloomfilter Broadcast neighbors size %d", neighbors.size());

    BlockSyncManager::Instance()->NewBroadcastMessage(message);
    if (message.gossip().max_hop_num() > 0 &&
            message.gossip().max_hop_num() <= message.hop_num()) {
        TOP_WARN2("message.type(%d) hop_num(%d) larger than gossip_max_hop_num(%d)",
                message.type(),
                message.hop_num(),
                message.gossip().max_hop_num());
        return;
    }

    if (ThisNodeIsEvil(message)) {
        TOP_WARN2("this node(%s) is evil", HexEncode(global_xid->Get()).c_str());
        return;
    }
    bool stop_gossip = false;
    auto bloomfilter = MessageWithBloomfilter::Instance()->GetMessageBloomfilter(
            message,
            stop_gossip);
    if (stop_gossip) {
        TOP_NETWORK_DEBUG_FOR_REDIS(message, "hop_num");
        TOP_DEBUG("stop gossip for message.type(%d) hop_num(%d)", message.type(), message.hop_num());
        return;
    }

    assert(bloomfilter);
    if (!bloomfilter) {
        TOP_WARN2("bloomfilter invalid");
        return;
    }
    bloomfilter->Add(local_hash64);

    std::vector<kadmlia::NodeInfoPtr> tmp_neighbors;
    uint32_t filtered = 0;
    for (auto iter = neighbors.begin(); iter != neighbors.end(); ++iter) {
        if ((*iter)->hash64 == 0) {
            TOP_WARN("node:%s hash64 empty, invalid", HexEncode((*iter)->xid).c_str());
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

        tmp_neighbors.push_back(*iter);
    }
    TOP_DEBUG("GossipBloomfilter Broadcast tmp_neighbors size %d, filtered %d nodes",
            tmp_neighbors.size(),
            filtered);

    std::vector<kadmlia::NodeInfoPtr> rest_random_neighbors;
    rest_random_neighbors = GetRandomNodes(tmp_neighbors, GetNeighborCount(message));
    if (rest_random_neighbors.empty()) {
        TOP_WARN2("stop Broadcast, rest_random_neighbors empty, broadcast failed, msg.hop_num(%d), msg.type(%d)",
                message.hop_num(),
                message.type());
        return;
    }

    if (message.hop_num() > message.gossip().ign_bloomfilter_level()) {
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

    TOP_DEBUG("GossipBloomfilter Broadcast finally %d neighbors", rest_random_neighbors.size());
    Send(message, rest_random_neighbors);

    TOP_NETWORK_DEBUG_FOR_REDIS(message, "hop_num");
}

void GossipBloomfilter::BroadcastWithNoFilter(
        const std::string& local_id,
        transport::protobuf::RoutingMessage& message,
        const std::vector<kadmlia::NodeInfoPtr>& neighbors) {
    TOP_DEBUG("GossipBloomfilter Broadcast neighbors size %d", neighbors.size());

    BlockSyncManager::Instance()->NewBroadcastMessage(message);
    if (message.gossip().max_hop_num() > 0 &&
            message.gossip().max_hop_num() <= message.hop_num()) {
        TOP_WARN2("message.type(%d) hop_num(%d) larger than gossip_max_hop_num(%d)",
                message.type(),
                message.hop_num(),
                message.gossip().max_hop_num());
        return;
    }

    if (ThisNodeIsEvil(message)) {
        TOP_WARN2("this node(%s) is evil", HexEncode(local_id).c_str());
        return;
    }
    Send(message, neighbors);
}

}  // namespace gossip

}  // namespace top
