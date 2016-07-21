#include "ssh.h"

struct ssh_ds *ssh_init(uint32_t size)
{
    struct ssh_ds *ssh;
    ssh = (struct ssh_ds *)calloc(1, sizeof(struct ssh_ds));

    if (!ssh) return ssh;

    ssh->size = (size | 1) + 1;

    ssh->counters = (struct ssh_counter *)calloc(ssh->size, sizeof(struct ssh_counter));

    if (!ssh->counters) {
        free(ssh);
        return NULL;
    }

    return ssh;
}

struct ssh_counter *find_item(struct ssh_ds *ssh, uint32_t item)
{
    int i = 0;

    for (i = 1; i < ssh->size; i++) {
        if (ssh->counters[i].item == item) {
            return &ssh->counters[i];
        }
    }

    return NULL;
}

void swap(void *a, void *b)
{
    struct ssh_counter tmp = *(struct ssh_counter *)a;
    *(struct ssh_counter *)a = *(struct ssh_counter *)b;
    *(struct ssh_counter *)b = tmp;
}

int sink_down(struct ssh_ds *ssh, int idx)
{
    /* wrong range */
    if (idx < 1 && idx >= ssh->size)
        return -1;

    int cur_idx = idx;

    while (1) {

        /* finished */
        if ((cur_idx << 1) >= ssh->size || cur_idx >= ssh->size)
            return 0;

        int left_idx = (cur_idx << 1);
        int right_idx = left_idx + 1;

        int smallest = ssh->counters[left_idx].count < ssh->counters[right_idx].count ? left_idx : right_idx;

        if (ssh->counters[cur_idx].count > ssh->counters[smallest].count) {
            printf("test: %d\n", cur_idx);
            ssh_show(ssh);
            swap(&ssh->counters[cur_idx], &ssh->counters[smallest]);
            ssh_show(ssh);
            cur_idx = smallest;
        } else return 0;
    }

    /* impossible */
    return -2;
}

int ssh_update(struct ssh_ds *ssh, uint32_t item, uint32_t value)
{
    struct ssh_counter *counter = find_item(ssh, item);

    if (counter) {
        counter->count += value;
        return sink_down(ssh, counter - ssh->counters);
    }

    ssh->counters[1].item = item;
    ssh->counters[1].count = value;

    //ssh_show(ssh);

    return sink_down(ssh, 1);
}

void ssh_show(struct ssh_ds *ssh)
{
    int i = 0;
    int j = 1;

    printf("\n**************\n");
    for (i = 1; i < ssh->size; i++) {
        printf("%llu", ssh->counters[i].count);

        if (i == j) {
            printf("\n");
            j = (j << 1) + 1;
        } else printf("\t");
    }

    printf("\n");
}
