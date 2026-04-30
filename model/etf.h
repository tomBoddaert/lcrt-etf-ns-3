/*
 * Copyright (c) 2026 Tom Boddaert
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Tom Boddaert <etf-ns3@tomboddaert.com>
 */

#ifndef ETF_H
#define ETF_H

/**
 * @defgroup etf ETF seamless pathing algorithm
 */

#include "ns3/application.h"
#include "ns3/lcrt.h"
#include "ns3/liblcrt.hpp"
#include "ns3/timer.h"
#include "ns3/vector.h"

namespace ns3
{

namespace etf
{

/**
 * @ingroup etf
 *
 * @brief ETF seamless pathing algorithm
 */
class EtfPathing : public Application
{
  public:
    static TypeId GetTypeId();

    EtfPathing();
    ~EtfPathing() override;
    void DoDispose() override;
    void StartApplication() override;

    bool BeginTransition(Vector target);

  private:
    double m_speed;
    EtfPath* m_path;
    Vector m_target;
    Timer m_timeout;
    Ipv4Address m_nextParent;

    void BeginNextSegment();
    void OnSegmentEnd(Ipv4Address nextParent);
    void OnParent(Ipv4Address parent);
};

} // namespace etf

} // namespace ns3

#endif // ETF_H
