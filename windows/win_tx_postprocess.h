/*
 * win_tx_postprocess.h -- common defines and declarations used in Windows-specific code
 *
 * Copyright (c) 2018 Juniper Networks, Inc. All rights reserved.
 */
#ifndef __WIN_TX_POSTPROCESS_H__
#define __WIN_TX_POSTPROCESS_H__

#include <basetsd.h>
#include <ndis.h>
#include <vr_packet.h>

#include "win_packet_raw.h"

// TODO: Think about returning PWIN_PACKET instead of PWIN_PACKET_RAW.
PWIN_PACKET_RAW WinTxPostprocess(struct vr_packet *VrPacket);

#endif
