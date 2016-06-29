/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/queue.h>

#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ip.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

//#define DEBUG
#define SIMMOD
#define MULTIMOD

#define RX_RING_SIZE 128
#define TX_RING_SIZE 512

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32
#define MAX_LCORE  4


#define IP_VERSION 0x40                                                         
#define IP_HDRLEN  0x05 /* default IP header length == five 32-bits words. */   
#define IP_DEFTTL  64   /* from RFC 1340. */                                    
#define IP_VHL_DEF (IP_VERSION | IP_HDRLEN)                                     
#define IP_DN_FRAGMENT_FLAG 0x0040

static const struct rte_eth_conf port_conf_default = {
	.rxmode = { .max_rx_pkt_len = ETHER_MAX_LEN, },
};

static unsigned nb_ports;
struct rte_mempool *mbuf_pool;
static uint16_t dst_port[MAX_LCORE][BURST_SIZE];

/* the Lua interpreter */
lua_State* L;

/* simulation test */

static struct ipv4_hdr ip_hdr_template[1];
static struct ether_hdr l2_hdr_template[1];

	void
init_hdr_templates(void)
{
	memset(ip_hdr_template, 0, sizeof(ip_hdr_template));
	memset(l2_hdr_template, 0, sizeof(l2_hdr_template));

	ip_hdr_template[0].version_ihl = IP_VHL_DEF;
	ip_hdr_template[0].type_of_service = (2 << 2); // default DSCP 2 
	ip_hdr_template[0].total_length = 0; 
	ip_hdr_template[0].packet_id = 0;
	ip_hdr_template[0].fragment_offset = IP_DN_FRAGMENT_FLAG;
	ip_hdr_template[0].time_to_live = IP_DEFTTL;
	ip_hdr_template[0].next_proto_id = IPPROTO_IP;
	ip_hdr_template[0].hdr_checksum = 0;
	ip_hdr_template[0].src_addr = rte_cpu_to_be_32(0x00000000);
	ip_hdr_template[0].dst_addr = rte_cpu_to_be_32(0x07010101);

	l2_hdr_template[0].d_addr.addr_bytes[0] = 0x0a;
	l2_hdr_template[0].d_addr.addr_bytes[1] = 0x00;
	l2_hdr_template[0].d_addr.addr_bytes[2] = 0x27;
	l2_hdr_template[0].d_addr.addr_bytes[3] = 0x00;
	l2_hdr_template[0].d_addr.addr_bytes[4] = 0x00;
	l2_hdr_template[0].d_addr.addr_bytes[5] = 0x01;

	l2_hdr_template[0].s_addr.addr_bytes[0] = 0x08;
	l2_hdr_template[0].s_addr.addr_bytes[1] = 0x00;
	l2_hdr_template[0].s_addr.addr_bytes[2] = 0x27;
	l2_hdr_template[0].s_addr.addr_bytes[3] = 0x7d;
	l2_hdr_template[0].s_addr.addr_bytes[4] = 0xc7;
	l2_hdr_template[0].s_addr.addr_bytes[5] = 0x68;

	l2_hdr_template[0].ether_type = rte_cpu_to_be_16(ETHER_TYPE_IPv4);

	return;
}

/*
 * Initialises a given port using global settings and with the rx buffers
 * coming from the mbuf_pool passed as parameter
 */
	static inline int
port_init(uint8_t port, struct rte_mempool *mbuf_pool)
{
	struct rte_eth_conf port_conf = port_conf_default;
	const uint16_t rx_rings = 1, tx_rings = 1;
	int retval;
	uint16_t q;

	if (port >= rte_eth_dev_count())
		return -1;

	retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
	if (retval != 0)
		return retval;

	for (q = 0; q < rx_rings; q++) {
		retval = rte_eth_rx_queue_setup(port, q, RX_RING_SIZE,
				rte_eth_dev_socket_id(port), NULL, mbuf_pool);
		if (retval < 0)
			return retval;
	}

	for (q = 0; q < tx_rings; q++) {
		retval = rte_eth_tx_queue_setup(port, q, TX_RING_SIZE,
				rte_eth_dev_socket_id(port), NULL);
		if (retval < 0)
			return retval;
	}

	retval  = rte_eth_dev_start(port);
	if (retval < 0)
		return retval;

	struct ether_addr addr;

	rte_eth_macaddr_get(port, &addr);
	printf("Port %u MAC: %02"PRIx8" %02"PRIx8" %02"PRIx8
			" %02"PRIx8" %02"PRIx8" %02"PRIx8"\n",
			(unsigned)port,
			addr.addr_bytes[0], addr.addr_bytes[1],
			addr.addr_bytes[2], addr.addr_bytes[3],
			addr.addr_bytes[4], addr.addr_bytes[5]);

	rte_eth_promiscuous_enable(port);

	return 0;
}

	static int
