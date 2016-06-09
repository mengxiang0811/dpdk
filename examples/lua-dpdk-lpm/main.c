/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/queue.h>

#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_debug.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#define MAX_LCORE 4

/* the Lua interpreter */
lua_State* L;

	static int
lpm_main_loop(__attribute__((unused)) void *arg)
{
	unsigned lcore_id;
	lcore_id = rte_lcore_id();

	printf("lpm at lcore %d\n", lcore_id);
#if 1
	lua_State *tl = lua_newthread(L);
	lua_getglobal(tl, "llpm_lookup");

	if (lua_pcall(tl, 0, 0, 0) != 0)
		error(tl, "error running function `llpm_lookup': %s", lua_tostring(tl, -1));
#endif

#if 0
	while (true) {
		/* receive the packets */

		/* push the packet/ a set of packets/IP address on top of the thread stack */
		if (lua_pcall(tl, 0, 0, 0) != 0)
			error(tl, "error running function `f': %s", lua_tostring(tl, -1));

		/* obtain the next hop information */
	}
#endif

	return 0;
}

	int
main(int argc, char **argv)
{
	int ret;
	unsigned lcore_id;

	/* initialize Lua */
	L = luaL_newstate();
	//lua_open();

	/* load Lua base libraries */
	luaL_openlibs(L);

	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_panic("Cannot init EAL\n");

#if 1
	/* load the script */
	luaL_loadfile(L, "./lpm.lua");
	//luaL_loadfile(L, "./test.lua");
	if (lua_pcall(L, 0, 0, 0) != 0)
		error(L, "error running function `f': %s", lua_tostring(L, -1));

	printf("After load the lua file!!!\n");
	/* setup the LPM table */
	lua_getglobal(L, "llpm_setup");
	//lua_getglobal(L, "test");
	if (lua_pcall(L, 0, 0, 0) != 0)
		error(L, "error running function `test': %s", lua_tostring(L, -1));
#endif

#if 1
	/* call lpm_main_loop() on every slave lcore */
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		rte_eal_remote_launch(lpm_main_loop, NULL, lcore_id);
	}
#endif

	/* call it on master lcore too */
	//lpm_main_loop(NULL);

	rte_eal_mp_wait_lcore();

	printf("All tasks are finished!\n");

	lua_close(L);

	printf("Close the Lua State: L!\n");

	return 0;
}
