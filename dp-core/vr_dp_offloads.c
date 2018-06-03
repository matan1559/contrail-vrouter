/*
 * vr_dp_offloads.c -- datapath flow offloads management
 *
 * Copyright 2018 Mellanox Technologies, Ltd
 */
#include <vrouter.h>
#include "vr_dp_offloads.h"
#include <vr_packet.h>

extern unsigned int vr_flow_entries;
extern unsigned int vr_oflow_entries;

struct vr_interface *pvif;
unsigned int host_ip;
struct vr_btable *offload_flows;
unsigned int datapath_offloads;

int
vr_dp_offloads_interface_add(struct vr_interface *vif)
{
    if (!vif)
        return -EINVAL;
    if (vif->vif_type == VIF_TYPE_PHYSICAL) {
        if (pvif && pvif != vif) {
            return -EEXIST;
        }
        pvif = vif;
    } else if (vif->vif_type == VIF_TYPE_HOST) {
        if (host_ip && vif->vif_ip != host_ip)
            return -EEXIST;
        host_ip = vif->vif_ip;
    }
    return 0;
}

int
vr_dp_offloads_interface_del(struct vr_interface *vif)
{
    if (!vif)
        return -EINVAL;
    if (vif->vif_type == VIF_TYPE_PHYSICAL)
        pvif = NULL;
    else if (vif->vif_type == VIF_TYPE_HOST)
        host_ip = 0;

    return 0;
}

int
vr_dp_offload_flow_del(struct vr_flow_entry * fe)
{
    struct vr_offload_flow *oflow;
    int ret = 0;

    if (!fe)
        return -EINVAL;

    oflow = (struct vr_offload_flow *)vr_btable_get(offload_flows,
                                                    fe->fe_hentry.hentry_index);
    if(!oflow)
        return -EINVAL;

    if(oflow->flow_handle)
        vr_offload_flow_destroy(oflow);

    memset(oflow, 0, sizeof(*oflow));

    return ret;
}

int
vr_dp_offloads_flow_set(struct vr_flow_entry *fe, unsigned int fe_index,
                        struct vr_flow_entry *rfe)
{
    struct vr_nexthop *nh;
    struct vr_offload_flow *oflow;
    int ret = 0;

    if (!fe || !pvif || !host_ip)
        return -EINVAL;

    oflow = (struct vr_offload_flow *)vr_btable_get(offload_flows, fe_index);
    if (!oflow)
        return -EINVAL;

    if (oflow->fe != fe || oflow->fe_index != fe_index || !oflow->tunnel_type)
        return -EINVAL;

    /* For flow change do destroy and create */
    if (oflow->flow_handle) {
        vr_offload_flow_destroy(oflow);
        oflow->flow_handle = NULL;
    }

    nh = oflow->nh;
    if (!(fe->fe_flags & VR_FLOW_FLAG_ACTIVE) ||
        fe->fe_action != VR_FLOW_ACTION_FORWARD ||
        !nh || !(nh->nh_flags & NH_FLAG_VALID) || nh->nh_type != NH_ENCAP ) {
        return 0;
    }

    oflow->pvif = pvif;
    oflow->ip = host_ip;

    return vr_offload_flow_create(oflow);
}

int
vr_dp_offload_flow_new_entry_set(unsigned int fe_index,
                                 struct vr_flow_entry *fe,
                                 struct vr_forwarding_md *fmd,
                                 struct vr_packet *pkt)
{
    struct vr_offload_flow *oflow;
    unsigned int tunnel_type = fmd->fmd_flags & (FMD_FLAG_TUNNEL_TYPE_GRE |
                                                 FMD_FLAG_TUNNEL_TYPE_MPLS_UDP |
                                                 FMD_FLAG_TUNNEL_TYPE_VXLAN);

    if (tunnel_type == 0)
        return 0;

    oflow = (struct vr_offload_flow *)vr_btable_get(offload_flows, fe_index);
    if (!oflow)
        return -EINVAL;

    oflow->nh = pkt->vp_nh;
    oflow->fe = fe;
    oflow->fe_index = fe_index;
    oflow->tunnel_tag = fmd->fmd_label;
    oflow->tunnel_type = tunnel_type;
    oflow->is_mpls_l2_tunnel = !!(fmd->fmd_flags & FMD_FLAG_L2_CONTROL_DATA);

    return 0;
}

struct vr_offload_ops vr_dp_offload_ops = {
    .voo_flow_set = vr_dp_offloads_flow_set,
    .voo_flow_del = vr_dp_offload_flow_del,
    .voo_flow_new_entry_set = vr_dp_offload_flow_new_entry_set,
    .voo_interface_add = vr_dp_offloads_interface_add,
    .voo_interface_del = vr_dp_offloads_interface_del,
};

int
vr_dp_offloads_flow_init(struct vrouter *router)
{
    if (!datapath_offloads)
        return 0;

    if (!vr_offload_flow_destroy || !vr_offload_flow_create ||
        !vr_offload_flow_index_get)
        return -ENOSYS;

    if (!offload_flows) {
        offload_flows = vr_btable_alloc(vr_flow_entries + vr_oflow_entries,
                                        sizeof(struct vr_offload_flow));
        if (!offload_flows)
            return -ENOMEM;
    }

    offload_ops = &vr_dp_offload_ops;

    return 0;

}

void
vr_dp_offloads_flow_exit(struct vrouter *router, bool soft_reset)
{
    struct vr_offload_flow *oflow;
    unsigned int i;
    unsigned int size;

    if (!datapath_offloads)
        return;

    if (!vr_offload_flow_destroy || !vr_offload_flow_create ||
        !vr_offload_flow_index_get)
        return;

    if (offload_flows) {
        size = vr_btable_size(offload_flows);
        for (i = 0; i < size; i++) {
            oflow = (struct vr_offload_flow *)vr_btable_get(offload_flows, i);
            if (!oflow)
                continue;
            if (oflow->flow_handle != NULL)
                vr_offload_flow_destroy(oflow);
            memset(oflow, 0, sizeof(*oflow));
        }

        if (!soft_reset) {
            vr_btable_free(offload_flows);
            offload_flows = NULL;
        }
    }
}

struct vr_offload_flow *
get_offload_flow(struct vr_packet *pkt)
{
    struct vr_offload_flow *oflow;
    unsigned int index;

    if (!datapath_offloads || !offload_flows)
         return NULL;

    if (vr_offload_flow_index_get(pkt, &index) != 0)
        return NULL;

    oflow = (struct vr_offload_flow *)vr_btable_get(offload_flows, index);
    if (!oflow || !oflow->flow_handle)
        return NULL;

    return  oflow;
}
