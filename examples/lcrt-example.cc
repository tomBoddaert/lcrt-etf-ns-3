/*
 * Copyright (c) 2026 Tom Boddaert
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Tom Boddaert <lcrt-ns3@tomboddaert.com>
 *
 * This file is based on aodv-example.cc by Pavel Boyko <boyko@iitp.ru>
 */

#include "ns3/core-module.h"
#include "ns3/etf-helper.h"
#include "ns3/etf.h"
#include "ns3/internet-module.h"
#include "ns3/lcrt-helper.h"
#include "ns3/lcrt.h"
#include "ns3/liblcrt.hpp"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/network-module.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/ping-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/yans-wifi-helper.h"

#include <iostream>
#include <memory>
#include <sstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LcrtExample");

/**
 * @defgroup lcrt-examples LCRT Examples
 * @ingroup lcrt
 * @ingroup examples
 */

/**
 * @ingroup lcrt-examples
 * @ingroup examples
 * @brief Test script.
 *
 * TODO: replace this with the new description
 * This script creates 1-dimensional grid topology and then ping last node from
 * the first one:
 *
 * [10.0.0.1] <-- step --> [10.0.0.2] <-- step --> [10.0.0.3] <-- step -->
 * [10.0.0.4]
 *
 * ping 10.0.0.4
 *
 * When 1/3 of simulation time has elapsed, one of the nodes is moved out of
 * range, thereby breaking the topology.  By default, this will result in
 * stopping ping replies reception after sequence number 33. If the step size is
 * reduced to cover the gap, then also the following pings can be received.
 */
class LcrtExample
{
  public:
    LcrtExample();
    /**
     * @brief Configure script parameters
     * @param argc is the command line argument count
     * @param argv is the command line arguments
     * @return true on successful configuration
     */
    bool Configure(int argc, char** argv);
    /// Run simulation
    void Run();
    /**
     * Report results
     * @param os the output stream
     */
    void Report(std::ostream& os);

  private:
    // parameters
    /// Number of nodes
    uint32_t size;
    /// Distance between nodes, meters
    double step;
    /// Simulation time, seconds
    double totalTime;
    /// Write per-device PCAP traces if true
    bool pcap;
    /// Print routes if true
    bool printRoutes;
    /// File to write the animation to
    std::string animFile;

    // network
    /// nodes used in the example
    NodeContainer nodes;
    /// devices used in the example
    NetDeviceContainer devices;
    /// interfaces used in the example
    Ipv4InterfaceContainer interfaces;
    /// animation recorder
    std::unique_ptr<AnimationInterface> anim;

  private:
    /// Create the nodes
    void CreateNodes();
    /// Create the devices
    void CreateDevices();
    /// Create the network
    void InstallInternetStack();
    /// Create the network
    void InstallInternetStack2();
    /// Create the simulation applications
    void InstallApplications();
    /// Create the NetAnim recorder
    void InstallAnimationRecorder();
};

static Vector positions[] = {
    Vector(70, 140, 35),
    Vector(40, 110, 20),
    Vector(110, 120, 20),
    Vector(0, 110, 0),
    Vector(20, 70, 20),
    Vector(120, 80, 40),
    Vector(10, 30, 30),
    Vector(120, 40, 20),
    Vector(100, 0, 30),
    Vector(0, 20, 50),
};

int
main(int argc, char** argv)
{
    Packet::EnablePrinting();

    LcrtExample test;
    if (!test.Configure(argc, argv))
    {
        NS_FATAL_ERROR("Configuration failed. Aborted.");
    }

    test.Run();
    test.Report(std::cout);
    return 0;
}

//-----------------------------------------------------------------------------
LcrtExample::LcrtExample()
    : size(10),
      step(50),
      totalTime(100),
      pcap(true),
      printRoutes(true),
      animFile("lcrt-example-animation.xml"),
      anim(nullptr)
{
}

bool
LcrtExample::Configure(int argc, char** argv)
{
    // Enable LCRT logs by default. Comment this if too noisy
    // LogComponentEnable("LcrtRoutingProtocol", LOG_LEVEL_ALL);

    SeedManager::SetSeed(12345);
    CommandLine cmd(__FILE__);

    cmd.AddValue("pcap", "Write PCAP traces.", pcap);
    cmd.AddValue("printRoutes", "Print routing table dumps.", printRoutes);
    // TODO: remove or change
    // cmd.AddValue("size", "Number of nodes.", size);
    cmd.AddValue("time", "Simulation time, s.", totalTime);
    cmd.AddValue("step", "Grid step, m", step);
    cmd.AddValue("animFile", "File to write NetAnim animation to", animFile);

    size = sizeof(positions) / sizeof(*positions);

    cmd.Parse(argc, argv);
    return true;
}

void
LcrtExample::Run()
{
    //  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold",
    //  UintegerValue (1)); // enable rts cts all the time.
    CreateNodes();
    CreateDevices();
    InstallInternetStack();
    InstallApplications();
    InstallAnimationRecorder();

    std::cout << "Starting simulation for " << totalTime << " s ...\n";

    Simulator::Stop(Seconds(totalTime));
    Simulator::Run();
    Simulator::Destroy();
}

void
LcrtExample::Report(std::ostream&)
{
}

