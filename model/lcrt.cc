/*
 * Copyright (c) 2026 Tom Boddaert
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Tom Boddaert <lcrt-ns3@tomboddaert.com>
 *
 * This file is based on aodv-routing-protocol.cc by Elena Buchatskaia <borovkovaes@iitp.ru> and
 * Pavel Boyko <boyko@iitp.ru>
 */

#include "lcrt.h"

#include "lcrt-header.h"

#include "ns3/double.h"
#include "ns3/lcrt-header.h"
#include "ns3/mobility-model.h"
#include "ns3/pointer.h"
#include "ns3/string.h"
#include "ns3/udp-header.h"
#include "ns3/udp-l4-protocol.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/uinteger.h"
#include "ns3/wifi-net-device.h"

#include <cstdint>
#include <iomanip>
#include <optional>

#undef NS_LOG_APPEND_CONTEXT
#define NS_LOG_APPEND_CONTEXT                                                                      \
    if (m_ipv4)                                                                                    \
    {                                                                                              \
        std::clog << "[node " << m_ipv4->GetObject<Node>()->GetId() << "] ";                       \
    }

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("LcrtRoutingProtocol");

namespace lcrt
{
NS_OBJECT_ENSURE_REGISTERED(RoutingProtocol);

const uint32_t RoutingProtocol::LCRT_PORT = 1657;

RoutingProtocol::RoutingProtocol()
    // : m_addressMulticast("224.1.1.1"),
    // m_lcrtK(DEFAULT_CONFIG.k),
    // m_lcrtRadius(DEFAULT_CONFIG.radius),
    // m_lcrtBitrateCapacity(DEFAULT_CONFIG.bitrate_capacity),
    // m_lcrtConstructTimeout(DEFAULT_CONFIG.construct_timeout),
    // m_lcrtConstructSourceTimeout(DEFAULT_CONFIG.source_construct_timeout),
    : m_timeout{Timer(Timer::CANCEL_ON_DESTROY), Timer(Timer::CANCEL_ON_DESTROY)},
      m_isSource(false)
{
    for (size_t i = 0; i < 2; i++)
    {
        m_timeout[i].SetFunction(&RoutingProtocol::HandleTimeout, this);
        m_timeout[i].SetArguments<uint8_t>(i + 1);
    }
}

TypeId
RoutingProtocol::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::lcrt::RoutingProtocol")
            .SetParent<Ipv4RoutingProtocol>()
            .SetGroupName("Lcrt")
            .AddConstructor<RoutingProtocol>()
            .AddAttribute("Group",
                          "LCRT multicast group",
                          Ipv4AddressValue("224.1.1.1"),
                          MakeIpv4AddressAccessor(&RoutingProtocol::m_addressMulticast),
                          MakeIpv4AddressChecker())
            .AddAttribute("K",
                          "LCRT k value",
                          UintegerValue(5),
                          MakeUintegerAccessor(&RoutingProtocol::m_lcrtK),
                          MakeUintegerChecker<uint16_t>(1))
            .AddAttribute("Radius",
                          "LCRT node coverage radius",
                          DoubleValue(50),
                          MakeDoubleAccessor(&RoutingProtocol::m_lcrtRadius),
                          MakeDoubleChecker<double>(0))
            .AddAttribute("BitrateCapacity",
                          "LCRT node transmission bitrate capacity",
                          DoubleValue(600e6),
                          MakeDoubleAccessor(&RoutingProtocol::m_lcrtBitrateCapacity),
                          MakeDoubleChecker<float>(0))
            .AddAttribute("ConstructTimeout",
                          "LCRT construct timeout",
                          TimeValue(MilliSeconds(300)),
                          MakeTimeAccessor(&RoutingProtocol::m_lcrtConstructTimeout),
                          MakeTimeChecker())
            .AddAttribute("SourceConstructTimeout",
                          "LCRT source construct timeout",
                          TimeValue(MilliSeconds(600)),
                          MakeTimeAccessor(&RoutingProtocol::m_lcrtConstructSourceTimeout),
                          MakeTimeChecker())
            .AddAttribute("MessagePeriod",
                          "LCRT expected message period",
                          TimeValue(MilliSeconds(600)),
                          MakeTimeAccessor(&RoutingProtocol::m_lcrtMessagePeriod),
                          MakeTimeChecker())
            .AddAttribute("Gamma",
                          "LCRT Gamma value",
                          UintegerValue(3),
                          MakeUintegerAccessor(&RoutingProtocol::m_lcrtGamma),
                          MakeUintegerChecker<uint8_t>())
            .AddAttribute("UniformRv",
                          "Access to the underlying UniformRandomVariable",
                          StringValue("ns3::UniformRandomVariable"),
                          MakePointerAccessor(&RoutingProtocol::m_uniformRandomVariable),
                          MakePointerChecker<UniformRandomVariable>())
            .AddTraceSource("Parent",
                            "The parent node received in an AreaInfo control message",
                            MakeTraceSourceAccessor(&RoutingProtocol::m_parentTrace),
                            "ns3::Lcrt::TracedCallback");

