// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "xledger/xledger_face.h"
#include "xpbase/base/top_utils.h"

namespace top {

namespace gossip {

class HeaderBlockData {
public:
    explicit HeaderBlockData(std::shared_ptr<top::ledger::xledger_face_t> ledger_face);
    ~HeaderBlockData();
    void AddData(const std::string& header_hash, const std::string& block);
    void GetData(const std::string& header_hash, std::string& block);
    bool HasData(const std::string& header_hash);
    void RemoveData(const std::string& header_hash);

private:
    std::shared_ptr<top::ledger::xledger_face_t> ledger_face_;

    DISALLOW_COPY_AND_ASSIGN(HeaderBlockData);
};

}  // namespace gossip

}  // namespace top
