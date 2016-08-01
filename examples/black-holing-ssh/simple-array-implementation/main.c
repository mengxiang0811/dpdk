/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
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

#include <stdint.h>
#include <unistd.h>
#include <inttypes.h>

#include <rte_eal.h>

#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_ip.h>

#include "ssh.h"
#include "cycles2sec.h"

/* this may depend on the parameter phi */
#define BH_POOL_SIZE  1024
#define BH_CACHE_SIZE 64

#define BH_RING_SIZE  1024

#define RX_RING_SIZE 128
#define TX_RING_SIZE 512

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

static const struct rte_eth_conf port_conf_default = {
	.rxmode = { .max_rx_pkt_len = ETHER_MAX_LEN, },
};

extern uint64_t cycles_per_sec;

/* the given percentage */
static float fPhi = 0.2;
static unsigned nb_ports;

/* report period is 30 seconds */
static uint64_t t_report_interval;
static uint64_t t_app_start;

static struct rte_ring *bh_counting_stat_ring;
static struct rte_mempool *bh_counting_stat_pool;

struct bh_counting_stat {
    uint32_t item;
    uint64_t count;
    uint64_t time;
    unsigned int lcore_id;
};

static inline int
bh_counting_init(void)
{
    bh_counting_stat_pool = rte_mempool_create("bh_counting_stat_pool",
            BH_POOL_SIZE, sizeof(struct bh_counting_stat), BH_CACHE_SIZE,
            0, NULL, NULL, NULL, NULL, rte_socket_id(), 0);

    if (bh_counting_stat_pool == NULL)
        rte_exit(EXIT_FAILURE, "Problem getting black holing stat pool\n");

    bh_counting_stat_ring = rte_ring_create("bh_counting_stat_ring",
            BH_RING_SIZE, rte_socket_id(), RING_F_SC_DEQ);

    if (bh_counting_stat_ring == NULL)
        rte_exit(EXIT_FAILURE, "Problem getting black holing stat ring\n");

    t_report_interval = (30UL * cycles_per_sec);

    return 0;
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
send_stat(unsigned int lcore_id, uint32_t item, uint64_t count)
{

	printf("In send_stat function: lcore_id = %u, item = %u, count = %" PRIu64 "\n", lcore_id, item, count);
	struct bh_counting_stat *stat = NULL;
	int ret = rte_mempool_mc_get(bh_counting_stat_pool, (void **)&stat);

	if (ret == -ENOENT) {
		printf("Not enough entries in the mempool\n");
		return -1;
	}

	stat->item = item;
	stat->count = count;
	stat->time = rte_rdtsc();
	stat->lcore_id = lcore_id;

	ret = rte_ring_mp_enqueue(bh_counting_stat_ring, stat);

	if (ret == -EDQUOT) {
		printf("RING ENQUEUE ERROR: Quota exceeded. The objects have been enqueued, but the high water mark is exceeded.\n");
		rte_mempool_mp_put(bh_counting_stat_pool, stat);
		return -1;
	} else if (ret == -ENOBUFS) {
		printf("RING ENQUEUE ERROR: Quota exceeded. Not enough room in the ring to enqueue.\n");
		rte_mempool_mp_put(bh_counting_stat_pool, stat);
		return -1;
	}

	return 0;
}

/*
 * thread that does the counting work on each lcore
 */
	static  int
lcore_counting(__attribute__((unused)) void *arg)
{
	uint8_t port;

	uint64_t tot_traffic = 0;
	uint64_t t_last_report = t_app_start;

	for (port = 0; port < nb_ports; port++)
		if (rte_eth_dev_socket_id(port) > 0 &&
				rte_eth_dev_socket_id(port) !=
				(int)rte_socket_id())
			printf("WARNING, port %u is on remote NUMA node to "
					"polling thread.\n\tPerformance will "
					"not be optimal.\n", port);

	printf("\nCore %u forwarding packets. [Ctrl+C to quit]\n",
			rte_lcore_id());

	struct ssh_ds *ssh = ssh_init(fPhi);

	for (;;) {
		for (port = 0; port < nb_ports; port++) {
			struct rte_mbuf *bufs[BURST_SIZE];
			const uint16_t nb_rx = rte_eth_rx_burst(port, 0,
					bufs, BURST_SIZE);
			uint16_t buf;

			if (unlikely(nb_rx == 0))
				continue;

			for (buf = 0; buf < nb_rx; buf++) {
				struct rte_mbuf *mbuf = bufs[buf];

				struct ether_hdr *eth_hdr;
				uint16_t ether_type;
				eth_hdr = rte_pktmbuf_mtod(mbuf, struct ether_hdr *);
				ether_type = eth_hdr->ether_type;

				/* use the data len field to calculate as the pkt length */
				if (ether_type == rte_cpu_to_be_16(ETHER_TYPE_IPv4)) {
				//if (RTE_ETH_IS_IPV4_HDR(mbuf->packet_type)) {
					struct ipv4_hdr *ipv4_hdr;
					ipv4_hdr = rte_pktmbuf_mtod_offset(mbuf, struct ipv4_hdr *,
							sizeof(struct ether_hdr));

					unsigned int len = rte_pktmbuf_data_len(mbuf);

					tot_traffic += len;
					
					printf("IPv4 PKT: ---> %u, length = %d\n", ipv4_hdr->dst_addr, len);

					ssh_update(ssh, ((struct ipv4_hdr *)ipv4_hdr)->dst_addr, len);

				} else printf("non-IPv4 packet!\n");
				//rte_pktmbuf_dump(stdout, mbuf, len);
				//rte_pktmbuf_free(mbuf);
			}

			const uint16_t nb_tx = rte_eth_tx_burst(1, 0, bufs, nb_rx);

			if (nb_tx < nb_rx) printf("%d packets have been dropped!\n", nb_rx - nb_tx);
		}

		/* output the stat results to the combine lcore */
		/* check to report the results */
		uint64_t t_now = rte_rdtsc();

		if (t_now - t_last_report >= t_report_interval) {

			ssh_show(ssh);

			/* first send out the total amount of traffic */
			send_stat(rte_lcore_id(), NULL_ITEM, tot_traffic);

			int i = 1;
			for (i = 1; i <= ssh->size; ++i)
			{
				if (ssh->counters[i].item != NULL_ITEM && \
						ssh->counters[i].count > 0) {
					send_stat(rte_lcore_id(), ssh->counters[i].item,\
							ssh->counters[i].count);
				}
			}
			
			ssh_reset(ssh);

			tot_traffic = 0;
			t_last_report += t_report_interval;
		}
	}

	if (ssh)
		ssh_destroy(ssh);

	return 0;
}

	static  int
lcore_combine_stat(__attribute__((unused)) void *arg)
{
	int com_tot_traffic = 0;
	uint64_t t_last_report = t_app_start;
	struct bh_counting_stat *stat;

	printf("\nCore %u combines the results. [Ctrl+C to quit]\n",
			rte_lcore_id());

	struct ssh_ds *ssh = ssh_init(fPhi / 8);

	while (1) {

		uint64_t t_now = rte_rdtsc();

		/*
		 * enter a new report period 
		 * delay 10 seconds to do the results collection
		 * make sure all the stat results have been received from other lcores
		 * */
		if (t_now - t_last_report >= (t_report_interval + t_report_interval / 3)) {
			ssh_show(ssh);

			printf("fPhi = %f, totoal traffic = %d bytes\n", fPhi, com_tot_traffic);

			int i = 1;
			for (i = 1; i <= ssh->size; ++i) {
				if (ssh->counters[i].count > fPhi * com_tot_traffic) {
					printf("Dest: %u has estimated %" PRIu64 " Bytes traffic!\n", ssh->counters[i].item, ssh->counters[i].count);
				}
			}

			com_tot_traffic = 0;
			t_last_report += t_report_interval;
			
			ssh_reset(ssh);
		}

		if (rte_ring_dequeue(bh_counting_stat_ring, (void **)&stat) < 0) {
			usleep(5);
			continue;
		}

		if (stat->item == NULL_ITEM) com_tot_traffic += stat->count;
		else ssh_update(ssh, stat->item, stat->count);

		rte_mempool_mp_put(bh_counting_stat_pool, stat);
	}

	if (ssh)
		ssh_destroy(ssh);

	return 0;
}

/* Main function, does initialisation and calls the per-lcore functions */
	int
main(int argc, char *argv[])
{
	struct rte_mempool *mbuf_pool;
	uint8_t portid;

	/* init EAL */
	int ret = rte_eal_init(argc, argv);

	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");
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

	cycles_to_sec_init();
	bh_counting_init();
	t_app_start = rte_rdtsc();

	rte_eal_remote_launch(lcore_counting, NULL, 1);

	/* call lcore_combine_stat on master core only */
	lcore_combine_stat(NULL);

	rte_eal_mp_wait_lcore();
	return 0;
}
