/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2016 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <string.h>
#include <sys/queue.h>
#include <stdarg.h>
#include <errno.h>
#include <getopt.h>
#include <stdbool.h>

#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_cycles.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_lpm.h>
#include <rte_lpm6.h>

#include "gfp.h"

//#define SIMMOD

#ifdef SIMMOD
struct ipv4_grantor_lpm_route {
	uint32_t ip;
	uint8_t  depth;
	uint8_t  if_out;
};

struct ipv6_grantor_lpm_route {
	uint8_t ip[16];
	uint8_t  depth;
	uint8_t  if_out;
};

static struct ipv4_grantor_lpm_route ipv4_grantor_lpm_route_array[] = {
	{IPv4(1, 1, 1, 0), 24, 0},
	{IPv4(2, 1, 1, 0), 24, 1},
	{IPv4(3, 1, 1, 0), 24, 2},
	{IPv4(4, 1, 1, 0), 24, 3},
	{IPv4(5, 1, 1, 0), 24, 4},
	{IPv4(6, 1, 1, 0), 24, 5},
	{IPv4(7, 1, 1, 0), 24, 6},
	{IPv4(8, 1, 1, 0), 24, 7},
};

static struct ipv6_grantor_lpm_route ipv6_grantor_lpm_route_array[] = {
	{{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, 48, 0},
	{{2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, 48, 1},
	{{3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, 48, 2},
	{{4, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, 48, 3},
	{{5, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, 48, 4},
	{{6, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, 48, 5},
	{{7, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, 48, 6},
	{{8, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, 48, 7},
};

#define IPV4_GRANTOR_LPM_NUM_ROUTES \
	(sizeof(ipv4_grantor_lpm_route_array) / sizeof(ipv4_grantor_lpm_route_array[0]))
#define IPV6_GRANTOR_LPM_NUM_ROUTES \
	(sizeof(ipv6_grantor_lpm_route_array) / sizeof(ipv6_grantor_lpm_route_array[0]))

#endif /* SIMMOD: for simulation purpose */

#define IPV4_GRANTOR_LPM_MAX_RULES         1024
#define IPV4_GRANTOR_LPM_NUMBER_TBL8S (1 << 8)
#define IPV6_GRANTOR_LPM_MAX_RULES         1024
#define IPV6_GRANTOR_LPM_NUMBER_TBL8S (1 << 16)

/* IPv6 is disabled by default. */
static int ipv6_on = 0;
/* NUMA is enabled by default. */
static int numa_on = 1;

static int active_lookup_struct_idx[MAX_NB_LCORES];
/* 
 * lookup data structure for IPv4 and IPv6
 *
 * each lcore needs two lookup data structures: one is for lookup,
 * another one is for update
 */
struct rte_lpm *ipv4_grantor_lpm_lookup_struct[MAX_NB_LCORES][2];
struct rte_lpm6 *ipv6_grantor_lpm_lookup_struct[MAX_NB_LCORES][2];

#if defined(__SSE4_1__)
#include "gfp_lpm_sse.h"
#else
#include "gfp_lpm.h"
#endif

/*
 * TODO: check the packet types that the port supports and 
 * callback functions register if necessary
 *
 * lpm_check_ptype
 */

static   int
lpm_check_ptype(int portid)
{
    int i, ret;
    int ptype_l3_ipv4 = 0, ptype_l3_ipv6 = 0;
    uint32_t ptype_mask = RTE_PTYPE_L3_MASK;

    ret = rte_eth_dev_get_supported_ptypes(portid, ptype_mask, NULL, 0);
    if (ret <= 0)
        return 0;

    uint32_t ptypes[ret];

    ret = rte_eth_dev_get_supported_ptypes(portid, ptype_mask, ptypes, ret);
    for (i = 0; i < ret; ++i) {
        if (ptypes[i] & RTE_PTYPE_L3_IPV4)
            ptype_l3_ipv4 = 1;
        if (ptypes[i] & RTE_PTYPE_L3_IPV6)
            ptype_l3_ipv6 = 1;
    }

    if (ptype_l3_ipv4 == 0)
        printf("port %d cannot parse RTE_PTYPE_L3_IPV4\n", portid);

    if (ptype_l3_ipv6 == 0)
        printf("port %d cannot parse RTE_PTYPE_L3_IPV6\n", portid);

    if (ptype_l3_ipv4 && ptype_l3_ipv6)
        return 1;

    return 0;

}

void
lpm_check_ports()
{
	unsigned nb_ports;
	uint8_t portid;

	nb_ports = rte_eth_dev_count();

	for (portid = 0; portid < nb_ports; portid++)
		lpm_check_ptype(portid);
}

void
lpm_init(const int is_ipv6_on, const int is_numa_on)
{
	if (is_ipv6_on > 0)
		ipv6_on = 1;

	if (is_numa_on > 0)
		numa_on = 1;

	memset(active_lookup_struct_idx, 0, sizeof(active_lookup_struct_idx));
}

	void
lpm_setup_lcore(const int lcoreid)
{
	struct rte_lpm6_config config;
	struct rte_lpm_config config_ipv4;
	unsigned i;
	int socketid;
	int ret;
	char s[64];

	if (numa_on == 1)
		socketid = rte_lcore_to_socket_id(lcoreid);
	else
		socketid = 0;

	/* create the LPM table */
	config_ipv4.max_rules = IPV4_GRANTOR_LPM_MAX_RULES;
	config_ipv4.number_tbl8s = IPV4_GRANTOR_LPM_NUMBER_TBL8S;
	config_ipv4.flags = 0;

	for (i = 0; i < 2; i++) {
		snprintf(s, sizeof(s), "IPV4_GRANTOR_LPM_%d_%d", lcoreid, i);
		ipv4_grantor_lpm_lookup_struct[lcoreid][i] =
			rte_lpm_create(s, socketid, &config_ipv4);
		if (ipv4_grantor_lpm_lookup_struct[lcoreid][i] == NULL)
			rte_exit(EXIT_FAILURE,
					"Unable to create the Grantor LPM table %d on socket %d\n",
					i, socketid);
	}

	/* create the LPM6 table */
	config.max_rules = IPV6_GRANTOR_LPM_MAX_RULES;
	config.number_tbl8s = IPV6_GRANTOR_LPM_NUMBER_TBL8S;
	config.flags = 0;
	
	for (i = 0; i < 2; i++) {
		snprintf(s, sizeof(s), "IPV6_GRANTOR_LPM_%d_%d", lcoreid, i);
		ipv6_grantor_lpm_lookup_struct[lcoreid][i] =
			rte_lpm6_create(s, socketid, &config);
		if (ipv6_grantor_lpm_lookup_struct[lcoreid][i] == NULL)
			rte_exit(EXIT_FAILURE,
					"Unable to create the Grantor LPM table %d on socket %d\n",
					i, socketid);
	}
}

	int
lpm_ipv4_route_add(uint32_t ip, uint8_t depth, uint8_t next_hop, 
		const int lcoreid)
{
	int ret = -1;

	struct rte_lpm *ipv4_l3fwd_lookup_struct = 
	lpm_get_ipv4_grantor_lookup_struct_for_update(lcoreid);

	ret = rte_lpm_add(ipv4_l3fwd_lookup_struct,
			ip,
			depth,
			next_hop);

	if (ret < 0) {
		rte_exit(EXIT_FAILURE,
				"Unable to add an IPv4 entry to the Grantor LPM table on lcore %d\n",
				lcoreid);
	}

	printf("LPM: Adding route 0x%08x / %d (%d) on lcore %d\n", (unsigned)ip, depth, next_hop, lcoreid);

	return 1;
}

	int
lpm_ipv6_route_add(uint8_t *ip, uint8_t depth, uint8_t next_hop, 
		const int lcoreid)
{
	int ret = -1;
	
	struct rte_lpm6 *ipv6_l3fwd_lookup_struct = 
	lpm_get_ipv6_grantor_lookup_struct_for_update(lcoreid);

	ret = rte_lpm6_add(ipv6_l3fwd_lookup_struct,
			ip,
			depth,
			next_hop);

	if (ret < 0) {
		rte_exit(EXIT_FAILURE,
				"Unable to add an IPv6 entry to the Grantor LPM table on lcore %d\n", 
				lcoreid);
	}

	printf("LPM: Adding route %s / %d (%d) on lcore %d\n", "IPv6", depth, next_hop, lcoreid);

	return 1;
}

	int
lpm_ipv4_route_del(uint32_t ip, uint8_t depth, const int lcoreid)
{
	int ret = -1;

	struct rte_lpm *ipv4_l3fwd_lookup_struct = 
	lpm_get_ipv4_grantor_lookup_struct_for_update(lcoreid);

	ret = rte_lpm_delete(ipv4_l3fwd_lookup_struct,
			ip,
			depth);

	if (ret < 0) {
		rte_exit(EXIT_FAILURE, 
				"Unable to delete an IPv4 entry in the Grantor LPM table on lcore %d\n", 
				lcoreid);
	}

	printf("LPM: Deleting route 0x%08x / %d on lcore %d\n", (unsigned)ip, depth, lcoreid);

	return 1;
}

	int
lpm_ipv6_route_del(uint8_t *ip, uint8_t depth, const int lcoreid)
{
	int ret = -1;
	
	struct rte_lpm6 *ipv6_l3fwd_lookup_struct = 
	lpm_get_ipv6_grantor_lookup_struct_for_update(lcoreid);

	ret = rte_lpm6_delete(ipv6_l3fwd_lookup_struct,
			ip,
			depth);

	if (ret < 0) {
		rte_exit(EXIT_FAILURE, 
				"Unable to delete an IPv6 entry in the Grantor LPM table on lcore %d\n",
				lcoreid);
	}

	printf("LPM: Deleting route %s / %d on lcore %d\n", "IPv6", depth, lcoreid);

	return 1;
}

	void
lpm_ipv4_route_del_all(const int lcoreid)
{
	rte_lpm_delete_all(ipv4_grantor_lpm_lookup_struct[lcoreid][0]);
	rte_lpm_delete_all(ipv4_grantor_lpm_lookup_struct[lcoreid][1]);

	printf("LPM: Deleting all IPv4 routes on lcore %d!\n", lcoreid);
}

	void
lpm_ipv6_route_del_all(const int lcoreid)
{
	rte_lpm6_delete_all(ipv6_grantor_lpm_lookup_struct[lcoreid][0]);
	rte_lpm6_delete_all(ipv6_grantor_lpm_lookup_struct[lcoreid][1]);

	printf("LPM: Deleting all IPv6 routes on lcore %d!\n", lcoreid);
}

	void   
lpm_lookup(int nb_rx, struct rte_mbuf **pkts_burst, 
		uint8_t portid, uint16_t *dst_port, const int lcoreid)
{
#if defined(__SSE4_1__)
	lpm_opt_lookup_packets(nb_rx, pkts_burst,
			portid, dst_port, lcoreid);
#else
	lpm_no_opt_lookup_packets(nb_rx, pkts_burst,
			portid, dst_port, lcoreid);
#endif /* __SSE_4_1__ */
}

uint8_t lpm_lookup_single_packet_with_ipv4(unsigned int ip, int lcoreid) {
	/* invalid port number */
	uint32_t next_hop;

	struct rte_lpm *ipv4_l3fwd_lookup_struct = 
	lpm_get_ipv4_grantor_lookup_struct_for_lookup(lcoreid);

	return (uint8_t) ((rte_lpm_lookup(ipv4_l3fwd_lookup_struct,
					ip,
					&next_hop) == 0) ? next_hop : 255);
}

uint8_t lpm_lookup_single_packet_with_ipv6(uint8_t ip6_addr[16], int lcoreid) {
	/* invalid port number */
	uint8_t next_hop;

	struct rte_lpm6 *ipv6_l3fwd_lookup_struct = 
	lpm_get_ipv6_grantor_lookup_struct_for_lookup(lcoreid);

	return (uint8_t) ((rte_lpm6_lookup(ipv6_l3fwd_lookup_struct,
					ip6_addr,
					&next_hop) == 0) ?  next_hop : 255);
}

int lpm_lookup_single_packet(struct rte_mbuf *m, uint8_t portid, int lcoreid) {
	struct ether_hdr *eth_hdr;
	struct ipv4_hdr *ipv4_hdr;

	eth_hdr = rte_pktmbuf_mtod(m, struct ether_hdr *);

	if (RTE_ETH_IS_IPV4_HDR(m->packet_type)) {
		/* Handle IPv4 headers.*/
		ipv4_hdr = rte_pktmbuf_mtod_offset(m, struct ipv4_hdr *,
				sizeof(struct ether_hdr));

		return lpm_lookup_single_packet_with_ipv4(rte_be_to_cpu_32(((struct ipv4_hdr *)ipv4_hdr)->dst_addr), lcoreid);
	} else if (RTE_ETH_IS_IPV6_HDR(m->packet_type)) {
		/* Handle IPv6 headers.*/
		struct ipv6_hdr *ipv6_hdr;

		ipv6_hdr = rte_pktmbuf_mtod_offset(m, struct ipv6_hdr *,
				sizeof(struct ether_hdr));
		return lpm_lookup_single_packet_with_ipv6(((struct ipv6_hdr *)ipv6_hdr)->dst_addr, lcoreid);
	}

	return portid;
}

/* this should be done before or after any lookup operations */
void
lpm_change_grantor_active_lookup_structure(const int lcoreid)
{
	active_lookup_struct_idx[lcoreid] = 1 - active_lookup_struct_idx[lcoreid];
}

/* Return ipv4/ipv6 lpm fwd lookup struct. */
	void *
lpm_get_ipv4_grantor_lookup_struct_for_lookup(const int lcoreid)
{
	int active = active_lookup_struct_idx[lcoreid];
	return ipv4_grantor_lpm_lookup_struct[lcoreid][active];
}

	void *
lpm_get_ipv4_grantor_lookup_struct_for_update(const int lcoreid)
{
	int active = active_lookup_struct_idx[lcoreid];
	return ipv4_grantor_lpm_lookup_struct[lcoreid][1 - active];
}

	void *
lpm_get_ipv6_grantor_lookup_struct_for_lookup(const int lcoreid)
{
	int active = active_lookup_struct_idx[lcoreid];
	return ipv6_grantor_lpm_lookup_struct[lcoreid][active];
}
	
void *
lpm_get_ipv6_grantor_lookup_struct_for_update(const int lcoreid)
{
	int active = active_lookup_struct_idx[lcoreid];
	return ipv6_grantor_lpm_lookup_struct[lcoreid][1 - active];
}
