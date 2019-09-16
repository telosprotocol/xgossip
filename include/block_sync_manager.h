// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <queue>
#include <set>
#include <mutex>

#include "xbase/xpacket.h"
#include "xtransport/proto/transport.pb.h"
#include "xpbase/base/top_timer.h"
#include "xgossip/include/header_block_data.h"
#include "xtransport/message_manager/message_manager_intf.h"

namespace top {

namespace kadmlia {

class RoutingTable;
typedef std::shared_ptr<RoutingTable> RoutingTablePtr;

}

namespace gossip {

struct SyncBlockItem {
    uint64_t routing_service_type;
    std::string header_hash;
    std::chrono::steady_clock::time_point time_point;
};

class BlockSyncManager {
public:
    static BlockSyncManager* Instance();
    void SetLeagerFace(std::shared_ptr<top::ledger::xledger_face_t> ledger_face);
    void SetRoutingTablePtr(kadmlia::RoutingTablePtr& routing_table);
    void NewBroadcastMessage(transport::protobuf::RoutingMessage& message);

private:
    BlockSyncManager();
    ~BlockSyncManager();

    bool HeaderHashExists(const std::string& header_hash);
    uint32_t GetBlockMsgType(const std::string& header_hash);
    void AddHeaderHashToQueue(
            const std::string& header_hash,
            uint64_t service_type);
    void CheckHeaderHashQueue();
    uint64_t GetRoutingServiceType(const std::string& des_node_id);
    void SendSyncAsk(std::shared_ptr<SyncBlockItem>& sync_item);
    void HandleSyncAsk(
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet);
    void HandleSyncAck(
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet);
    void HandleSyncRequest(
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet);
    void HandleSyncResponse(
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet);
    bool HeaderRequested(const std::string& header_hash);
    void RemoveHeaderBlock(const std::string& header_hash);
    bool DataExists(const std::string& header_hash);

    std::map<std::string, std::shared_ptr<SyncBlockItem>> block_map_;
    std::mutex block_map_mutex_;
    base::TimerRepeated timer_{base::TimerManager::Instance(), "BlockSyncManager"};
    std::map<std::string, std::chrono::steady_clock::time_point> requested_headers_;
    std::mutex requested_headers_mutex_;
    std::shared_ptr<HeaderBlockData> header_block_data_{ nullptr };
    kadmlia::RoutingTablePtr routing_table_;
    transport::MessageManagerIntf* message_manager_{transport::MessageManagerIntf::Instance()};

    DISALLOW_COPY_AND_ASSIGN(BlockSyncManager);
};

}  // namespace gossip

}  // namespace top