lpm_main_loop(__attribute__((unused)) void *arg)
{
	unsigned int lcore_id = rte_lcore_id();

	lua_State *tl = lua_newthread(L);

	printf("\nCore %u forwarding packets with LPM. [Ctrl+C to quit]\n",
			rte_lcore_id());
	
	lua_getglobal(tl, "lpm_table_init"); /* function to be called */
	lua_pushinteger(tl, lcore_id);   /* push 1st argument: #packets */
	if (lua_pcall(tl, 1, 0, 0) != 0)
		error(tl, "error running function `lpm_table_init': %s", lua_tostring(tl, -1));

#ifndef SIMMOD
	uint8_t port;

	for (port = 0; port < nb_ports; port++)
		if (rte_eth_dev_socket_id(port) > 0 &&
				rte_eth_dev_socket_id(port) !=
				(int)rte_socket_id())
			printf("WARNING, port %u is on remote NUMA node to "
					"polling thread.\n\tPerformance will "
					"not be optimal.\n", port);

	for (;;) {
		for (port = 0; port < nb_ports; port++) {
			struct rte_mbuf *bufs[BURST_SIZE];
			const uint16_t nb_rx = rte_eth_rx_burst(port, 0,
					bufs, BURST_SIZE);
			uint16_t buf;

			if (unlikely(nb_rx == 0))
				continue;

			for (buf = 0; buf < nb_rx; buf++) {
				printf("*******************Recieve PKT*******************\n");
				struct rte_mbuf *mbuf = bufs[buf];
				unsigned int len = rte_pktmbuf_data_len(mbuf);
				rte_pktmbuf_dump(stdout, mbuf, len);

				printf("***************PKT LOOKUP**************\n");
				lua_checkstack(tl, 20);

				/* push functions and arguments */
				lua_getglobal(tl, "llpm_simple_lookup"); /* function to be called */
				lua_pushlightuserdata(tl, mbuf);   /* push 1st argument */
				lua_pushinteger(tl, lcore_id);   /* push 2nd argument */

				if (lua_pcall(tl, 2, 1, 0) != 0)
					error(tl, "error running function `llpm_simple_lookup': %s", lua_tostring(tl, -1));

				/* retrieve result */
				int nexthop = 0;
				nexthop = lua_tointeger(tl, -1);
				lua_pop(tl, 1);  /* pop returned value */

				printf("\t\t\t#######The next hop is %d\n", nexthop);

				/*release the packet*/
				rte_pktmbuf_free(mbuf);
			}
		}
	}
#else

	int loop = 10;

#ifdef MULTIMOD
	/* lookup a burst of 5 packets */
	int len = 0;
	struct rte_mbuf *pkts[BURST_SIZE];
#endif /* MULTIMOD */

	while (loop-- > 0) {
		struct rte_mbuf *mbuf = rte_pktmbuf_alloc(mbuf_pool);

#ifdef MULTIMOD
		pkts[len++] = mbuf;
#endif /* MULTIMOD */

#ifdef DEBUG
		printf("*******************Construct PKT*******************\n");
#endif /* DEBUG */
		mbuf->packet_type |= RTE_PTYPE_L3_IPV4;
		struct ether_hdr *pneth = (struct ether_hdr *)rte_pktmbuf_prepend(mbuf, sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr));
		//struct ether_hdr *pneth = rte_pktmbuf_mtod(&mbuf, struct ether_hdr *);
		struct ipv4_hdr *ip = (struct ipv4_hdr *) &pneth[1];

		pneth = rte_memcpy(pneth, &l2_hdr_template[0],
				sizeof(struct ether_hdr));

		ip = rte_memcpy(ip, &ip_hdr_template[0],
				sizeof(struct ipv4_hdr));

		unsigned int new_ip_addr = 0;

		new_ip_addr += loop * (1 << 24) + (1 << 16) + (1 << 8) + loop;

		ip->dst_addr = rte_cpu_to_be_32(new_ip_addr);

#ifdef MULTIMOD
		if (len % 5 != 0) continue;
#endif /* MULTIMOD */

#ifdef DEBUG
		unsigned int len = rte_pktmbuf_data_len(mbuf);
		rte_pktmbuf_dump(stdout, mbuf, len);
		printf("***************PKT LOOKUP**************\n");
#endif /* DEBUG */

		lua_checkstack(tl, 20);

#ifdef MULTIMOD
		printf("In the multi packets mode!!!\n");
		/* push functions and arguments */
		lua_getglobal(tl, "llpm_opt_lpm_lookup"); /* function to be called */
		lua_pushinteger(tl, len);   /* push 1st argument: #packets */
		lua_pushlightuserdata(tl, pkts);   /* push 2nd argument: packets */
		lua_pushlightuserdata(tl, dst_port[lcore_id]);   /* push 3rd argument: destination ports buffer for this lcore */
		lua_pushinteger(tl, lcore_id);   /* push 4th argument: lcore_id */

		if (lua_pcall(tl, 4, 0, 0) != 0)
			error(tl, "error running function `llpm_opt_lpm_lookup': %s", lua_tostring(tl, -1));

		int i = 0;

		for (i = 0; i < len; i++) {
			printf("LCORE %d#######The next hop is %d for packet %d\n", rte_lcore_id(), dst_port[lcore_id][i], i);
			rte_pktmbuf_free(pkts[i]);
		}

		len = 0;
#else
		/* push functions and arguments */
		lua_getglobal(tl, "llpm_simple_lookup"); /* function to be called */
		lua_pushlightuserdata(tl, mbuf);   /* push 1st argument */
		lua_pushinteger(tl, lcore_id);   /* push 2nd argument */

		if (lua_pcall(tl, 2, 1, 0) != 0)
			error(tl, "error running function `llpm_simple_lookup': %s", lua_tostring(tl, -1));

		/* retrieve result */
		int nexthop = 0;
		nexthop = lua_tointeger(tl, -1);
		lua_pop(tl, 1);  /* pop returned value */
		printf("LCORE %d#######The next hop is %d for %d.1.1.%d\n", rte_lcore_id(), nexthop, loop, loop);
		rte_pktmbuf_free(mbuf);
		sleep(rand()%3);
#endif /* MULTIMOD */
	}

