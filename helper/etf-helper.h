/*
 * Copyright (c) 2026 Tom Boddaert
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Tom Boddaert <etf-ns3@tomboddaert.com>
 */

#ifndef ETF_PATHING_HELPER_H
#define ETF_PATHING_HELPER_H

#include "ns3/application-helper.h"

namespace ns3
{
/**
 * @ingroup etf
 * @brief Helper class that adds ETF pathing to nodes.
 */
class EtfHelper : public ApplicationHelper
{
    public:
        EtfHelper();
};

} // namespace ns3

#endif // ETF_PATHING_HELPER_H