    return tid;
}

RoutingProtocol::~RoutingProtocol()
{
}

void
RoutingProtocol::DoDispose()
{
    m_ipv4 = nullptr;

    // for (auto iter = m_socketAddresses.begin(); iter != m_socketAddresses.end(); iter++)
    // {
    //     iter->first->Close();
    // }
    // if (m_socket)
    // {
    //     m_socket->Close();
    //     m_socket = nullptr;
    // }
    // m_socketAddresses.clear();
    m_address.reset();
    // for (auto iter = m_socketSubnetBroadcastAddresses.begin(); iter !=
    // m_socketSubnetBroadcastAddresses.end(); iter++)
    // {
    //     iter->first->Close();
    // }
    if (m_socketBroadcast)
    {
        m_socketBroadcast->Close();
        m_socketBroadcast = nullptr;
    }
    // m_socketSubnetBroadcastAddresses.clear();

    if (m_internal)
    {
        lcrt_area_drop(m_internal);
        m_internal = nullptr;
    }

    Ipv4RoutingProtocol::DoDispose();
}

void
RoutingProtocol::PrintRoutingTable(Ptr<OutputStreamWrapper> stream, Time::Unit unit) const
{
    std::ostream* os = stream->GetStream();
    // Copy the current ostream state
    std::ios oldState(nullptr);
    oldState.copyfmt(*os);

    Ptr<Node> node = GetObject<Node>();
    *os << "Node: " << node->GetId() << "; Time: " << Now().As(unit)
        << ", Local time: " << node->GetLocalTime().As(unit) << ", LCRT Routing table" << std::endl;

    *os << std::resetiosflags(std::ios::adjustfield) << std::setiosflags(std::ios::left);
    *os << std::setw(16) << "Destination";
    *os << std::setw(16) << "Gateway";
    *os << "Hops" << std::endl;

    uint16_t hop_distance;
    if (lcrt_area_get_hop_distance(m_internal, &hop_distance))
    {
        m_addressMulticast.Print(*os);
        *os << "\t";
        Ipv4Address::GetBroadcast().Print(*os);
        *os << "\t";
        *os << hop_distance << std::endl;
    }

    *stream->GetStream() << std::endl;
}

