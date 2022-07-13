## Seeder Server


| Directory		| Description								|
| ----------------------- | -------------------------------------------------------------	|
| build			| Build directory with shell script				|
| doc			| documentation							|
| install			| compiled binaries which can be run out of box	|
| src				| source files								|

### Build

In a fresh Ubuntu environment run the below script only once to install 
the necessary dependencies. Admin persmissions would be required to install.


    $ cd build
    $ ./build_dependencies.sh

For building the source kindly issue the following command.
        
    $ cd build
    $ ./build.sh

This would compile the source and copy the built binaries to `install` folder.


### Without building

The zip file is provided with built binaries in `install` folder which can be run out of the box. 


### Run server and client

For running the server, open a command prompt

	$ cd install
	$ ./server

For running the client, open another command prompt

	$ cd install
	$ ./client

### Explanation

Server doesn't print any logs and all the operations are requested from client. Below is the prompt on client asking for input after it is run.

	$ ./client 

	Please specify the option below:
	1. Send hello to server
	2. Get peer list
	3. Nodes which are alive during the last x secs/mins/hrs/days etc
	4. Quit/Shutdown client

- Send a hello to server by specifying `1` and the client would be added to the peer list
- Performing any operation `2 or 3` before specifying sending hello to server would print this error
	
	```
	[error]ProcessInputThreadFunction[258]: Input error or send hello before inputting other choice. Choose again between [1-4].
	```
	
- Now specify `2` to get a list of peers including the current client.

	```c
	$ ./client 

	Please specify the option below:
	1. Send hello to server
	2. Get peer list
	3. Nodes which are alive during the last x secs/mins/hrs/days etc
	4. Quit/Shutdown client
	1

	Please specify the option below:
	1. Send hello to server
	2. Get peer list
	3. Nodes which are alive during the last x secs/mins/hrs/days etc
	4. Quit/Shutdown client
	2
	Peer IP addresses:Peer ports
	127.0.0.1:1471

	```

- Specify `3` to get duration alive status. This would ask for more information like `Specify time period like 20s, 10m, 2h, 1d:`

	```c
	$ ./client 

	Please specify the option below:
	1. Send hello to server
	2. Get peer list
	3. Nodes which are alive during the last x secs/mins/hrs/days etc
	4. Quit/Shutdown client
	1

	Please specify the option below:
	1. Send hello to server
	2. Get peer list
	3. Nodes which are alive during the last x secs/mins/hrs/days etc
	4. Quit/Shutdown client
	2
	Peer IP addresses:Peer ports
	127.0.0.1:1471

	Please specify the option below:
	1. Send hello to server
	2. Get peer list
	3. Nodes which are alive during the last x secs/mins/hrs/days etc
	4. Quit/Shutdown client
	3
	Specify time period like 20s, 10m, 2h, 1d:
	20s
	Peer IP addresses:Peer ports
	127.0.0.1:1471

	```
	
- Specify `4`to shut down the client

	```c
	$ ./client 

	Please specify the option below:
	1. Send hello to server
	2. Get peer list
	3. Nodes which are alive during the last x secs/mins/hrs/days etc
	4. Quit/Shutdown client
	1

	Please specify the option below:
	1. Send hello to server
	2. Get peer list
	3. Nodes which are alive during the last x secs/mins/hrs/days etc
	4. Quit/Shutdown client
	2
	Peer IP addresses:Peer ports
	127.0.0.1:1471

	Please specify the option below:
	1. Send hello to server
	2. Get peer list
	3. Nodes which are alive during the last x secs/mins/hrs/days etc
	4. Quit/Shutdown client
	3
	Specify time period like 20s, 10m, 2h, 1d:
	2s
	Peer IP addresses:Peer ports
	127.0.0.1:1471

	Please specify the option below:
	1. Send hello to server
	2. Get peer list
	3. Nodes which are alive during the last x secs/mins/hrs/days etc
	4. Quit/Shutdown client
	4
	Client shutting down
	```

- Further the client periodically (every 10 seconds) sends a ping to the server, if the server doesn't receive a ping for more than 10 seconds it assumes the client has died and proceeds to remove the client from the peer list it maintains. 
- This feature can be tested by opening multiple clients from various command prompts and registering with the server by sending a `1` from all the clients and closing one of them and waiting for 10 seconds or so and issuing a `2` from the active client.
- Further, debug logs can be enabled by defining PRINT_LOGS in `src/util/util.h`

	```
	#define PRINT_LOGS                  (PRINT_ERROR | PRINT_INFO | PRINT_DEBUG)
	```

