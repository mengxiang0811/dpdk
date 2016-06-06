#Lua DPDK Helloworld Example

##Compile Shared Library
To compile the shared library ``libhelloworld.so``, go to the ``libhelloworld`` directory, and ``make``. Then, you can check the library in ``build`` directory by using the ``ldd libhelloworld.so`` command.
Since we need to run applications with ``root`` privilige, we need to make sure that the command ``sudo ldd libhelloworld.so`` can also correctly find the dependent libraries. In case, the library ``rte_eal`` cannot be found, you can add the path of DPDK shared libraries to the ``/etc/ld.so.conf`` file.
Notice that, the compilation needs to know the path of the ***DPDK shared libraries***, you may need to change the path in ``CFLAGS``.

##Compile helloworld Application
To compile the application, go to the ``app-helloworld`` directory, and ``make``. Then, you can check the application in the ``build`` directory. You can copy the ``libhelloworld.so`` and the ``test.lua`` script to the current ``build`` directory.
And run ``sudo ./helloworld`` to display the results.