Ptr<Ipv4Route>
RoutingProtocol::RouteOutput(Ptr<Packet> p,
                             const Ipv4Header& header,
                             Ptr<NetDevice> oif,
                             Socket::SocketErrno& sockerr)
{
    NS_LOG_FUNCTION(this << header << (oif ? oif->GetIfIndex() : 0));
    sockerr = Socket::ERROR_NOROUTETOHOST;
    if (!p)
    {
        NS_ABORT_MSG("TODO: Handle packet is == 0");
    }
    if (!m_socketBroadcast)
    {
        NS_LOG_LOGIC("No lcrt interfaces");
        Ptr<Ipv4Route> route;
        return route;
    }

    Ipv4Address dst = header.GetDestination();

    if (!lcrt_area_is_forwarder(m_internal, dst.Get()))
    {
        // NS_ABORT_MSG("LCRT not handling routes to address " << dst); // TODO: remove
        NS_LOG_LOGIC("LCRT not handling routes to address " << dst);
        Ptr<Ipv4Route> route;
        return route;
    }

    PacketMetadata::ItemIterator it = p->BeginItem();
    while (it.HasNext())
    {
        auto item = it.Next();
        NS_ASSERT(item.tid != LcrtHeader::GetTypeId());
    }
    // PacketMetadata::ItemIterator it = p->BeginItem();
    // if (it.HasNext())
    // {
    //     PacketMetadata::Item first = it.Next();
    //     if (first.tid == LcrtHeader::GetTypeId())
    //     {
    //         NS_ABORT_IF(first.tid == LcrtHeader::GetTypeId())
    //         throw "fail";
    //         return Ptr<Ipv4Route>();
    //         LcrtHeader lcrtHeader;
    //         p->RemoveHeader(lcrtHeader);
    //         hasHeader = true;
    //     }
    // }

    uint8_t pid;
    if (!lcrt_area_next_packet_id(m_internal, &pid))
    {
        NS_LOG_ERROR("LCRT not handling messages from this node");
        Ptr<Ipv4Route> route;
        return route;
    }
    LcrtHeader lcrtHeader;
    lcrtHeader.SetLastForwarder(m_address.value().GetAddress());
    lcrtHeader.SetPacketId(pid);
    p->AddHeader(lcrtHeader);

    sockerr = Socket::ERROR_NOTERROR;

    if (!oif)
    {
        oif = m_ipv4->GetNetDevice(1);
    }

    Ptr<Ipv4Route> route = Create<Ipv4Route>();
    route->SetDestination(dst);
    route->SetGateway(Ipv4Address::GetZero());
    route->SetOutputDevice(oif);
    route->SetSource(m_ipv4->GetAddress(m_ipv4->GetInterfaceForDevice(oif), 0).GetAddress());

    NS_LOG_LOGIC("Routing via " << route->GetGateway() << " on " << route->GetOutputDevice()
                                << " to " << route->GetDestination());
    return route;
}

