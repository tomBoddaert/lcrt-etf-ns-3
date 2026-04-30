/*
 * Copyright (c) 2026 Tom Boddaert
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Tom Boddaert <lcrt-ns3@tomboddaert.com>
 *
 * This file is based on aodv-helper.cc by Pavel Boyko <boyko@iitp.ru>
 */

#include "lcrt-helper.h"

#include "ns3/lcrt.h"

namespace ns3
{

LcrtHelper::LcrtHelper()
    : Ipv4RoutingHelper()
{
    m_agentFactory.SetTypeId("ns3::lcrt::RoutingProtocol");
}

LcrtHelper*
LcrtHelper::Copy() const
{
    return new LcrtHelper(*this);
}

Ptr<Ipv4RoutingProtocol>
LcrtHelper::Create(Ptr<Node> node) const
{
    Ptr<lcrt::RoutingProtocol> agent = m_agentFactory.Create<lcrt::RoutingProtocol>();
    node->AggregateObject(agent);
    return agent;
}

void
LcrtHelper::Set(std::string name, const AttributeValue& value)
{
    m_agentFactory.Set(name, value);
}

} // namespace ns3
