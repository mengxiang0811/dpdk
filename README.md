DPDK is a set of libraries and drivers for fast packet processing.
It supports many processor architectures and both FreeBSD and Linux.

The DPDK uses the Open Source BSD license for the core libraries and
drivers. The kernel components are GPLv2 licensed.

Please check the doc directory for release notes,
API documentation, and sample application information.

For questions and usage discussions, subscribe to: users@dpdk.org
Report bugs and issues to the development mailing list: dev@dpdk.org

Notice that, *for calling DPDK libraries from LuaJIT-5.1, we need to compile DPDK as shared libraries, since LuaJIT FFI library can only support shared libraries*. This can be done by setting option ``CONFIG_RTE_BUILD_SHARED_LIB=y`` in the ``config/common_base`` file.