bool
RoutingProtocol::RouteInput(Ptr<const Packet> p,
                            const Ipv4Header& header,
                            Ptr<const NetDevice> idev,
                            const UnicastForwardCallback& ucb,
                            const MulticastForwardCallback& mcb,
                            const LocalDeliverCallback& lcb,
                            const ErrorCallback& ecb)
{
    NS_LOG_FUNCTION(this << p->GetUid() << header.GetDestination() << idev->GetAddress());
    NS_ASSERT(m_ipv4);
    // Check if input device supports IP
    NS_ASSERT(m_ipv4->GetInterfaceForDevice(idev) >= 0);
    uint32_t iif = m_ipv4->GetInterfaceForDevice(idev);

    Ipv4Address dst = header.GetDestination();
    Ipv4Address origin = header.GetSource();

    if (header.GetProtocol() != UdpL4Protocol::PROT_NUMBER)
    {
        NS_LOG_WARN("LCRT only supports UDP");
        return false;
    }

    // Duplicate of my own packet
    if (origin == m_address.value().GetAddress())
    {
        return true;
    }

    if (dst.IsBroadcast())
    {
        // TODO: add duplicate packet detection

        if (!lcb.IsNull())
        {
            NS_LOG_LOGIC("Broadcast local delivery to " << m_address.value().GetAddress());
            lcb(p, header, iif);
        }
        else
        {
            NS_LOG_ERROR("Unable to deliver packet locally due to null callback "
                         << p->GetUid() << " from " << origin);
            ecb(p, header, Socket::ERROR_NOROUTETOHOST);
        }

        if (header.GetProtocol() == UdpL4Protocol::PROT_NUMBER)
        {
            UdpHeader udpHeader;
            p->PeekHeader(udpHeader);
            if (udpHeader.GetDestinationPort() == LCRT_PORT)
            {
                // LCRT packets sent in broadcast are already managed
                return true;
            }
        }

        NS_ABORT_MSG("TODO: handle additional broadcast packets");
    }

    if (dst != m_addressMulticast)
    {
        NS_LOG_WARN("LCRT not handling destination " << dst);
        return false;
    }

    // std::cout << "==== HERE ====" << std::endl;

    // p->Print(std::cout);
    // std::cout << std::endl;

    Ptr<Packet> pCopy = p->Copy();
    PacketMetadata::ItemIterator it = pCopy->BeginItem();
    // NS_ASSERT_MSG(it.HasNext(), "Has Packet::EnablePrinting() been set?");
    // NS_ASSERT(it.Next().tid == UdpHeader::GetTypeId());
    UdpHeader udpHeader;
    pCopy->RemoveHeader(udpHeader);
    it = pCopy->BeginItem();
    // NS_ASSERT(it.HasNext());
    // NS_ASSERT(it.Next().tid == LcrtHeader::GetTypeId());
    LcrtHeader lcrtHeader;
    pCopy->RemoveHeader(lcrtHeader);

    bool isChild = lcrt_area_is_parent(m_internal, lcrtHeader.GetLastForwarder().Get());
    if (!isChild)
    {
        return true;
    }

    // NS_LOG_DEBUG("LCRT Header: " << lcrtHeader);
    // pCopy->Print(std::cout);
    // std::cout << std::endl;

    // Buffer b(header.GetSerializedSize());
    // header.Serialize(b.Begin());
    // LcrtHeader h;
    // h.Deserialize(b.Begin());
    // NS_LOG_DEBUG("== HEADER == " << h);

    // if (dst == m_addressMulticast)
    // {
    //     uint32_t removed = pCopy->RemoveHeader(lcrtHeader);
    //     NS_ASSERT(removed == lcrtHeader.GetSerializedSize());
    //     NS_LOG_LOGIC("Removed header: " << lcrtHeader);
    //     pCopy->Print(std::cout);
    //     std::cout << std::endl;
    // }

    NS_LOG_LOGIC(dst << " is being listened to: " << m_ipv4->IsDestinationAddress(dst, iif)
                     << " (packet id: " << (uint32_t)lcrtHeader.GetPacketId()
                     << ", forwarder: " << lcrtHeader.GetLastForwarder() << ")");

    // if (m_ipv4->IsDestinationAddress(dst, iif))

    LcrtResponse response;
    lcrt_area_notify_received_packet(m_internal, lcrtHeader.GetPacketId(), &response);
    HandleResponse(response);

    if (!lcb.IsNull())
    {
        NS_LOG_LOGIC("Local delivery to " << m_address.value().GetAddress());
        pCopy->AddHeader(udpHeader);
        lcb(p, header, iif);
        pCopy->RemoveAtStart(udpHeader.GetSerializedSize());
    }
    else
    {
        NS_LOG_ERROR("Unable to deliver packet locally due to null callback "
                     << p->GetUid() << " from " << origin);
        ecb(p, header, Socket::ERROR_NOROUTETOHOST);
    }

    bool isForwarder = lcrt_area_is_forwarder(m_internal, dst.Get());
    bool forward = isForwarder && isChild;
    NS_LOG_LOGIC("Is forwarder for " << dst << ": " << isForwarder << ", is child of "
                                     << lcrtHeader.GetLastForwarder() << ": " << isChild);

    if (!forward)
    {
        return true;
    }

    lcrtHeader.SetLastForwarder(m_address.value().GetAddress());
    pCopy->AddHeader(lcrtHeader);
    pCopy->AddHeader(udpHeader);

    Ptr<Ipv4MulticastRoute> route = Create<Ipv4MulticastRoute>();
    route->SetGroup(dst);
    route->SetOrigin(origin);
    NS_LOG_DEBUG("== origin: " << origin << " ==");
    route->SetParent(iif);
    for (uint32_t i = 0; i < m_ipv4->GetNInterfaces(); i++)
    {
        // if (i != iif)
        // {
        route->SetOutputTtl(i, 1);
        // }
    }
    NS_LOG_LOGIC("Multicast forwarding packet");
    mcb(route, pCopy, header);
    // pCopy->Print(std::cout);
    // std::cout << std::endl;

    return true;
}

