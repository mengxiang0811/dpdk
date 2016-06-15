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

#ifndef __GRANTOR_FIRST_POLICY_H__
#define __GRANTOR_FIRST_POLICY_H__

#include <rte_vect.h>

#define DO_RFC_1812_CHECKS

#define RTE_LOGTYPE_GRANTOR RTE_LOGTYPE_USER1

#define MAX_PKT_BURST     32
#define BURST_TX_DRAIN_US 100 /* TX drain every ~100us */

#define MAX_RX_QUEUE_PER_LCORE 16

/*
 * Try to avoid TX buffering if we have at least MAX_TX_BURST packets to send.
 */
#define	MAX_TX_BURST	  (MAX_PKT_BURST / 2)

#define NB_SOCKETS        8

/* Configure how many packets ahead to prefetch, when reading packets */
#define PREFETCH_OFFSET	  3

/* Used to mark destination port as 'invalid'. */
#define	BAD_PORT ((uint16_t)-1)

#define FWDSTEP	4

/* replace first 12B of the ethernet header. */
#define	MASK_ETH 0x3f

struct mbuf_table {
	uint16_t len;
	struct rte_mbuf *m_table[MAX_PKT_BURST];
};

#ifdef DO_RFC_1812_CHECKS
static inline int
is_valid_ipv4_pkt(struct ipv4_hdr *pkt, uint32_t link_len)
{
	/* From http://www.rfc-editor.org/rfc/rfc1812.txt section 5.2.2 */
	/*
	 * 1. The packet length reported by the Link Layer must be large
	 * enough to hold the minimum length legal IP datagram (20 bytes).
	 */
	if (link_len < sizeof(struct ipv4_hdr))
		return -1;

	/* 2. The IP checksum must be correct. */
	/* this is checked in H/W */

	/*
	 * 3. The IP version number must be 4. If the version number is not 4
	 * then the packet may be another version of IP, such as IPng or
	 * ST-II.
	 */
	if (((pkt->version_ihl) >> 4) != 4)
		return -3;
	/*
	 * 4. The IP header length field must be large enough to hold the
	 * minimum length legal IP datagram (20 bytes = 5 words).
	 */
	if ((pkt->version_ihl & 0xf) < 5)
		return -4;

	/*
	 * 5. The IP total length field must be large enough to hold the IP
	 * datagram header, whose length is specified in the IP header length
	 * field.
	 */
	if (rte_cpu_to_be_16(pkt->total_length) < sizeof(struct ipv4_hdr))
		return -5;

	return 0;
}
#endif /* DO_RFC_1812_CHECKS */

/* Function pointers for LPM functionality. */
void 
lpm_check_ports(void);

void
lpm_setup(const int mode, const int socketid);

/* populate a LPM route */
int
lpm_ipv4_route_add(uint32_t ip, uint8_t depth, uint8_t next_hop, 
        const int socketid);

int
lpm_ipv6_route_add(uint8_t *ip, uint8_t depth, uint8_t next_hop, 
        const int socketid);

int
lpm_ipv4_route_del(uint32_t ip, uint8_t depth, const int socketid);

int
lpm_ipv6_route_del(uint8_t *ip, uint8_t depth, const int socketid);

void
lpm_ipv4_route_del_all(const int socketid);

void
lpm_ipv6_route_del_all(const int socketid);

/* 
 * store the dst_port information in a global data structure? 
 * e.g., dst_port[lcore_id][32] for each lcore
 */
void
lpm_lookup(int nb_rx, struct rte_mbuf **pkts_burst, 
        uint8_t portid, uint16_t *dst_port, const int socketid);

uint8_t
lpm_lookup_single_packet_with_ipv4(unsigned int ip, int socketid);

int
lpm_lookup_single_packet(struct rte_mbuf *m, uint8_t portid, int socketid);

void *
lpm_get_ipv4_grantor_lookup_struct(const int socketid);

void *
lpm_get_ipv6_grantor_lookup_struct(const int socketid);

#endif  /* __GRANTOR_FIRST_POLICY_H__ */