#endif /* SIMMOD */

	return 0;
}

	int
main(int argc, char **argv)
{
	int ret;
	unsigned lcore_id;
	int i = 0;

	uint8_t portid;

	init_hdr_templates();
	/* init EAL */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_panic("Cannot init EAL\n");

#ifndef SIMMOD
	argc -= ret;
	argv += ret;

	nb_ports = rte_eth_dev_count();
	printf("There are %d ports!\n", nb_ports);
	if (nb_ports < 2 || (nb_ports & 1))
		rte_exit(EXIT_FAILURE, "Error: number of ports must be even\n");

	mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL",
			NUM_MBUFS * nb_ports, MBUF_CACHE_SIZE, 0,
			RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

	/* initialize all ports */
	for (portid = 0; portid < nb_ports; portid++)
		if (port_init(portid, mbuf_pool) != 0)
			rte_exit(EXIT_FAILURE, "Cannot init port %"PRIu8"\n",
					portid);

	if (rte_lcore_count() > 1)
		printf("\nWARNING: Too much enabled lcores - "
				"App uses only 1 lcore\n");
#else
	srand(time(NULL));
	mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL",
			NUM_MBUFS * 2, MBUF_CACHE_SIZE, 0,
			RTE_MBUF_DEFAULT_BUF_SIZE, 0);
	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

#endif /* SIMMOD */

	/* initialize Lua */
	L = luaL_newstate();
	//lua_open();

	/* load Lua base libraries */
	luaL_openlibs(L);

#if 1
	/* load the script */
	luaL_loadfile(L, "./first_policy.lua");
	//luaL_loadfile(L, "./test.lua");
	if (lua_pcall(L, 0, 0, 0) != 0)
		error(L, "error running function `first_policy.lua': %s", lua_tostring(L, -1));

	printf("After load the lua file!!!\n");
	/* setup the LPM table */
	lua_getglobal(L, "llpm_setup");
	//lua_getglobal(L, "test");
	if (lua_pcall(L, 0, 0, 0) != 0)
		error(L, "error running function `llpm_setup': %s", lua_tostring(L, -1));
#endif /* 1 */

#if 1
	/* call lpm_main_loop() on every slave lcore */
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		rte_eal_remote_launch(lpm_main_loop, NULL, lcore_id);
	}
#endif /* 1 */

	/* call it on master lcore too */
	lpm_main_loop(NULL);

	rte_eal_mp_wait_lcore();

	printf("All tasks are finished!\n");

	lua_close(L);

	printf("Close the Lua State: L!\n");

	return 0;
}
