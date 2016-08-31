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
#include <inttypes.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>

#include "mail_box.h"

#define RX_RING_SIZE 128
#define TX_RING_SIZE 512

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

static const struct rte_eth_conf port_conf_default = {
	.rxmode = { .max_rx_pkt_len = ETHER_MAX_LEN }
};

enum FUNC_BLK_ID {
	GK,
	GT,
	FB_MAX,
};

struct fb_mailbox mailboxes[GATEKEEPER_MAX_FUNC_BLKS] = {
	{"gatekeeper", { NULL, NULL }},
	{"grantor", { NULL, NULL }},
};

enum OPS {
	ADD,
	SUB,
	UND
};

struct func_cmd {
	int cmd_id;
	int number;
};

static struct func_cmd gk_cmd[5] = {
	{ADD, 1},
	{ADD, 2},
	{ADD, 3},
	{ADD, 4},
	{SUB, 5},
};

static void send_cmd(void)
{
	int i = 0;

	for (i = 0; i < 5; i++) 
		if (rte_ring_mp_enqueue(mailboxes[GT].func_mailbox[0], &gk_cmd[i]) == -ENOBUFS) {
			printf("Not enough room in the ring to enqueue, no object is enqueued\n");
			return;
		}
}

static void recv_cmd(void)
{
	int i = 0;
	int result = 0;
	int cmd_cnt = 0;

	struct func_cmd *gt_cmd[10];

	while (cmd_cnt < 10) {
		cmd_cnt += (size_t)rte_ring_sc_dequeue_burst(mailboxes[GT].func_mailbox[0], (void **)(gt_cmd + cmd_cnt), (unsigned int)(10 - cmd_cnt));
	}
	
	for (i = 0; i < 10; i++)
	{
		printf("cmd_id = %d, number = %d\n", gt_cmd[i]->cmd_id, gt_cmd[i]->number);
		if (gt_cmd[i]->cmd_id == ADD) result += gt_cmd[i]->number;
		else result -= gt_cmd[i]->number;
	}

	printf("The final result is: %d\n", result);
}

	static int
func(__attribute__((unused)) void *arg)
{
	unsigned int lcore_id = rte_lcore_id();

	printf("At lcore %u\n", lcore_id);

	if (lcore_id != 0) send_cmd();
	else recv_cmd();

	return 0;
}

/*
 * The main function, which does initialization and calls the per-lcore
 * functions.
 */
	int
main(int argc, char *argv[])
{
	/* Initialize the Environment Abstraction Layer (EAL). */
	int ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");
	fb_mailbox_create();

	int i = 0;

	for (i = 1; i < 3; i++)
	{
		rte_eal_remote_launch(func, NULL, i);
	}

	func(NULL);

	rte_eal_mp_wait_lcore();

	fb_mailbox_destroy();
	return 0;
}
