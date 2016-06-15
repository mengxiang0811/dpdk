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

#include "l3fwd.h"

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

struct rte_lpm *ipv4_grantor_lpm_lookup_struct[NB_SOCKETS];
struct rte_lpm6 *ipv6_grantor_lpm_lookup_struct[NB_SOCKETS];

#if defined(__SSE4_1__)
#include "l3fwd_lpm_sse.h"
#else
#include "l3fwd_lpm.h"
#endif

#if 0
/* main processing loop */
int
lpm_main_loop(__attribute__((unused)) void *dummy)
{
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	unsigned lcore_id;
	uint64_t prev_tsc, diff_tsc, cur_tsc;
	int i, nb_rx;
	uint8_t portid, queueid;
	struct lcore_conf *qconf;
	const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) /
		US_PER_S * BURST_TX_DRAIN_US;

	prev_tsc = 0;

	lcore_id = rte_lcore_id();
	qconf = &lcore_conf[lcore_id];

	if (qconf->n_rx_queue == 0) {
		RTE_LOG(INFO, L3FWD, "lcore %u has nothing to do\n", lcore_id);
		return 0;
	}

	RTE_LOG(INFO, L3FWD, "entering main loop on lcore %u\n", lcore_id);

	for (i = 0; i < qconf->n_rx_queue; i++) {

		portid = qconf->rx_queue_list[i].port_id;
		queueid = qconf->rx_queue_list[i].queue_id;
		RTE_LOG(INFO, L3FWD,
			" -- lcoreid=%u portid=%hhu rxqueueid=%hhu\n",
			lcore_id, portid, queueid);
	}

	while (!force_quit) {

		cur_tsc = rte_rdtsc();

		/*
		 * TX burst queue drain
		 */
		diff_tsc = cur_tsc - prev_tsc;
		if (unlikely(diff_tsc > drain_tsc)) {

			for (i = 0; i < qconf->n_tx_port; ++i) {
				portid = qconf->tx_port_id[i];
				if (qconf->tx_mbufs[portid].len == 0)
					continue;
				send_burst(qconf,
					qconf->tx_mbufs[portid].len,
					portid);
				qconf->tx_mbufs[portid].len = 0;
			}

			prev_tsc = cur_tsc;
		}

		/*
		 * Read packet from RX queues
		 */
		for (i = 0; i < qconf->n_rx_queue; ++i) {
			portid = qconf->rx_queue_list[i].port_id;
			queueid = qconf->rx_queue_list[i].queue_id;
			nb_rx = rte_eth_rx_burst(portid, queueid, pkts_burst,
				MAX_PKT_BURST);
			if (nb_rx == 0)
				continue;

#if defined(__SSE4_1__)
			l3fwd_lpm_send_packets(nb_rx, pkts_burst,
						portid, qconf);
#else
			l3fwd_lpm_no_opt_send_packets(nb_rx, pkts_burst,
							portid, qconf);
#endif /* __SSE_4_1__ */
		}
	}

	return 0;
}
#endif

/*
 * TODO: check the packet types that the port supports and callback functions register
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

void lpm_check_ports()
{
	unsigned nb_ports;
    uint8_t portid;

	nb_ports = rte_eth_dev_count();

    for (portid = 0; portid < nb_ports; portid++)
        lpm_check_ptype(portid)
}

    void
lpm_setup(const int mode, const int socketid)
{
    struct rte_lpm6_config config;
    struct rte_lpm_config config_ipv4;
    unsigned i;
    int ret;
    char s[64];

    /* TODO: the mode for ipv4/ipv6 on/off */

    /* create the LPM table */
    config_ipv4.max_rules = IPV4_GRANTOR_LPM_MAX_RULES;
    config_ipv4.number_tbl8s = IPV4_GRANTOR_LPM_NUMBER_TBL8S;
    config_ipv4.flags = 0;
    snprintf(s, sizeof(s), "IPV4_GRANTOR_LPM_%d", socketid);
    ipv4_grantor_lpm_lookup_struct[socketid] =
        rte_lpm_create(s, socketid, &config_ipv4);
    if (ipv4_grantor_lpm_lookup_struct[socketid] == NULL)
        rte_exit(EXIT_FAILURE,
                "Unable to create the Grantor LPM table on socket %d\n",
                socketid);

#ifdef SIMMOD

    /* populate the LPM table */
    for (i = 0; i < IPV4_GRANTOR_LPM_NUM_ROUTES; i++) {

        /* skip unused ports */
        if ((1 << ipv4_grantor_lpm_route_array[i].if_out &
                    enabled_port_mask) == 0)
            continue;

        ret = rte_lpm_add(ipv4_grantor_lpm_lookup_struct[socketid],
                ipv4_grantor_lpm_route_array[i].ip,
                ipv4_grantor_lpm_route_array[i].depth,
                ipv4_grantor_lpm_route_array[i].if_out);

        if (ret < 0) {
            rte_exit(EXIT_FAILURE,
                    "Unable to add entry %u to the Grantor LPM table on socket %d\n",
                    i, socketid);
        }

        printf("LPM: Adding route 0x%08x / %d (%d)\n",
                (unsigned)ipv4_grantor_lpm_route_array[i].ip,
                ipv4_grantor_lpm_route_array[i].depth,
                ipv4_grantor_lpm_route_array[i].if_out);
    }