void
LcrtExample::CreateNodes()
{
    std::cout << "Creating " << (unsigned)size << " nodes " << step << " m apart.\n";
    nodes.Create(size);
    // Name nodes
    for (uint32_t i = 0; i < size; ++i)
    {
        // std::ostringstream os;
        // os << "node-" << i;
        // Names::Add(os.str(), nodes.Get(i));
        Names::Add(std::string(1, 'A' + i), nodes.Get(i));
    }

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    for (size_t i = 0; i < size; i++)
    {
        Vector pos = positions[i];
        pos.x *= 2.0 / 3.0;
        pos.y *= 2.0 / 3.0;
        pos.z *= 2.0 / 3.0;
        positionAlloc->Add(pos);
    }
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::WaypointMobilityModel");
    mobility.Install(nodes);
}

void
LcrtExample::CreateDevices()
{
    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");
    YansWifiPhyHelper wifiPhy;
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());
    WifiHelper wifi;
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode",
                                 StringValue("OfdmRate6Mbps"),
                                 "RtsCtsThreshold",
                                 UintegerValue(0));
    devices = wifi.Install(wifiPhy, wifiMac, nodes);

    if (pcap)
    {
        wifiPhy.EnablePcapAll(std::string("lcrt"));
    }
}

void
LcrtExample::InstallInternetStack()
{
    LcrtHelper lcrt;
    // you can configure LCRT attributes here using lcrt.Set(name, value)
    lcrt.Set("Radius", DoubleValue(50.0 * 2.0 / 3.0));
    InternetStackHelper stack;
    stack.SetRoutingHelper(lcrt); // has effect on the next Install ()
    stack.Install(nodes);
    nodes.Get(0)->GetObject<lcrt::RoutingProtocol>()->SetIsSource(true);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.0.0.0");
    interfaces = address.Assign(devices);

    if (printRoutes)
    {
        Ptr<OutputStreamWrapper> routingStream =
            Create<OutputStreamWrapper>("lcrt.routes", std::ios::out);
        Ipv4RoutingHelper::PrintRoutingTableAllAt(Seconds(8), routingStream);
    }
}

void
LcrtExample::InstallInternetStack2()
{
    Ipv4StaticRoutingHelper staticRouting;
    InternetStackHelper stack;
    stack.SetRoutingHelper(staticRouting); // has effect on the next Install ()
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.0.0.0");
    interfaces = address.Assign(devices);

    for (auto device = devices.Begin(); device != devices.End(); device++)
    {
        Ptr<Node> node = (*device)->GetNode();
        std::string name = Names::FindName(node);

        if (name == "A")
        {
            // route for host
            // Use host routing entry according to note in Ipv4StaticRouting::RouteOutput:
            auto ipv4 = node->GetObject<Ipv4>();
            NS_ASSERT_MSG((bool)ipv4,
                          "Node " << Names::FindName(node) << " does not have Ipv4 aggregate");
            auto routing = staticRouting.GetStaticRouting(ipv4);
            routing->AddHostRouteTo("224.1.1.1", ipv4->GetInterfaceForDevice(*device), 0);
        }
        else
        {
            // route for forwarding
            staticRouting.AddMulticastRoute(node,
                                            Ipv4Address::GetAny(),
                                            "224.1.1.1",
                                            *device,
                                            NetDeviceContainer(*device));
        }
    }

    if (printRoutes)
    {
        Ptr<OutputStreamWrapper> routingStream =
            Create<OutputStreamWrapper>("static.routes", std::ios::out);
        Ipv4RoutingHelper::PrintRoutingTableAllAt(Seconds(8), routingStream);
    }
}

void
LcrtExample::InstallApplications()
{
    Ipv4Address multicastGroup("224.1.1.1");
    uint16_t port = 9;

    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(multicastGroup, port));
    ApplicationContainer sinkApps;
    for (uint32_t i = 1; i < nodes.GetN(); i++)
    {
        sinkApps.Add(sink.Install(nodes.Get(i)));
    }
    sinkApps.Start(Seconds(1));
    sinkApps.Stop(Seconds(totalTime) - Seconds(0.001));

    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(multicastGroup, port));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetConstantRate(DataRate("8kbps"));
    ApplicationContainer sourceApps = onoff.Install(nodes.Get(0));
    sourceApps.Start(Seconds(1));
    sourceApps.Stop(Seconds(totalTime) - Seconds(0.001));

    EtfHelper etf;
    ApplicationContainer etfApps = etf.Install(nodes);

    // move node away
    Ptr<Node> node = nodes.Get(size - 1);
    Ptr<etf::EtfPathing> etfApp = etfApps.Get(size - 1)->GetObject<etf::EtfPathing>();
    NS_ASSERT(etfApp);
    Simulator::Schedule(Seconds(totalTime / 3),
                        &etf::EtfPathing::BeginTransition,
                        etfApp,
                        Vector(130 * (2.0 / 3.0), 130 * (2.0 / 3.0), 0));
}

void
LcrtExample::InstallAnimationRecorder()
{
    if (animFile.empty())
    {
        return;
    }

    anim = std::make_unique<AnimationInterface>(animFile);
    anim->EnablePacketMetadata();
    anim->EnableWifiPhyCounters(Seconds(0), Seconds(totalTime));
    anim->EnableIpv4L3ProtocolCounters(Seconds(0), Seconds(totalTime));
    anim->EnableWifiMacCounters(Seconds(0), Seconds(totalTime));

    for (auto node = nodes.Begin(); node != nodes.End(); node++)
    {
        anim->UpdateNodeDescription(*node, std::string(1, 'A' + (*node)->GetId()));
        anim->UpdateNodeColor(*node, 150, 150, 250);
        anim->UpdateNodeSize(*node, 3, 3);
        std::ostringstream name;
        name << "Node " << (*node)->GetId();
        anim->UpdateNodeDescription(*node, name.str());
    }

    anim->UpdateNodeColor(nodes.Get(0), 150, 250, 150);
}
