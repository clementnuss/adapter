#Introduction
These instructions are specific to building and running **only** the **Fanuc** mtconnect adapter on the linux-compat branch of the mmxe/adapter repository.


The fanuc adapter depends on a header file and a binary library for Linux from Fanuc (fwlib32). The library is 32 bit only.  The files are included in this repository. The documentation on the CD from Fanuc states that these files are freely redistributable. PDF docs from the CD are also included in the "fanuc" folder.

#Installation
## Prepare
To install on a x64 system, add the libc6-dev-i386 and g++ multilib libraries.
```
sudo apt-get install libc6-dev-i386 g++-multilib
```

## Build and Install

```
git clone https://github.com/mmxe/adapter.git
git checkout linux-compat
cd adapter
cmake .
sudo make install
```
At the moment you must manually create a folder for log files: /var/log/adapter/ and give permissions to the user that will run the adapter.

```
sudo mkdir /var/log/adapter
sudo chown your_username:your_username /var/log/adapter
OR
sudo chmod o+w /var/log/adapter
```

# Configure and Run
Install process doesn't copy config file.

If you have a multi controller environment, each controller needs its own .ini file in a separate folder.

```
$ mkdir ~/adapter-machine1
$ mkdir ~/adapter-machine2
$ cp ./fanuc/adapter.ini ~/adapter-machine1/adapter1.ini
$ cp ./fanuc/adapter.ini ~/adapter-machine2/adapter2.ini
```

```
$ cd ~/adapter-machine1
$ nano adapter1.ini (change the machine tool IP address and localhost listening port number so they are unique for each adapter instance)
$ adapter_fanuc -c adapter1.ini &
$ cd ~/adapter-machine2
$ nano adapter2.ini (change the machine tool IP address and localhost listening port number)
$ adapter_fanuc -c adapter2.ini &
```
Log files are named based on the config file name. You should make unique config file names if you run more than once adapter instance. Otherwise log file names will collide and cause problems and confusion.

# Usage
```
$ adapter_fanuc -h
MTConnect Adapter - *nix edition - version 0.9.0

Options: 
	-c,--conf	specify config file location
	-v,--verbose	messages will be directed to stderr instead of "adapter.log"
			in the directory where you start the adapter
	-db,--debug	get debug messages in the log file (or stderr with "-v")
 	-h,--help	this help message

	The log files are written to /var/log/adapter/. Create an "adapter" folder in /var/log and give permission for the adapter to write to it.
	The log file name is copied from the .ini config file name.
	Use unique config file names if you run more than one adapter instance.
```


### Successful Launch
```
2016-09-15T17:11:13.347944Z - Info: Arguments: 1
2016-09-15T17:11:13.348857Z - Info: Ini File: adapter/fanuc/adapter.ini
2016-09-15T17:11:13.351756Z - Info: Showing hidden axis.
2016-09-15T17:11:13.352613Z - Info: Adding sample macro 'gauge1' at location 500
2016-09-15T17:11:13.354319Z - Info: Adding pmc 'Fovr' at location -12
2016-09-15T17:11:13.355044Z - Info: Adding pmc 'r100' at location 50500
2016-09-15T17:11:13.355729Z - Info: Adding parameter 'iochannel' at location 20
2016-09-15T17:11:13.355994Z - Info: Adding parameter 'cuttime' at location 6754
2016-09-15T17:11:13.356081Z - Info: Adding parameter 'powerontime' at location 6750
2016-09-15T17:11:13.356496Z - Info: Adding diagnostic 'XposError' at location 15202
2016-09-15T17:11:13.356558Z - Info: Adding diagnostic 'DCLink' at location 10752
2016-09-15T17:11:13.356643Z - Info: Adding alarm 'alarm1' at location 1
2016-09-15T17:11:13.356721Z - Info: Adding alarm 'alarm2' at location 2
2016-09-15T17:11:13.356923Z - Info: Adding alarm 'alarm3' at location 3
2016-09-15T17:11:13.356969Z - Info: Adding alarm 'alarm4' at location 4
2016-09-15T17:11:13.357281Z - Info: Adding critical 'critical1' at location 0
2016-09-15T17:11:13.357366Z - Info: Adding critical 'critical2' at location 0
2016-09-15T17:11:13.357670Z - Info: Adding critical 'critical3' at location 0
2016-09-15T17:11:13.357741Z - Info: Adding critical 'critical4' at location 0
2016-09-15T17:11:13.357820Z - Info: Server started, waiting on port 7878
2016-09-21T13:46:12.908673Z - Info: Connected to: 127.0.0.1 on port 41939
2016-09-21T13:46:12.908707Z - Info: Connecting to host 10.9.8.21 on port 8193
2016-09-21T13:46:12.999998Z - Info: Result: 0
2016-09-21T13:46:13.000014Z - Info: Configuring...
2016-09-21T13:46:13.000058Z - Info: Configuration for path 1:
2016-09-21T13:46:13.000065Z - Info:   Max Axis: 32
2016-09-21T13:46:13.000068Z - Info:   CNC Type:  0
2016-09-21T13:46:13.000071Z - Info:   MT Type:  M
2016-09-21T13:46:13.000073Z - Info:   Series: D5
2016-09-21T13:46:13.000076Z - Info:   Version: 10
2016-09-21T13:46:13.000078Z - Info:   Axes: 03
2016-09-21T13:46:13.047767Z - Info: Axis 0 : X
2016-09-21T13:46:13.047782Z - Info: Axis X #0 - actual (unit 0 flag 0x1)
2016-09-21T13:46:13.047786Z - Info:  Units: mm
2016-09-21T13:46:13.047811Z - Info: Axis 1 : Y
2016-09-21T13:46:13.047816Z - Info: Axis Y #1 - actual (unit 0 flag 0x1)
2016-09-21T13:46:13.047818Z - Info:  Units: mm
2016-09-21T13:46:13.047834Z - Info: Axis 2 : Z
2016-09-21T13:46:13.047837Z - Info: Axis Z #2 - actual (unit 0 flag 0x1)
2016-09-21T13:46:13.047840Z - Info:  Units: mm
2016-09-21T13:46:13.079889Z - Info: Spindle 0 : S1
2016-09-21T13:46:13.080063Z - Info: Current path: 1, maximum paths: 1
2016-09-21T13:46:13.409188Z - Warning: Cannot cnc_rdntool for path 1: 0
2016-09-21T13:46:13.409342Z - Warning: Trying modal tool number
```

Note that the adapter is listening on Port 7878 for an agent to make a request. 

It doesn't contact the Fanuc controller until an agent makes a request.

# Next steps
* Make the linux compatible adapter daemonize (background) properly. 