#endif /* SIMMOD */

    /* create the LPM6 table */
    snprintf(s, sizeof(s), "IPV6_GRANTOR_LPM_%d", socketid);

    config.max_rules = IPV6_GRANTOR_LPM_MAX_RULES;
    config.number_tbl8s = IPV6_GRANTOR_LPM_NUMBER_TBL8S;
    config.flags = 0;
    ipv6_grantor_lpm_lookup_struct[socketid] = rte_lpm6_create(s, socketid,
            &config);
    if (ipv6_grantor_lpm_lookup_struct[socketid] == NULL)
        rte_exit(EXIT_FAILURE,
                "Unable to create the Grantor LPM table on socket %d\n",
                socketid);

#ifdef SIMMOD

    /* populate the LPM table */
    for (i = 0; i < IPV6_GRANTOR_LPM_NUM_ROUTES; i++) {

        /* skip unused ports */
        if ((1 << ipv6_grantor_lpm_route_array[i].if_out &
                    enabled_port_mask) == 0)
            continue;

        ret = rte_lpm6_add(ipv6_grantor_lpm_lookup_struct[socketid],
                ipv6_grantor_lpm_route_array[i].ip,
                ipv6_grantor_lpm_route_array[i].depth,
                ipv6_grantor_lpm_route_array[i].if_out);

        if (ret < 0) {
            rte_exit(EXIT_FAILURE,
                    "Unable to add entry %u to the Grantor LPM table on socket %d\n",
                    i, socketid);
        }

        printf("LPM: Adding route %s / %d (%d)\n",
                "IPV6",
                ipv6_grantor_lpm_route_array[i].depth,
                ipv6_grantor_lpm_route_array[i].if_out);
    }

#endif /* SIMMOD */

}

    int
lpm_ipv4_route_add(uint32_t ip, uint8_t depth, uint8_t next_hop, 
        const int socketid)
{
    int ret = -1;
    ret = rte_lpm_add(ipv4_grantor_lpm_lookup_struct[socketid],
            ip,
            depth,
            next_hop);

    if (ret < 0) {
        rte_exit(EXIT_FAILURE,
                "Unable to add an IPv4 entry to the Grantor LPM table on socket %d\n",
                socketid);
    }

    printf("LPM: Adding route 0x%08x / %d (%d)\n", (unsigned)ip, depth, next_hop);

    return 1;
}

int
lpm_ipv6_route_add(uint8_t *ip, uint8_t depth, uint8_t next_hop, 
        const int socketid)
{
    int ret = -1;
    ret = rte_lpm6_add(ipv6_grantor_lpm_lookup_struct[socketid],
            ip,
            depth,
            next_hop);

    if (ret < 0) {
        rte_exit(EXIT_FAILURE,
                "Unable to add an IPv6 entry to the Grantor LPM table on socket %d\n", 
                socketid);
    }

    printf("LPM: Adding route %s / %d (%d)\n", "IPv6", depth, next_hop);

    return 1;
}

    int
lpm_ipv4_route_del(uint32_t ip, uint8_t depth, const int socketid)
{
    int ret = -1;
    ret = rte_lpm_delete(ipv4_grantor_lpm_lookup_struct[socketid],
            ip,
            depth);

    if (ret < 0) {
        rte_exit(EXIT_FAILURE, 
                "Unable to delete an IPv4 entry in the Grantor LPM table on socket %d\n", 
                socketid);
    }

    printf("LPM: Deleting route 0x%08x / %d\n", (unsigned)ip, depth);

    return 1;
}

int
lpm_ipv6_route_del(uint8_t *ip, uint8_t depth, const int socketid)
{
    int ret = -1;
    ret = rte_lpm6_delete(ipv6_grantor_lpm_lookup_struct[socketid],
            ip,
            depth);

    if (ret < 0) {
        rte_exit(EXIT_FAILURE, 
                "Unable to delete an IPv6 entry in the Grantor LPM table on socket %d\n",
                socketid);
    }

    printf("LPM: Deleting route %s / %d\n", "IPv6", depth);

    return 1;
}

    int
lpm_ipv4_route_del_all(const int socketid)
{
    int ret = -1;
    ret = rte_lpm_delete_all(ipv4_grantor_lpm_lookup_struct[socketid]);

    if (ret < 0) {
        rte_exit(EXIT_FAILURE,
                "Unable to delete all IPv4 entries in the Grantor LPM table on socket %d\n",
                socketid);
    }

    printf("LPM: Deleting all IPv4 routes!\n");

    return 1;
}

int
lpm_ipv6_route_del_all(const int socketid)
{
    int ret = -1;
    ret = rte_lpm6_delete_all(ipv6_grantor_lpm_lookup_struct[socketid]);

    if (ret < 0) {
        rte_exit(EXIT_FAILURE,
                "Unable to delete all IPv6 entries in the Grantor LPM table on socket %d\n",
                socketid);
    }

    printf("LPM: Deleting all IPv6 routes!\n");

    return 1;
}

    int
lpm_lookup(int nb_rx, struct rte_mbuf **pkts_burst, 
        uint8_t portid, uint16_t *dst_port, const int socketid)
{
    int ret = (uint16_t)(-1);
#if defined(__SSE4_1__)
    ret = lpm_lookup_packets(nb_rx, pkts_burst,
            portid, dst_port, socketid);
#else
    ret = lpm_no_opt_lookup_packets(nb_rx, pkts_burst,
            portid, dst_port, socketid);
#endif /* __SSE_4_1__ */

    return ret;
}

/* Return ipv4/ipv6 lpm fwd lookup struct. */
    void *
lpm_get_ipv4_grantor_lookup_struct(const int socketid)
{
    return ipv4_grantor_lpm_lookup_struct[socketid];
}

    void *
lpm_get_ipv6_grantor_lookup_struct(const int socketid)
{
    return ipv6_grantor_lpm_lookup_struct[socketid];
}