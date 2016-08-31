#ifndef __MAIL_BOX_H
#define __MAIL_BOX_H

#include <rte_ring.h>

#define	FUNC_NAME_LEN		  (128)
#define MAILBOX_RING_SIZE	  (128)
#define GATEKEEPER_MAX_FUNC_BLKS  (2)
#define GATEKEEPER_MAX_NUMA_NODES (1)

struct fb_mailbox {
	char fb_name[FUNC_NAME_LEN];
	struct rte_ring *func_mailbox[GATEKEEPER_MAX_NUMA_NODES] __rte_cache_aligned;
} __rte_cache_aligned;

void
fb_mailbox_create(void);

void
fb_mailbox_destroy(void);

#endif
