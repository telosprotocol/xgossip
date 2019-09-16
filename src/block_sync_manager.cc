// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xgossip/include/block_sync_manager.h"

#include "xpbase/base/top_log.h"
#include "xpbase/base/kad_key/get_kadmlia_key.h"
#include "xtransport/transport_message_register.h"
#include "xkad/routing_table/callback_manager.h"
#include "xkad/routing_table/routing_table.h"
#include "xwrouter/register_routing_table.h"
#include "xwrouter/message_handler/wrouter_message_handler.h"
#include "xgossip/include/gossip_utils.h"
#include "xutility/xhash.h"
#include "xpbase/base/redis_client.h"
#include "xpbase/base/top_utils.h"
#include "xwrouter/xwrouter.h"

namespace top {

namespace gossip {

static const uint32_t kMaxBlockQueueSize = 1024u;
static const uint32_t kCheckHeaderHashPeriod = 1000 * 1000;  // 1s
static const uint32_t kSyncAskNeighborCount = 6u;  // random ask 3 neighbors who has block
static const uint32_t kHeaderSavePeriod = 60 * 1000;  // keep 30s
static const uint32_t kHeaderRequestedPeriod = 2 * 1000;  // request 3s

BlockSyncManager::BlockSyncManager() {
    wrouter::WrouterRegisterMessageHandler(kGossipBlockSyncAsk, [this](
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet) {
        TOP_INFO("HandleMessage kGossipBlockSyncAsk");
        HandleSyncAsk(message, packet);
    });
    wrouter::WrouterRegisterMessageHandler(kGossipBlockSyncAck, [this](
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet) {
        TOP_INFO("HandleMessage kGossipBlockSyncAck");
        HandleSyncAck(message, packet);
    });
    wrouter::WrouterRegisterMessageHandler(kGossipBlockSyncRequest, [this](
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet) {
        TOP_INFO("HandleMessage kGossipBlockSyncRequest");
        HandleSyncRequest(message, packet);
    });
    wrouter::WrouterRegisterMessageHandler(kGossipBlockSyncResponse, [this](
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet) {
        TOP_INFO("HandleMessage kGossipBlockSyncResponse");
        HandleSyncResponse(message, packet);
    });
    timer_.Start(
            kCheckHeaderHashPeriod,
            kCheckHeaderHashPeriod,
            std::bind(&BlockSyncManager::CheckHeaderHashQueue, this));
}

BlockSyncManager::~BlockSyncManager() {}

BlockSyncManager* BlockSyncManager::Instance() {
    static BlockSyncManager ins;
    return &ins;
}

void BlockSyncManager::NewBroadcastMessage(transport::protobuf::RoutingMessage& message) {
    if (!message.gossip().has_header_hash() || message.gossip().header_hash().empty()) {
        return;
    }
    TOP_DEBUG("blockmessage: msg_id(%d) has_block(%d),head_hash:%s",
            message.id(),
            message.gossip().has_block(),
            HexEncode(message.gossip().header_hash()).c_str());
    if (DataExists(message.gossip().header_hash())) {
        TOP_DEBUG("blockmessage: already have");
        return;
    }

    if (message.gossip().has_block() && !message.gossip().block().empty()) {
        TOP_DEBUG("add block message: id(%d) header_hash(%s)",
                message.id(),
                HexEncode(message.gossip().header_hash()).c_str());
        header_block_data_->AddData(message.gossip().header_hash(), message.SerializeAsString());
        return;
    }

    if (HeaderHashExists(message.gossip().header_hash())) {
        TOP_DEBUG("blockmessage header hash already have");
        return;
    }
    uint64_t des_service_type;
    if (message.has_is_root() && message.is_root()) {
        des_service_type = kRoot;
    } else {
        des_service_type = GetRoutingServiceType(message.des_node_id());
    }

    AddHeaderHashToQueue(
            message.gossip().header_hash(),
            des_service_type);

}

bool BlockSyncManager::DataExists(const std::string& header_hash) {
    return header_block_data_->HasData(header_hash);
}

bool BlockSyncManager::HeaderHashExists(const std::string& header_hash) {
    std::unique_lock<std::mutex> lock(block_map_mutex_);
    auto iter = block_map_.find(header_hash);
    if (iter != block_map_.end()) {
        return true;
    }
    return false;
}

void BlockSyncManager::AddHeaderHashToQueue(
        const std::string& header_hash,
        uint64_t service_type) {
    std::unique_lock<std::mutex> lock(block_map_mutex_);
    block_map_.insert(std::make_pair(
            header_hash,
            std::make_shared<SyncBlockItem>(SyncBlockItem{
            service_type,
            header_hash,
            std::chrono::steady_clock::now() +
            std::chrono::milliseconds(kHeaderSavePeriod) })));
}

void BlockSyncManager::SetRoutingTablePtr(kadmlia::RoutingTablePtr& routing_table) {
    routing_table_ = routing_table;
}

void BlockSyncManager::SendSyncAsk(std::shared_ptr<SyncBlockItem>& sync_item) {
	TOP_INFO("SendSyncAsk: %d, header_hash:%s",sync_item->routing_service_type,HexEncode(sync_item->header_hash).c_str());
    auto routing = wrouter::GetRoutingTable(sync_item->routing_service_type);
    if (!routing) {
		TOP_INFO("no routing table:%d", sync_item->routing_service_type);
        return;
    }
    assert(routing);
    transport::protobuf::RoutingMessage pbft_message;
    routing->SetFreqMessage(pbft_message);
    pbft_message.set_type(kGossipBlockSyncAsk);
    pbft_message.set_id(kadmlia::CallbackManager::MessageId());
    pbft_message.set_data(sync_item->header_hash);
	pbft_message.set_src_service_type(sync_item->routing_service_type);
	
    std::set<kadmlia::NodeInfoPtr> asked_nodes;
    for (uint32_t i = 0; i < kSyncAskNeighborCount; ++i) {
        auto node_ptr = routing->GetRandomNode();
        if (!node_ptr) {
            return;
        }

        auto iter = asked_nodes.find(node_ptr);
        if (iter != asked_nodes.end()) {
            continue;
        }
        pbft_message.set_des_node_id(node_ptr->node_id);
        routing->SendData(pbft_message, node_ptr->public_ip, node_ptr->public_port);
		TOP_DEBUG("send sync ask: %s,%d", node_ptr->public_ip.c_str(), node_ptr->public_port);
    }

    TOP_DEBUG("[gossip_sync]send out ask,%d[%s].", kGossipBlockSyncAsk, HexEncode(pbft_message.data()).c_str());
}

void BlockSyncManager::CheckHeaderHashQueue() {
    std::map<std::string, std::shared_ptr<SyncBlockItem>> block_map;
    {
        std::unique_lock<std::mutex> lock(block_map_mutex_);
        block_map = block_map_;
    }

    auto tp_now = std::chrono::steady_clock::now();
    {
        for (auto iter = block_map.begin(); iter != block_map.end(); ++iter) {
            auto item = iter->second;
            if (item->time_point <= tp_now || DataExists(item->header_hash)) {
                RemoveHeaderBlock(item->header_hash);
                continue;
            }

            /*
            if ((item->time_point - std::chrono::milliseconds(kHeaderSavePeriod) +
                    std::chrono::milliseconds(kHeaderRequestedPeriod)) > tp_now) {
                continue;
            }
            */

            if (HeaderHashExists(item->header_hash) && !HeaderRequested(item->header_hash)) {
                SendSyncAsk(item);
                continue;
            }
        }
    }
}

uint64_t BlockSyncManager::GetRoutingServiceType(const std::string& des_node_id) {
    auto kad_key = base::GetKadmliaKey(des_node_id);
    return kad_key->GetServiceType();
}

void BlockSyncManager::SetLeagerFace(std::shared_ptr<top::ledger::xledger_face_t> ledger_face) {
    header_block_data_ = std::make_shared<HeaderBlockData>(ledger_face);
}

void BlockSyncManager::HandleSyncAsk(
        transport::protobuf::RoutingMessage& message,
        base::xpacket_t& packet) {
	TOP_INFO("enter HandleSyncAsk:%s", HexEncode(message.data()).c_str());
    if (!message.has_data()) {
        return;
    }

    if (message.type() != kGossipBlockSyncAsk) {
        return;
    }

    if (!header_block_data_->HasData(message.data())) {
        return;
    }

    auto routing = wrouter::GetRoutingTable(message.src_service_type());
    if (!routing) {
		TOP_INFO("no routing table:%d", message.src_service_type());
        return;
    }
    assert(routing);
    transport::protobuf::RoutingMessage pbft_message;
    routing->SetFreqMessage(pbft_message);
    pbft_message.set_type(kGossipBlockSyncAck);
    pbft_message.set_id(message.id());
    pbft_message.set_data(message.data());
    pbft_message.set_des_node_id(message.src_node_id());
	pbft_message.set_src_service_type(message.src_service_type());

    routing->SendData(pbft_message, packet.get_from_ip_addr(), packet.get_from_ip_port());
    TOP_DEBUG("[gossip_sync]handled ask[%s].", HexEncode(message.data()).c_str());
}

void BlockSyncManager::HandleSyncAck(
        transport::protobuf::RoutingMessage& message,
        base::xpacket_t& packet) {
    if (!message.has_data()) {
        return;
    }

    if (message.type() != kGossipBlockSyncAck) {
        return;
    }

    /*
    if (HeaderRequested(message.data())) {
        return;
    }
    */

    if (!HeaderHashExists(message.data()) || DataExists(message.data())) {
        return;
    }

    auto routing = wrouter::GetRoutingTable(message.src_service_type());
    if (!routing) {
		TOP_INFO("no routing table:%d", message.src_service_type());
        return;
    }
    assert(routing);
    transport::protobuf::RoutingMessage pbft_message;
    routing->SetFreqMessage(pbft_message);
    pbft_message.set_type(kGossipBlockSyncRequest);
    pbft_message.set_id(kadmlia::CallbackManager::MessageId());
    pbft_message.set_des_node_id(message.src_node_id());
    pbft_message.set_data(message.data());
	pbft_message.set_src_service_type(message.src_service_type());

    routing->SendData(pbft_message, packet.get_from_ip_addr(), packet.get_from_ip_port());
    TOP_DEBUG("[gossip_sync]handled ack[%s].", HexEncode(message.data()).c_str());
}

void BlockSyncManager::HandleSyncRequest(
        transport::protobuf::RoutingMessage& message,
        base::xpacket_t& packet) {
    if (!message.has_data()) {
        return;
    }

    if (message.type() != kGossipBlockSyncRequest) {
        return;
    }

    std::string message_string;
    header_block_data_->GetData(message.data(), message_string);
    if (message_string.empty()) {
        return;
    }

    auto routing = wrouter::GetRoutingTable(message.src_service_type());
    if (!routing) {
		TOP_INFO("no routing table:%d", message.src_service_type());
        return;
    }
    assert(routing);
    transport::protobuf::RoutingMessage pbft_message;
    routing->SetFreqMessage(pbft_message);
    pbft_message.set_type(kGossipBlockSyncResponse);
    pbft_message.set_id(message.id());
    pbft_message.set_des_node_id(message.src_node_id());
    transport::protobuf::GossipSyncBlockData gossip_data;
    gossip_data.set_header_hash(message.data());
    gossip_data.set_block(message_string); // get the whole message stored
    pbft_message.set_data(gossip_data.SerializeAsString());
	pbft_message.set_src_service_type(message.src_service_type());

    routing->SendData(pbft_message, packet.get_from_ip_addr(), packet.get_from_ip_port());
    TOP_DEBUG("[gossip_sync]handled request[%s].", HexEncode(message.data()).c_str());
}

void BlockSyncManager::HandleSyncResponse(
        transport::protobuf::RoutingMessage& message,
        base::xpacket_t& packet) {
    if (!message.has_data()) {
        return;
    }

    if (message.type() != kGossipBlockSyncResponse) {
        return;
    }

    transport::protobuf::GossipSyncBlockData gossip_data;
    if (!gossip_data.ParseFromString(message.data())) {
        return;
    }
    std::string header_hash = gossip_data.header_hash();

    if (!HeaderHashExists(header_hash)) {
        return;
    }
    if (header_block_data_->HasData(header_hash)){
        return;
    }

    transport::protobuf::RoutingMessage sync_message;
    if (!sync_message.ParseFromString(gossip_data.block())) {
        TOP_WARN("SyncMessae ParseFromString failed");
        return;
    }
    header_block_data_->AddData(header_hash, gossip_data.block());

    // call callback
    if (sync_message.type() == kElectVhostRumorMessage) {
        std::string vhost_data = sync_message.gossip().block();
        uint32_t vhash = base::xhash32_t::digest(vhost_data);
        if (header_hash != std::to_string(vhash)) {
            TOP_WARN("[gossip_sync] header hash(%s) not equal", HexEncode(header_hash).c_str());
            return;
        }
        base::xpacket_t packet;
        wrouter::Wrouter::Instance()->HandleOwnSyncPacket(sync_message, packet);
        TOP_DEBUG("blockmessage callback hash:%u,header_hash:%s,type:%d", vhash, HexEncode(header_hash).c_str(), sync_message.type());
    } else if (sync_message.type() == kTestChainTrade) {
#ifndef NDEBUG
        std::string vhost_data = sync_message.gossip().block();
        uint32_t vhash = base::xhash32_t::digest(vhost_data);
        if (header_hash != std::to_string(vhash)) {
            TOP_WARN("[gossip_sync] header hash(%s) not equal", HexEncode(header_hash).c_str());
            return;
        }
        base::xpacket_t packet;
        wrouter::Wrouter::Instance()->HandleOwnSyncPacket(sync_message, packet);
        TOP_DEBUG("blockmessage callback msg_hash:%u,header_hash:%s,type:%d", sync_message.gossip().msg_hash(), HexEncode(header_hash).c_str(), sync_message.type());
        std::cout << "sync block ok " << std::endl;
#endif
    }

    RemoveHeaderBlock(header_hash);
    TOP_DEBUG("blockmessage add block data size(%d),hash:%s", gossip_data.block().size(), HexEncode(header_hash).c_str());
}

void BlockSyncManager::RemoveHeaderBlock(const std::string& header_hash) {
    {
        std::unique_lock<std::mutex> lock(requested_headers_mutex_);
        auto iter = requested_headers_.find(header_hash);
        if (iter != requested_headers_.end()) {
            requested_headers_.erase(iter);
        }
    }

    {
        std::unique_lock<std::mutex> lock(block_map_mutex_);
        auto iter = block_map_.find(header_hash);
        if (iter != block_map_.end()) {
            block_map_.erase(iter);
        }
    }
    // (delete block from db)
    header_block_data_->RemoveData(header_hash);
}

bool BlockSyncManager::HeaderRequested(const std::string& header_hash) {
    auto tp_now = std::chrono::steady_clock::now();
    std::unique_lock<std::mutex> lock(requested_headers_mutex_);
    auto iter = requested_headers_.find(header_hash);
    if (iter != requested_headers_.end()) {
        if (iter->second <= tp_now) {
            requested_headers_.erase(iter);
            return false;
        }
        // in kHeaderRequestPeriod seconds, have requested
        return true;
    }

    requested_headers_.insert(std::make_pair(
            header_hash,
            std::chrono::steady_clock::now() +
            std::chrono::milliseconds(kHeaderRequestedPeriod)));
    return false;
}

}  // namespace gossip

}  // namespace top
