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

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#define RX_RING_SIZE 128
#define TX_RING_SIZE 512

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

static const struct rte_eth_conf port_conf_default = {
	.rxmode = { .max_rx_pkt_len = ETHER_MAX_LEN, },
};

static unsigned nb_ports;

/* the Lua interpreter */
lua_State* L;

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
	uint8_t port;

	for (port = 0; port < nb_ports; port++)
		if (rte_eth_dev_socket_id(port) > 0 &&
				rte_eth_dev_socket_id(port) !=
				(int)rte_socket_id())
			printf("WARNING, port %u is on remote NUMA node to "
					"polling thread.\n\tPerformance will "
					"not be optimal.\n", port);

	printf("\nCore %u forwarding packets with LPM. [Ctrl+C to quit]\n",
			rte_lcore_id());

	lua_State *tl = lua_newthread(L);
	
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

				printf("\t\t\t***************PKT LOOKUP**************\n");
                lua_checkstack(tl, 20);

                /* push functions and arguments */
                lua_getglobal(tl, "llpm_get_dst_port"); /* function to be called */
                lua_pushlightuserdata(tl, mbuf);   /* push 1st argument */
                //lua_pushinteger(tl, 0);   /* push 2nd argument */

                if (lua_pcall(tl, 1, 1, 0) != 0)
                    error(tl, "error running function `llpm_get_dst_port': %s", lua_tostring(tl, -1));

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

    return 0;
}

    int
main(int argc, char **argv)
{
    int ret;
    unsigned lcore_id;
    int i = 0;

	struct rte_mempool *mbuf_pool;
	uint8_t portid;

    /* initialize Lua */
    L = luaL_newstate();
    //lua_open();

    /* load Lua base libraries */
    luaL_openlibs(L);

	/* init EAL */
    ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_panic("Cannot init EAL\n");

	argc -= ret;
	argv += ret;

	nb_ports = rte_eth_dev_count();
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

#if 1
    /* load the script */
    luaL_loadfile(L, "./lpm.lua");
    //luaL_loadfile(L, "./test.lua");
    if (lua_pcall(L, 0, 0, 0) != 0)
        error(L, "error running function `lpm.lua': %s", lua_tostring(L, -1));

    printf("After load the lua file!!!\n");
    /* setup the LPM table */
    lua_getglobal(L, "llpm_setup");
    //lua_getglobal(L, "test");
    if (lua_pcall(L, 0, 0, 0) != 0)
        error(L, "error running function `llpm_setup': %s", lua_tostring(L, -1));
#endif

#if 0
    /* call lpm_main_loop() on every slave lcore */
    RTE_LCORE_FOREACH_SLAVE(lcore_id) {
        rte_eal_remote_launch(lpm_main_loop, NULL, lcore_id);
    }
#endif

    /* call it on master lcore too */
    lpm_main_loop(NULL);

    rte_eal_mp_wait_lcore();

    printf("All tasks are finished!\n");

    lua_close(L);

    printf("Close the Lua State: L!\n");

    return 0;
}
