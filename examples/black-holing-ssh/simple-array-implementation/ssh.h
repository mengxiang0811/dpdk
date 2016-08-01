#ifndef __SSH_H
#define __SSH_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>

#define NULL_ITEM 0x7FFFFFFF

struct ssh_counter {
    uint32_t item;
    uint64_t count;
    uint64_t error;
};

struct ssh_ds {
    uint32_t size;
    struct ssh_counter *counters;
};

struct ssh_ds *ssh_init(float fPhi);
struct ssh_counter *find_item(struct ssh_ds *ssh, uint32_t item);
int ssh_update(struct ssh_ds *ssh, uint32_t item, uint32_t value);
int sink_down(struct ssh_ds *ssh, int idx);
void swap(void *a, void *b);
void ssh_show(struct ssh_ds *ssh);
void ssh_reset(struct ssh_ds *ssh);

#endif