void
RoutingProtocol::SetIpv4(Ptr<Ipv4> ipv4)
{
    NS_LOG_FUNCTION(this << ipv4);
    NS_ASSERT(ipv4);
    NS_ASSERT(!m_ipv4);

    m_ipv4 = ipv4;

    // Assert that the only one interface up for now is loopback
    NS_ASSERT(m_ipv4->GetNInterfaces() == 1 &&
              m_ipv4->GetAddress(0, 0).GetAddress() == Ipv4Address("127.0.0.1"));

    // Ipv4Address address = ipv4->GetAddress(0, 0).GetAddress();

    // NodeInfo* node_info = NodeInfo::create(address);
    // m_internal = node_info->node;

    // NS_ABORT_MSG("TODO: SetIpv4, ip: " << m_ipv4->GetAddress(0, 0) << " ===> " << ipv4);
}

void
RoutingProtocol::NotifyInterfaceUp(uint32_t i)
{
    NS_LOG_FUNCTION(this << m_ipv4->GetAddress(i, 0).GetAddress());

    Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol>();
    if (l3->GetNAddresses(i) > 1)
    {
        NS_LOG_WARN("LCRT does not work with more then one address per interface.");
    }
    Ipv4InterfaceAddress iface = l3->GetAddress(i, 0);
    if (iface.GetAddress() == Ipv4Address("127.0.0.1"))
    {
        NS_ABORT_MSG("TODO: decide what do do here (loopback = ignore?)");
        return;
    }
    if (m_socketBroadcast)
    {
        NS_LOG_WARN("LCRT does not support multiple interfaces");
        return;
    }

    // Ptr<NetDevice> dev =
    // m_ipv4->GetNetDevice(m_ipv4->GetInterfaceForAddress(iface.GetAddress())); Ptr<WifiNetDevice>
    // wifi = dev->GetObject<WifiNetDevice>(); if (!wifi)
    // {
    //     NS_LOG_WARN("LCRT only supports Wifi");
    // }

    AddAddress(i, iface, l3, iface);
}

void
RoutingProtocol::NotifyInterfaceDown(uint32_t i)
{
    NS_LOG_FUNCTION(this << m_ipv4->GetAddress(i, 0).GetAddress());

    Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol>();

    if (m_ipv4->GetAddress(i, 0) != m_address)
    {
        return;
    }
    // NS_ASSERT(m_socket);
    // m_socket->Close();
    // m_socket = nullptr;
    NS_ASSERT(m_socketBroadcast);
    m_socketBroadcast->Close();
    m_socketBroadcast = nullptr;

    for (size_t i = 1; i <= 2; i++)
    {
        m_timeout[i].Cancel();
    }

    NS_ASSERT(m_internal);
    lcrt_area_drop(m_internal);
}

void
RoutingProtocol::NotifyAddAddress(uint32_t i, Ipv4InterfaceAddress address)
{
    NS_LOG_FUNCTION(this << " interface " << i << " address " << address);

    Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol>();
    if (!l3->IsUp(i))
    {
        return;
    }

    if (l3->GetNAddresses(i) != 1)
    {
        NS_LOG_WARN("LCRT does support more then one address per interface.");
    }

    Ipv4InterfaceAddress iface = l3->GetAddress(i, 0);
    if (iface.GetAddress() == Ipv4Address("127.0.0.1"))
    {
        return;
    }

    if (m_address)
    {
        NS_LOG_WARN("LCRT does not support more than one address at once.");
        return;
    }

    AddAddress(i, address, l3, iface);
}

