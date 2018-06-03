/*
 * vr_dp_offloads.h --
 *
 * Copyright 2015 Mellanox Technologies, Ltd
 */
#ifndef __VR_DP_OFFLOADS_H__
#define __VR_DP_OFFLOADS_H__

#include "vr_offloads.h"
#include "vr_btable.h"

struct vr_offload_flow {
    struct vr_nexthop *nh;
    struct vr_flow_entry *fe;
    unsigned int fe_index;
    unsigned int tunnel_tag;
    unsigned int tunnel_type;
    bool is_mpls_l2;
    struct vr_interface *pvif;
    unsigned int ip;
    void *flow_handle;
}__attribute__((aligned(64)));

struct vr_offload_tag {
	unsigned int tag;
	bool is_mpls;
	bool valid;
};

int vr_dp_offloads_flow_init(struct vrouter *router);
void vr_dp_offloads_flow_exit(struct vrouter *router, bool soft_reset);
struct vr_offload_flow *get_offload_flow(struct vr_packet *pkt);

#endif /* __VR_DP_OFFLOADS_H__ */
