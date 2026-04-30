/*
 * Copyright (c) 2026 Tom Boddaert
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Tom Boddaert <lcrt-ns3@tomboddaert.com>
 *
 * This file is based on aodv-routing-protocol.h by Elena Buchatskaia <borovkovaes@iitp.ru> and
 * Pavel Boyko <boyko@iitp.ru>
 */

#ifndef LCRT_H
#define LCRT_H

/**
 * @defgroup lcrt LCRT routing algorithm
 */

#include "ns3/ipv4-interface.h"
#include "ns3/ipv4-l3-protocol.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/liblcrt.hpp"
#include "ns3/node.h"
#include "ns3/output-stream-wrapper.h"
#include "ns3/random-variable-stream.h"
#include "ns3/timer.h"

#include <optional>

namespace ns3
{

// Each class should be documented using Doxygen,
// and have an @ingroup lcrt directive

namespace lcrt
{

/**
 * @ingroup lcrt
 *
 * @brief LCRT routing protocol
 */
class RoutingProtocol : public Ipv4RoutingProtocol
{
  public:
    /**
     * @brief Get the type ID.
     * @return the object TypeId
     */
    static TypeId GetTypeId();
    static const uint32_t LCRT_PORT;

    RoutingProtocol();
    ~RoutingProtocol() override;
    void DoDispose() override;

    // Inherited from Ipv4RoutingProtocol
    Ptr<Ipv4Route> RouteOutput(Ptr<Packet> p,
                               const Ipv4Header& header,
                               Ptr<NetDevice> oif,
                               Socket::SocketErrno& sockerr) override;
    bool RouteInput(Ptr<const Packet> p,
                    const Ipv4Header& header,
                    Ptr<const NetDevice> idev,
                    const UnicastForwardCallback& ucb,
                    const MulticastForwardCallback& mcb,
                    const LocalDeliverCallback& lcb,
                    const ErrorCallback& ecb) override;
    void NotifyInterfaceUp(uint32_t interface) override;
    void NotifyInterfaceDown(uint32_t interface) override;
    void NotifyAddAddress(uint32_t interface, Ipv4InterfaceAddress address) override;
    void NotifyRemoveAddress(uint32_t interface, Ipv4InterfaceAddress address) override;
    void SetIpv4(Ptr<Ipv4> ipv4) override;
    void PrintRoutingTable(Ptr<OutputStreamWrapper> stream,
                           Time::Unit unit = Time::S) const override;

    void SetIsSource(bool isSource);
    bool GetIsSource() const;
    LcrtArea const* GetInternal() const;
    void ChangeParent(Ipv4Address newParent);

    typedef void (*TracedCallback)(Ipv4Address parent);

  private:
    /// IP protocol
    Ptr<Ipv4> m_ipv4;
    // /// Raw unicast socket per each IP interface, map socket -> iface address (IP + mask)
    // std::map<Ptr<Socket>, Ipv4InterfaceAddress> m_socketAddresses;
    // /// Raw subnet directed broadcast socket per each IP interface, map socket -> iface address
    // (IP
    // /// + mask)
    // std::map<Ptr<Socket>, Ipv4InterfaceAddress> m_socketSubnetBroadcastAddresses;
    Ptr<Socket> m_socketBroadcast;
    std::optional<Ipv4InterfaceAddress> m_address;
    Ipv4Address m_addressMulticast;
    /// Provides uniform random variables.
    Ptr<UniformRandomVariable> m_uniformRandomVariable;

    // LcrtConfig
    uint16_t m_lcrtK;
    double m_lcrtRadius;
    float m_lcrtBitrateCapacity;
    Time m_lcrtConstructTimeout, m_lcrtConstructSourceTimeout, m_lcrtMessagePeriod;
    uint8_t m_lcrtGamma;
    // LcrtConfig m_lcrtConfig;
    LcrtArea* m_internal;
    /// LCRT timeouts
    Timer m_timeout[2];
    bool m_isSource;
    ns3::TracedCallback<Ipv4Address> m_parentTrace;

    /**
     * Receive and process control packet
     * @param socket input socket
     */
    void RecvLcrt(Ptr<Socket> socket);
    void HandleResponse(LcrtResponse response);
    void HandleTimeout(uint8_t timeoutId);
    void AddAddress(uint32_t i,
                    Ipv4InterfaceAddress address,
                    Ptr<Ipv4L3Protocol> l3,
                    Ipv4InterfaceAddress iface);
    /**
     * Send packet to destination socket
     * @param packet packet to send
     * @param destination destination node IP address
     */
    static void SendTo(Ptr<Socket> socket, Ptr<Packet> packet, Ipv4Address destination);

    LcrtConfig lcrt_Config() const;

    LcrtPosition lcrt_Position();
    static LcrtPosition lcrt_PositionCall(void* ctx);
    float lcrt_CurrentBitrate();
    static float lcrt_CurrentBitrateCall(void* ctx);
    uint16_t lcrt_InterferingNeighbours();
    static uint16_t lcrt_InterferingNeighboursCall(void* ctx);
};

} // namespace lcrt

} // namespace ns3

#endif // LCRT_H
