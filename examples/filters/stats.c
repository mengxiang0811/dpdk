#include "stats.h"

static uint8_t rx_queue_stats_mapping_enabled = 1;
static uint8_t tx_queue_stats_mapping_enabled = 0;

int stats_mapping_setup(portid_t port_id)
{
	int ret = -1;
	
	ret = rte_eth_dev_set_rx_queue_stats_mapping(port_id, 0, 1);
	ret = rte_eth_dev_set_rx_queue_stats_mapping(port_id, 127, 15);

	return ret;
}

void
nic_stats_display(portid_t port_id)
{
	static uint64_t prev_pkts_rx[RTE_MAX_ETHPORTS];
	static uint64_t prev_pkts_tx[RTE_MAX_ETHPORTS];
	static uint64_t prev_cycles[RTE_MAX_ETHPORTS];
	uint64_t diff_pkts_rx, diff_pkts_tx, diff_cycles;
	uint64_t mpps_rx, mpps_tx;
	struct rte_eth_stats stats;
	uint8_t i;

	static const char *nic_stats_border = "########################";

	rte_eth_stats_get(port_id, &stats);
	printf("\n  %s NIC statistics for port %-2d %s\n",
	       nic_stats_border, port_id, nic_stats_border);

	if ((!rx_queue_stats_mapping_enabled) && (!tx_queue_stats_mapping_enabled)) {
		printf("  RX-packets: %-10"PRIu64" RX-missed: %-10"PRIu64" RX-bytes:  "
		       "%-"PRIu64"\n",
		       stats.ipackets, stats.imissed, stats.ibytes);
		printf("  RX-errors: %-"PRIu64"\n", stats.ierrors);
		printf("  RX-nombuf:  %-10"PRIu64"\n",
		       stats.rx_nombuf);
		printf("  TX-packets: %-10"PRIu64" TX-errors: %-10"PRIu64" TX-bytes:  "
		       "%-"PRIu64"\n",
		       stats.opackets, stats.oerrors, stats.obytes);
	}
	else {
		printf("  RX-packets:              %10"PRIu64"    RX-errors: %10"PRIu64
		       "    RX-bytes: %10"PRIu64"\n",
		       stats.ipackets, stats.ierrors, stats.ibytes);
		printf("  RX-errors:  %10"PRIu64"\n", stats.ierrors);
		printf("  RX-nombuf:               %10"PRIu64"\n",
		       stats.rx_nombuf);
		printf("  TX-packets:              %10"PRIu64"    TX-errors: %10"PRIu64
		       "    TX-bytes: %10"PRIu64"\n",
		       stats.opackets, stats.oerrors, stats.obytes);
	}

	if (rx_queue_stats_mapping_enabled) {
		printf("\n");
		for (i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS; i++) {
			printf("  Stats reg %2d RX-packets: %10"PRIu64
			       "    RX-errors: %10"PRIu64
			       "    RX-bytes: %10"PRIu64"\n",
			       i, stats.q_ipackets[i], stats.q_errors[i], stats.q_ibytes[i]);
		}
	}
	if (tx_queue_stats_mapping_enabled) {
		printf("\n");
		for (i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS; i++) {
			printf("  Stats reg %2d TX-packets: %10"PRIu64
			       "                             TX-bytes: %10"PRIu64"\n",
			       i, stats.q_opackets[i], stats.q_obytes[i]);
		}
	}

	diff_cycles = prev_cycles[port_id];
	prev_cycles[port_id] = rte_rdtsc();
	if (diff_cycles > 0)
		diff_cycles = prev_cycles[port_id] - diff_cycles;

	diff_pkts_rx = stats.ipackets - prev_pkts_rx[port_id];
	diff_pkts_tx = stats.opackets - prev_pkts_tx[port_id];
	prev_pkts_rx[port_id] = stats.ipackets;
	prev_pkts_tx[port_id] = stats.opackets;
	mpps_rx = diff_cycles > 0 ?
		diff_pkts_rx * rte_get_tsc_hz() / diff_cycles : 0;
	mpps_tx = diff_cycles > 0 ?
		diff_pkts_tx * rte_get_tsc_hz() / diff_cycles : 0;
	printf("\n  Throughput (since last show)\n");
	printf("  Rx-pps: %12"PRIu64"\n  Tx-pps: %12"PRIu64"\n",
			mpps_rx, mpps_tx);

	printf("  %s############################%s\n",
	       nic_stats_border, nic_stats_border);
}

void
nic_stats_clear(portid_t port_id)
{
	rte_eth_stats_reset(port_id);
	printf("\n  NIC statistics for port %d cleared\n", port_id);
}

void
nic_xstats_display(portid_t port_id)
{
	struct rte_eth_xstat *xstats;
	int cnt_xstats, idx_xstat, idx_name;
	struct rte_eth_xstat_name *xstats_names;

	printf("###### NIC extended statistics for port %-2d\n", port_id);
	if (!rte_eth_dev_is_valid_port(port_id)) {
		printf("Error: Invalid port number %i\n", port_id);
		return;
	}

	/* Get count */
	cnt_xstats = rte_eth_xstats_get_names(port_id, NULL, 0);
	if (cnt_xstats  < 0) {
		printf("Error: Cannot get count of xstats\n");
		return;
	}

	/* Get id-name lookup table */
	xstats_names = malloc(sizeof(struct rte_eth_xstat_name) * cnt_xstats);
	if (xstats_names == NULL) {
		printf("Cannot allocate memory for xstats lookup\n");
		return;
	}
	if (cnt_xstats != rte_eth_xstats_get_names(
			port_id, xstats_names, cnt_xstats)) {
		printf("Error: Cannot get xstats lookup\n");
		free(xstats_names);
		return;
	}

	/* Get stats themselves */
	xstats = malloc(sizeof(struct rte_eth_xstat) * cnt_xstats);
	if (xstats == NULL) {
		printf("Cannot allocate memory for xstats\n");
		free(xstats_names);
		return;
	}
	if (cnt_xstats != rte_eth_xstats_get(port_id, xstats, cnt_xstats)) {
		printf("Error: Unable to get xstats\n");
		free(xstats_names);
		free(xstats);
		return;
	}

	/* Display xstats */
	for (idx_xstat = 0; idx_xstat < cnt_xstats; idx_xstat++)
		for (idx_name = 0; idx_name < cnt_xstats; idx_name++)
			if (xstats_names[idx_name].id == xstats[idx_xstat].id) {
				printf("%s: %"PRIu64"\n",
					xstats_names[idx_name].name,
					xstats[idx_xstat].value);
				break;
			}
	free(xstats_names);
	free(xstats);
}

void
nic_xstats_clear(portid_t port_id)
{
	rte_eth_xstats_reset(port_id);
}
