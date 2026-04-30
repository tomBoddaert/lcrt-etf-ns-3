/*
 * Copyright (c) 2026 Tom Boddaert
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Tom Boddaert <etf-ns3@tomboddaert.com>
 */

#include "etf.h"

#include "lcrt.h"

#include "ns3/double.h"
#include "ns3/waypoint-mobility-model.h"

#undef NS_LOG_APPEND_CONTEXT
#define NS_LOG_APPEND_CONTEXT                                                                      \
    if (m_node)                                                                                    \
    {                                                                                              \
        std::clog << "[node " << m_node->GetId() << "] ";                                          \
    }

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("EtfPathing");

namespace etf
{
NS_OBJECT_ENSURE_REGISTERED(EtfPathing);

EtfPathing::EtfPathing()
    : m_path(nullptr),
    m_timeout(Timer::CANCEL_ON_DESTROY)
{
}

TypeId
EtfPathing::GetTypeId()
{
    static TypeId tid = TypeId("ns3::etf::EtfPathing")
                            .SetParent<Application>()
                            .SetGroupName("Etf")
                            .AddConstructor<EtfPathing>()
                            .AddAttribute("Speed",
                                          "UAV movement speed",
                                          DoubleValue(5),
                                          MakeDoubleAccessor(&EtfPathing::m_speed),
                                          MakeDoubleChecker<double>(0));

    return tid;
}

EtfPathing::~EtfPathing()
{
}

void
EtfPathing::DoDispose()
{
    NS_LOG_FUNCTION(this);
    if (m_path)
    {
        etf_path_drop(m_path);
        m_path = nullptr;
    }
    Application::DoDispose();
}

void
EtfPathing::StartApplication()
{
    NS_LOG_FUNCTION(this);

    Ptr<lcrt::RoutingProtocol> lcrt = m_node->GetObject<lcrt::RoutingProtocol>();
    NS_ASSERT_MSG(lcrt, "ETF requires LCRT to be installed on the node");

    lcrt->TraceConnectWithoutContext("Parent", MakeCallback(&EtfPathing::OnParent, this));
}

bool
EtfPathing::BeginTransition(Vector target)
{
    NS_LOG_FUNCTION(this << target);

    if (m_path)
    {
        NS_LOG_WARN("node is currently mid-transition (it will be cancelled)");
        etf_path_drop(m_path);
        m_path = nullptr;
        m_nextParent = Ipv4Address::GetAny();
    }

    Ptr<lcrt::RoutingProtocol> lcrt = m_node->GetObject<lcrt::RoutingProtocol>();
    NS_ASSERT_MSG(lcrt, "ETF requires LCRT to be installed on the node");
    const LcrtArea* area = lcrt->GetInternal();

    EtfPath* path = etf_find_path(area, LcrtPosition{target.x, target.y, target.z});
    if (!path)
    {
        return false;
    }

    m_path = path;
    m_target = target;
    Simulator::ScheduleNow(&EtfPathing::BeginNextSegment, this);

    return true;
}

void
EtfPathing::BeginNextSegment()
{
    if (!m_path)
    {
        NS_LOG_ERROR("BeginNextSegment called with no active path");
        return;
    }
    LcrtPosition nextPositionLcrt;
    uint32_t nextForwarder;
    bool continues = etf_path_next(m_path, &nextPositionLcrt, &nextForwarder);
    Vector nextPosition = Vector(nextPositionLcrt.x, nextPositionLcrt.y, nextPositionLcrt.z);
    if (!continues)
    {
        etf_path_drop(m_path);
        m_path = nullptr;

        nextPosition = m_target;
    }

    Ptr<WaypointMobilityModel> mob = GetNode()->GetObject<WaypointMobilityModel>();
    NS_ASSERT_MSG(mob, "ETF requires the WaypointMobilityModel to be installed on the node");
    Vector currentPosition = mob->GetPosition();

    double distance = CalculateDistance(currentPosition, nextPosition);
    Time duration = Seconds(distance / m_speed);

    mob->AddWaypoint(Waypoint(Simulator::Now() + duration, nextPosition));

    if (continues)
    {
        Simulator::Schedule(duration, &EtfPathing::OnSegmentEnd, this, Ipv4Address(nextForwarder));
    }
}

void
EtfPathing::OnSegmentEnd(Ipv4Address nextParent)
{
    Ptr<lcrt::RoutingProtocol> lcrt = m_node->GetObject<lcrt::RoutingProtocol>();
    NS_ASSERT(lcrt);

    m_nextParent = nextParent;
    lcrt->ChangeParent(nextParent);
}

void
EtfPathing::OnParent(Ipv4Address parent)
{
    if (parent != m_nextParent){
        return;
    }
    m_nextParent = Ipv4Address::GetAny();

    BeginNextSegment();
}

} // namespace etf

} // namespace ns3