void
RoutingProtocol::AddAddress(uint32_t i,
                            Ipv4InterfaceAddress address,
                            Ptr<Ipv4L3Protocol> l3,
                            Ipv4InterfaceAddress iface)
{
    m_address.emplace(address);

    // Create also a subnet broadcast socket
    Ptr<Socket> socket = Socket::CreateSocket(GetObject<Node>(), UdpSocketFactory::GetTypeId());
    NS_ASSERT(socket);
    socket->SetRecvCallback(MakeCallback(&RoutingProtocol::RecvLcrt, this));
    socket->BindToNetDevice(l3->GetNetDevice(i));
    socket->Bind(InetSocketAddress(Ipv4Address::GetBroadcast(), LCRT_PORT));
    socket->SetAllowBroadcast(true);
    m_socketBroadcast = socket;

    LcrtNodeInfo nodeInfo{
        this,
        RoutingProtocol::lcrt_PositionCall,
        RoutingProtocol::lcrt_CurrentBitrateCall,
        RoutingProtocol::lcrt_InterferingNeighboursCall,
    };

    if (m_isSource)
    {
        LcrtResponse response;
        m_internal = lcrt_area_source_new(lcrt_Config(),
                                          nodeInfo,
                                          iface.GetAddress().Get(),
                                          m_addressMulticast.Get(),
                                          &response);
        HandleResponse(response);
    }
    else
    {
        m_internal = lcrt_area_new(lcrt_Config(),
                                   nodeInfo,
                                   iface.GetAddress().Get(),
                                   m_addressMulticast.Get());
    }
}

void
RoutingProtocol::NotifyRemoveAddress(uint32_t i, Ipv4InterfaceAddress address)
{
    NS_LOG_FUNCTION(this);

    NS_ABORT_MSG("TODO: NotifyRemoveAddress");
}

void
RoutingProtocol::RecvLcrt(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);

    Address sourceAddress;
    Ptr<Packet> packet = socket->RecvFrom(sourceAddress);
    InetSocketAddress inetSourceAddr = InetSocketAddress::ConvertFrom(sourceAddress);
    Ipv4Address sender = inetSourceAddr.GetIpv4();
    // InetSocketAddress inetSourceAddr = InetSocketAddress::ConvertFrom(sourceAddress);
    // Ipv4Address sender = inetSourceAddr.GetIpv4();
    Ipv4Address receiver = m_address.value().GetAddress();

    // if (socket != m_socket && socket != m_socketBroadcast)
    if (socket != m_socketBroadcast)
    {
        NS_ABORT_MSG("Received a packet from an unknown socket");
    }

    // if (m_socketAddresses.find(socket) != m_socketAddresses.end())
    // {
    //     receiver = m_socketAddresses[socket].GetAddress();
    // }
    // else if (m_socketSubnetBroadcastAddresses.find(socket) !=
    //          m_socketSubnetBroadcastAddresses.end())
    // {
    //     receiver = m_socketSubnetBroadcastAddresses[socket].GetAddress();
    // }
    // else
    // {
    //     NS_ASSERT_MSG(false, "Received a packet from an unknown socket");
    // }
    NS_LOG_DEBUG("LCRT node " << this << " received a LCRT packet from " << sender << " to "
                              << receiver);

    Ipv4Header header;
    packet->RemoveHeader(header);

    uint32_t size = packet->GetSize();
    auto* buffer = new uint8_t[size];
    uint32_t size_read = packet->CopyData(buffer, size);
    NS_ASSERT(size_read == size);

    LcrtResponse response;
    lcrt_area_handle_message(m_internal, LcrtMessage{buffer, size}, &response);
    HandleResponse(response);

    delete[] buffer;
}

