cmd_lpm = gcc -m64 -pthread -fPIC  -march=native -DRTE_MACHINE_CPUFLAG_SSE -DRTE_MACHINE_CPUFLAG_SSE2 -DRTE_MACHINE_CPUFLAG_SSE3 -DRTE_MACHINE_CPUFLAG_SSSE3 -DRTE_MACHINE_CPUFLAG_SSE4_1 -DRTE_MACHINE_CPUFLAG_SSE4_2 -DRTE_MACHINE_CPUFLAG_AES -DRTE_MACHINE_CPUFLAG_PCLMULQDQ -DRTE_MACHINE_CPUFLAG_AVX -DRTE_MACHINE_CPUFLAG_RDRAND  -I/home/vagrant/ddos/dpdk/examples/lua-first-policy/build/include -I/home/vagrant/ddos/dpdk/x86_64-native-linuxapp-gcc/include -include /home/vagrant/ddos/dpdk/x86_64-native-linuxapp-gcc/include/rte_config.h -O3 -I/home/vagrant/ddos/lua-5.1.5/include  -Wl,-Map=lpm.map,--cref -o lpm main.o -Wl,--no-as-needed -Wl,-export-dynamic -L/home/vagrant/ddos/dpdk/examples/lua-first-policy/build/lib -L/home/vagrant/ddos/dpdk/x86_64-native-linuxapp-gcc/lib -Wl,-rpath=/home/vagrant/ddos/dpdk/x86_64-native-linuxapp-gcc/lib  -Wl,-lm -Wl,-lluajit-5.1 -L/home/vagrant/ddos/dpdk/x86_64-native-linuxapp-gcc/lib -Wl,--whole-archive -Wl,-lrte_distributor -Wl,-lrte_reorder -Wl,-lrte_kni -Wl,-lrte_pipeline -Wl,-lrte_table -Wl,-lrte_port -Wl,-lrte_timer -Wl,-lrte_hash -Wl,-lrte_jobstats -Wl,-lrte_lpm -Wl,-lrte_power -Wl,-lrte_acl -Wl,-lrte_meter -Wl,-lrte_sched -Wl,-lrte_vhost -Wl,--start-group -Wl,-lrte_kvargs -Wl,-lrte_mbuf -Wl,-lrte_ip_frag -Wl,-lethdev -Wl,-lrte_cryptodev -Wl,-lrte_mempool -Wl,-lrte_ring -Wl,-lrte_eal -Wl,-lrte_cmdline -Wl,-lrte_cfgfile -Wl,-lrte_pmd_bond -Wl,-lgcc_s -Wl,-ldl -Wl,--end-group -Wl,--no-whole-archive 
