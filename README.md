### Introduction

This repository contains the instructions to build and install the MT Connect adapter using CMake to simplify the building process.

To install on a x64 system, add the libc6-dev-i386 library and multilib for g++.

	apt-get install libc6-dev-i386
	sudo apt-get install g++-multilib

### Build process

First we need to download the source of the adapter.

	git clone https://github.com/clementnuss/adapter
	git checkout linux-compat
	cd adapter

We then need to copy the `fwlib32.so` library to one of the Linux lib folder.
This library is the Fanuc FOCAS library used to communicate with Fanuc devices, and it is present in the `fanuc/` folder.

	
	cp adapter/fanuc/libfwlib32.so.1.0.0 /usr/local/lib/
	ln -s libfwlib32.so.1.0.0 libfwlib32.so.1
	ln -s libfwlib32.so.1.0.0 libfwlib32.so
	ldconfig

Once it is in place, we can check that it is found using:

	ld --verbose -lfwlib32

And check that the libraries it (it stands for the FOCAS library here, i.e. fwlib32) depends on are also available.

	ldd /usr/local/lib/libfwlib32.so

Then we can build the adapter using:

    mkdir build && cd build
	cmake ..
	make

The `make install` command installs the `adapter.ini` file into `/etc/mtconnect/adapter/`, and the `adapter_fanuc` binary in `/usr/local/bin/`

Run the adapter with:

	adapter_fanuc /etc/mtconnect/adapter/adapter.ini

The compiler should return the following output (with the `--verbose:

    $ fanuc/adapter_fanuc -v -c /etc/mtconnect/adapter/adapter.ini
    MTConnect Adapter - *nix edition - version 0.9.1
    2017-08-31T14:40:06.313906Z - Info: Showing hidden axis.
    2017-08-31T14:40:06.333517Z - Info: Adding sample macro 'gauge1' at location 500
    2017-08-31T14:40:06.335491Z - Info: Adding pmc 'Fovr' at location -12
    2017-08-31T14:40:06.336307Z - Info: Adding pmc 'r100' at location 50500
    2017-08-31T14:40:06.337735Z - Info: Adding parameter 'iochannel' at location 20
    2017-08-31T14:40:06.338541Z - Info: Adding parameter 'cuttime' at location 6754
    2017-08-31T14:40:06.339226Z - Info: Adding parameter 'powerontime' at location 6750
    2017-08-31T14:40:06.340877Z - Info: Adding diagnostic 'XposError' at location 15202
    2017-08-31T14:40:06.341779Z - Info: Adding diagnostic 'DCLink' at location 10752
    2017-08-31T14:40:06.343349Z - Info: Adding alarm 'alarm1' at location 1
    2017-08-31T14:40:06.344239Z - Info: Adding alarm 'alarm2' at location 2
    2017-08-31T14:40:06.344965Z - Info: Adding alarm 'alarm3' at location 3
    2017-08-31T14:40:06.345707Z - Info: Adding alarm 'alarm4' at location 4
    2017-08-31T14:40:06.347300Z - Info: Adding critical 'critical1' at location 0
    2017-08-31T14:40:06.348191Z - Info: Adding critical 'critical2' at location 0
    2017-08-31T14:40:06.348932Z - Info: Adding critical 'critical3' at location 0
    2017-08-31T14:40:06.349996Z - Info: Adding critical 'critical4' at location 0
    2017-08-31T14:40:06.353188Z - Info: Server started, waiting on port 7878


Notice it's listening on Port 7878 for an agent to make a request. It doesn't contact an actual Fanuc controller until an agent makes a request.

##### Next steps:
Get it do daemonize properly, and to be able to run either multiple instances of the program on different ports (without colliding log files as will happen now) OR to fork multiple processes based on how many hosts and ports are defined in the config file.