void
RoutingProtocol::HandleResponse(LcrtResponse response)
{
    NS_LOG_FUNCTION(this << response.message.len << response.timeout.id << response.timeout.delay
                         << static_cast<size_t>(response.event.tag));
    if (response.timeout.id)
    {
        m_timeout[response.timeout.id - 1].Cancel();
        m_timeout[response.timeout.id - 1].Schedule(NanoSeconds(response.timeout.delay));
    }

    if (response.message.data)
    {
        Ipv4Header header;
        Ipv4InterfaceAddress iface = m_address.value();
        header.SetSource(iface.GetAddress());
        header.SetDestination(iface.GetBroadcast());

        Ptr<Socket> socket = m_socketBroadcast;

        Ptr<Packet> packet = Create<Packet>(response.message.data, response.message.len);
        lcrt_message_drop(response.message);
        packet->AddHeader(header);

        // Ipv4Address destination = iface.GetBroadcast();

        NS_LOG_DEBUG("Send routing information with id " << header.GetIdentification()
                                                         << " to socket");
        Simulator::Schedule(MilliSeconds(m_uniformRandomVariable->GetInteger(0, 20)),
                            &RoutingProtocol::SendTo,
                            m_socketBroadcast,
                            packet,
                            Ipv4Address::GetBroadcast());
    }

    switch (response.event.tag)
    {
    case LcrtEvent::Tag::None:
        break;

    case LcrtEvent::Tag::Parent:
        m_parentTrace(Ipv4Address(response.event.parent._0));
        break;
    }
}

void
RoutingProtocol::HandleTimeout(uint8_t timeoutId)
{
    NS_LOG_FUNCTION(this << timeoutId);
    LcrtResponse response;
    lcrt_area_handle_timeout(m_internal, timeoutId, &response);
    HandleResponse(response);
}

void
RoutingProtocol::SetIsSource(bool isSource)
{
    if (m_socketBroadcast)
    {
        NS_ABORT_MSG("LCRT source status cannot be changed after adding interfaces.");
    }
    m_isSource = isSource;
}

bool
RoutingProtocol::GetIsSource() const
{
    return m_isSource;
}

const LcrtArea*
RoutingProtocol::GetInternal() const
{
    return m_internal;
}

void
RoutingProtocol::ChangeParent(Ipv4Address newParent)
{
    NS_LOG_FUNCTION(newParent);
    NS_ASSERT(m_internal);

    LcrtResponse response;
    lcrt_area_change_parent(m_internal, newParent.Get(), &response);
    HandleResponse(response);
}

void
RoutingProtocol::SendTo(Ptr<Socket> socket, Ptr<Packet> packet, Ipv4Address destination)
{
    socket->SendTo(packet, 0, InetSocketAddress(destination, LCRT_PORT));
}

LcrtConfig
RoutingProtocol::lcrt_Config() const
{
    return LcrtConfig{m_lcrtK,
                      m_lcrtRadius,
                      m_lcrtBitrateCapacity,
                      static_cast<uint64_t>(m_lcrtConstructTimeout.GetNanoSeconds()),
                      static_cast<uint64_t>(m_lcrtConstructSourceTimeout.GetNanoSeconds()),
                      static_cast<uint64_t>(m_lcrtMessagePeriod.GetNanoSeconds()),
                      m_lcrtGamma};
}

LcrtPosition
RoutingProtocol::lcrt_Position()
{
    Ptr<MobilityModel> mobility = GetObject<MobilityModel>();
    NS_ASSERT(mobility);
    Vector pos = mobility->GetPosition();
    return LcrtPosition{pos.x, pos.y, pos.z};
}

LcrtPosition
RoutingProtocol::lcrt_PositionCall(void* ctx)
{
    return static_cast<RoutingProtocol*>(ctx)->lcrt_Position();
}

float
RoutingProtocol::lcrt_CurrentBitrate()
{
    NS_LOG_WARN("TODO: current bitrate");
    return 0;
}

float
RoutingProtocol::lcrt_CurrentBitrateCall(void* ctx)
{
    return static_cast<RoutingProtocol*>(ctx)->lcrt_CurrentBitrate();
}

uint16_t
RoutingProtocol::lcrt_InterferingNeighbours()
{
    NS_LOG_WARN("TODO: interfering neighbours");
    return 0;
}

uint16_t
RoutingProtocol::lcrt_InterferingNeighboursCall(void* ctx)
{
    return static_cast<RoutingProtocol*>(ctx)->lcrt_InterferingNeighbours();
}

} // namespace lcrt

/* ... */

} // namespace ns3
