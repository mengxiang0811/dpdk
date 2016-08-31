#include "mail_box.h"

extern struct fb_mailbox mailboxes[GATEKEEPER_MAX_FUNC_BLKS];
#if 0
struct fb_mailbox mailboxes[GATEKEEPER_MAX_FUNC_BLKS] = {
	{"gatekeeper", { NULL, NULL }},
	{"grantor", { NULL, NULL }},
};
#endif

	void
fb_mailbox_create(void)
{
	int i = 0, j = 0;

	for (i = 0; i < GATEKEEPER_MAX_FUNC_BLKS; i++)
	{
		for (j = 0; j < GATEKEEPER_MAX_NUMA_NODES; j++)
		{
			char ring_name[256];                                                 
			snprintf(ring_name, sizeof(ring_name), "%s_%d", mailboxes[i].fb_name, j);

			printf("%s\n", ring_name);

			mailboxes[i].func_mailbox[j] = rte_ring_create(ring_name, MAILBOX_RING_SIZE, j, RING_F_SC_DEQ);

			if (mailboxes[i].func_mailbox[j] == NULL)
			{                                                                   
				fprintf(stderr, "failed to allocate %s mailbox %d\n", mailboxes[i].fb_name, j);
				return;                                                         
			}
		}
	}
}

	void
fb_mailbox_destroy(void)
{
	int i = 0, j = 0;

	for (i = 0; i < GATEKEEPER_MAX_FUNC_BLKS; i++)
		for (j = 0; j < GATEKEEPER_MAX_NUMA_NODES; j++)
			rte_ring_free(mailboxes[i].func_mailbox[j]);
}
