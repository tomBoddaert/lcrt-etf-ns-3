/*
 * Copyright (c) 2026 Tom Boddaert
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Tom Boddaert <lcrt-ns3@tomboddaert.com>
 *
 * This file is based on simple-multicast-flooding.cc by Tommaso Pecorella
 * <tommaso.pecorella@unifi.it> and Jared Dulmage <jared.dulmage@caliola.com>
 */

#include "ns3/boolean.h"
#include "ns3/config.h"
#include "ns3/core-module.h"
#include "ns3/data-rate.h"
#include "ns3/double.h"
#include "ns3/inet-socket-address.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-l3-protocol.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-static-routing.h"
#include "ns3/lcrt-helper.h"
#include "ns3/lcrt.h"
#include "ns3/log.h"
#include "ns3/mobility-module.h"
#include "ns3/names.h"
#include "ns3/node.h"
#include "ns3/on-off-helper.h"
#include "ns3/onoff-application.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/packet-sink.h"
#include "ns3/random-variable-stream.h"
#include "ns3/simple-channel.h"
#include "ns3/simple-net-device-helper.h"
#include "ns3/simple-net-device.h"
#include "ns3/simulator.h"
#include "ns3/socket.h"
#include "ns3/string.h"
#include "ns3/test.h"
#include "ns3/trace-helper.h"
#include "ns3/traffic-control-layer.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/udp-socket.h"
#include "ns3/uinteger.h"

#include <chrono>
#include <fstream>
#include <ios>
#include <sstream>
#include <string>

using namespace ns3;

/**
 * Network topology:
 *
 *        /---- B ----\
 *  A ----      |       ---- D ---- E
 *        \---- C ----/
 *
 * This example demonstrates configuration of
 * static routing to realize broadcast-like
 * flooding of packets from node A
 * across the illustrated topology.
 */
