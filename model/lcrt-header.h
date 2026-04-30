/*
 * Copyright (c) 2026 Tom Boddaert
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Tom Boddaert <lcrt-ns3@tomboddaert.com>
 */

#ifndef LCRTHEADER_H
#define LCRTHEADER_H

#include "ns3/header.h"
#include "ns3/ipv4-address.h"

#include <ostream>

namespace ns3
{
namespace lcrt
{

class LcrtHeader : public Header
{
  public:
    LcrtHeader();

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    Ipv4Address GetLastForwarder() const;
    void SetLastForwarder(Ipv4Address lastForwarder);
    uint8_t GetPacketId() const;
    void SetPacketId(uint8_t packetId);

  private:
    Ipv4Address m_lastForwarder;
    uint8_t m_packetId;
};

std::ostream& operator<<(std::ostream& os, const LcrtHeader& h);

} // namespace lcrt
} // namespace ns3

#endif // LCRTHEADER_H
