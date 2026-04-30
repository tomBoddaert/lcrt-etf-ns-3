/*
 * Copyright (c) 2026 Tom Boddaert
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Tom Boddaert <lcrt-ns3@tomboddaert.com>
 *
 * This file is based on aodv-helper.h by Pavel Boyko <boyko@iitp.ru>
 */

#ifndef LCRT_HELPER_H
#define LCRT_HELPER_H

#include "ns3/attribute-helper.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/liblcrt.hpp"
#include "ns3/node-container.h"
#include "ns3/node.h"
#include "ns3/object-factory.h"

namespace ns3
{
/**
 * @ingroup lcrt
 * @brief Helper class that adds LCRT routing to nodes.
 */
class LcrtHelper : public Ipv4RoutingHelper
{
  public:
    LcrtHelper();

    /**
     * @returns pointer to clone of this LcrtHelper
     *
     * @internal
     * This method is mainly for internal use by the other helpers;
     * clients are expected to free the dynamic memory allocated by this method
     */
    LcrtHelper* Copy() const override;

    /**
     * @param node the node on which the routing protocol will run
     * @returns a newly-created routing protocol
     *
     * This method will be called by ns3::InternetStackHelper::Install
     */
    Ptr<Ipv4RoutingProtocol> Create(Ptr<Node> node) const override;

    /**
     * @param name the name of the attribute to set
     * @param value the value of the attribute to set.
     *
     * This method controls the attributes of ns3::lcrt::RoutingProtocol
     */
    void Set(std::string name, const AttributeValue& value);

  private:
    /** the factory to create LCRT routing object */
    ObjectFactory m_agentFactory;
};

} // namespace ns3

#endif // LCRT_HELPER_H