int
main(int argc, char* argv[])
{
    CommandLine cmd(__FILE__);
    bool useLcrt = true;
    cmd.AddValue("useLcrt", "Use LCRT (static if false)", useLcrt);
    uint32_t gridWidth = 5;
    cmd.AddValue("gridWidth", "Square root of the number of nodes", gridWidth);
    double time = 100;
    cmd.AddValue("time", "Amount of time to run the simulation for", time);
    cmd.Parse(argc, argv);

    // multicast target
    const std::string targetAddr = "224.1.1.1";
    Config::SetDefault("ns3::Ipv4L3Protocol::EnableDuplicatePacketDetection", BooleanValue(true));
    Config::SetDefault("ns3::Ipv4L3Protocol::DuplicateExpire", TimeValue(Seconds(10)));
    Packet::EnablePrinting();

    // Create topology

    // Create nodes
    auto nodes = NodeContainer();
    nodes.Create(gridWidth * gridWidth);

    // Name nodes
    for (uint32_t x = 0; x < gridWidth; x++)
    {
        for (uint32_t y = 0; y < gridWidth; y++)
        {
            std::stringstream buffer;
            buffer << x << ", " << y;
            Ptr<Node> node = nodes.Get(y * gridWidth + x);
            Names::Add(buffer.str(), node);
        }
    }
    // Names::Add("A", nodes.Get(0));
    // Names::Add("B", nodes.Get(1));
    // Names::Add("C", nodes.Get(2));
    // Names::Add("D", nodes.Get(3));
    // Names::Add("E", nodes.Get(4));

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX",
                                  DoubleValue(0.0),
                                  "MinY",
                                  DoubleValue(0.0),
                                  "DeltaX",
                                  DoubleValue(1),
                                  "DeltaY",
                                  DoubleValue(1),
                                  "GridWidth",
                                  UintegerValue(gridWidth));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    SimpleNetDeviceHelper simplenet;
    auto devices = simplenet.Install(nodes);
    // name devices
    // Names::Add("A/dev", devices.Get(0));
    // Names::Add("B/dev", devices.Get(1));
    // Names::Add("C/dev", devices.Get(2));
    // Names::Add("D/dev", devices.Get(3));
    // Names::Add("E/dev", devices.Get(4));

    // setup static routes to facilitate multicast flood
    Ipv4ListRoutingHelper listRouting;
    Ipv4StaticRoutingHelper staticRouting;
    LcrtHelper lcrt;
    if (useLcrt)
    {
        lcrt.Set("Radius", DoubleValue(1.2));
        lcrt.Set("K", UintegerValue(gridWidth * 3));
        listRouting.Add(lcrt, 0);
    }
    else
    {
        listRouting.Add(staticRouting, 0);
    }

    InternetStackHelper internet;
    internet.SetIpv6StackInstall(false);
    internet.SetIpv4ArpJitter(true);
    internet.SetRoutingHelper(listRouting);
    internet.Install(nodes);

    if (useLcrt)
    {
        nodes.Get(0)->GetObject<lcrt::RoutingProtocol>()->SetIsSource(true);
    }

    Ipv4AddressHelper ipv4address;
    ipv4address.SetBase("10.0.0.0", "255.255.255.0");
    ipv4address.Assign(devices);

    if (!useLcrt)
    {
        // add static routes for each node / device
        for (auto diter = devices.Begin(); diter != devices.End(); ++diter)
        {
            Ptr<Node> node = (*diter)->GetNode();

            if (Names::FindName(node) == "0, 0")
            {
                // route for host
                // Use host routing entry according to note in Ipv4StaticRouting::RouteOutput:
                //// Note:  Multicast routes for outbound packets are stored in the
                //// normal unicast table.  An implication of this is that it is not
                //// possible to source multicast datagrams on multiple interfaces.
                //// This is a well-known property of sockets implementation on
                //// many Unix variants.
                //// So, we just log it and fall through to LookupStatic ()
                auto ipv4 = node->GetObject<Ipv4>();
                NS_ASSERT_MSG((bool)ipv4,
                              "Node " << Names::FindName(node) << " does not have Ipv4 aggregate");
                auto routing = staticRouting.GetStaticRouting(ipv4);
                routing->AddHostRouteTo(targetAddr.c_str(), ipv4->GetInterfaceForDevice(*diter), 0);
            }
            else
            {
                // route for forwarding
                staticRouting.AddMulticastRoute(node,
                                                Ipv4Address::GetAny(),
                                                targetAddr.c_str(),
                                                *diter,
                                                NetDeviceContainer(*diter));
            }
        }
    }

    // set the topology, by default fully-connected
    auto channel = devices.Get(0)->GetChannel();
    auto simplechannel = channel->GetObject<SimpleChannel>();
    for (uint32_t ax = 0; ax < gridWidth; ax++)
    {
        for (uint32_t ay = 0; ay < gridWidth; ay++)
        {
            auto node = DynamicCast<SimpleNetDevice>(devices.Get(ay * gridWidth + ax));
            for (uint32_t bx = 0; bx < gridWidth; bx++)
            {
                for (uint32_t by = 0; by < gridWidth; by++)
                {
                    auto diff = [](uint32_t i, uint32_t j) { return i < j ? j - i : i - j; };
                    if (diff(ax, bx) + diff(ay, by) <= 1)
                    {
                        continue;
                    }
                    auto target = DynamicCast<SimpleNetDevice>(devices.Get(by * gridWidth + bx));
                    simplechannel->BlackList(node, target);
                    simplechannel->BlackList(target, node);
                }
            }
        }
    }
    // ensure some time progress between re-transmissions
    simplechannel->SetAttribute("Delay", TimeValue(MilliSeconds(1)));

    // sinks
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory",
                                InetSocketAddress(Ipv4Address::GetAny(), 9));
    ApplicationContainer sinks;
    for (uint32_t x = 0; x < gridWidth; x++)
    {
        for (uint32_t y = 0; y < gridWidth; y++)
        {
            Ptr<Node> node = nodes.Get(y * gridWidth + x);
            sinks.Add(sinkHelper.Install(node));
        }
    }
    sinks.Start(Seconds(1));

    // source
    OnOffHelper onoffHelper("ns3::UdpSocketFactory", InetSocketAddress(targetAddr.c_str(), 9));
    onoffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoffHelper.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    // onoffHelper.SetAttribute("MaxBytes", UintegerValue(10 * 1024));
    // onoffHelper.SetAttribute("MaxBytes", UintegerValue(1));
    auto source = onoffHelper.Install(nodes.Get(0));
    source.Start(Seconds(1.1));

    // pcap traces
    for (auto end = nodes.End(), iter = nodes.Begin(); iter != end; ++iter)
    {
        internet.EnablePcapIpv4("smf-trace", (*iter)->GetId(), 1, false);
    }

    std::chrono::steady_clock::time_point start;
    Simulator::Schedule(MilliSeconds(999),
                        [&start]() { start = std::chrono::steady_clock::now(); });

    // run simulation
    Simulator::Stop(Seconds(time));
    Simulator::Run();

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    std::cout << "Duration: " << duration << std::endl;

    // auto sourceApp = DynamicCast<OnOffApplication>(source.Get(0));
    // std::cout << "Node 0, 0 sent " << sourceApp->SetNode(Ptr<Node> node) << " bytes" <<
    // std::endl;
    for (auto end = sinks.End(), iter = sinks.Begin(); iter != end; ++iter)
    {
        auto node = (*iter)->GetNode();
        auto sink = (*iter)->GetObject<PacketSink>();
        std::cout << "Node " << Names::FindName(node) << " received " << sink->GetTotalRx()
                  << " bytes" << std::endl;
    }

    std::ofstream csv("lcrt-benchmark-times.csv", std::ios_base::app | std::ios_base::out);
    if (csv.tellp() == 0)
    {
        csv << "useLcrt, gridWidth, time (s), duration (µs)" << std::endl;
    }
    csv << useLcrt << ", " << gridWidth << ", " << time << ", " << duration << std::endl;

    Simulator::Destroy();

    Names::Clear();
    return 0;
}
