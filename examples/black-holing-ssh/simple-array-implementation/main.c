#include "ssh.h"

int main()
{
    struct ssh_ds *ssh = ssh_init(12);

    ssh_update(ssh, 1, 100);
    ssh_update(ssh, 2, 200);
    ssh_update(ssh, 3, 300);

    ssh_show(ssh);
    return 0;
}
