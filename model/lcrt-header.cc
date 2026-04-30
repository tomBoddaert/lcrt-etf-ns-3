/*
 * Copyright (c) 2026 Tom Boddaert
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Tom Boddaert <lcrt-ns3@tomboddaert.com>
 */

#include "lcrt-header.h"

#include "ns3/address-utils.h"

#include <ostream>

namespace ns3
{
namespace lcrt
{

NS_OBJECT_ENSURE_REGISTERED(LcrtHeader);

LcrtHeader::LcrtHeader()
    : m_lastForwarder(Ipv4Address::GetZero())
{
}

TypeId
LcrtHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::lcrt::LcrtHeader")
                            .SetParent<Header>()
                            .SetGroupName("Lcrt")
                            .AddConstructor<LcrtHeader>();

    return tid;
}

TypeId
LcrtHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
LcrtHeader::GetSerializedSize() const
{
    return 5;
}

void
LcrtHeader::Serialize(Buffer::Iterator i) const
{
    WriteTo(i, m_lastForwarder);
    i.WriteU8(m_packetId);
}

uint32_t
LcrtHeader::Deserialize(Buffer::Iterator start)
{
    Buffer::Iterator i = start;
    ReadFrom(i, m_lastForwarder);
    m_packetId = i.ReadU8();

    uint32_t dist = i.GetDistanceFrom(start);
    NS_ASSERT(dist == GetSerializedSize());
    return dist;
}

void
LcrtHeader::Print(std::ostream& os) const
{
    os << "Previous forwarder: ipv4 " << m_lastForwarder << ", Packet ID: " << (uint32_t)m_packetId;
}

Ipv4Address
LcrtHeader::GetLastForwarder() const
{
    return m_lastForwarder;
}

void
LcrtHeader::SetLastForwarder(Ipv4Address lastForwarder)
{
    m_lastForwarder = lastForwarder;
}

uint8_t
LcrtHeader::GetPacketId() const
{
    return m_packetId;
}

void
LcrtHeader::SetPacketId(uint8_t packetId)
{
    m_packetId = packetId;
}

std::ostream&
operator<<(std::ostream& os, const LcrtHeader& h)
{
    h.Print(os);
    return os;
}

} // namespace lcrt
} // namespace ns3
