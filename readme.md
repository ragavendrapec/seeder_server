## Seeder Server


| Directory		| Description								|
| ----------------------- | -------------------------------------------------------------	|
| build			| Build directory with shell script				|
| doc			| documentation							|
| install			| compiled binaries which can be run out of box	|
| src				| source files								|

### Build

In a fresh Ubuntu environment run the below script only once to install 
the necessary dependencies. Admin permissions would be required to install.


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

By default server runs on port 54321

For running the client, open another command prompt (here the default server address is assumed).

	$ cd install
	$ ./client

Client can also be run by specifying the server's IPv4 address as below (if the server and client are run from different machines).

	$ cd install
	$ ./client 192.168.10.175

### Explanation

Server doesn't print any logs and all the operations are requested from client. Below is the prompt on client asking for input after it is run.

	$ ./client 

	Please specify the option below:
	1. Send hello to server
	2. Get peer list
	3. Nodes which are alive during the last x secs/mins/hrs/days etc
	4. Msg peer nodes
	5. Quit/Shutdown client


- Send a hello to server by specifying `1` and the client would be added to the peer list
- Performing any operation `2 or 3` before specifying sending hello to server would print this error
	
	```
	[error]ProcessInputThreadFunction[366]: Input error or send hello before inputting other choice. Choose again between [1-5].
	```
	
- Now specify `2` to get a list of peers including the current client. Here *peer_info_list* points to the IP addresses of the peers that the current client is messaging. Since, this is the only client and hasn't messaged any other client it is empty below.

	```c
	$ ./client 

	Please specify the option below:
	1. Send hello to server
	2. Get peer list
	3. Nodes which are alive during the last x secs/mins/hrs/days etc
	4. Msg peer nodes
	5. Quit/Shutdown client
	1

	Please specify the option below:
	1. Send hello to server
	2. Get peer list
	3. Nodes which are alive during the last x secs/mins/hrs/days etc
	4. Msg peer nodes
	5. Quit/Shutdown client
	2
	IP addresses:Ports*peer_info_list
	127.0.0.1:48452*peer_info_list*

	```

- Specify `3` to get duration alive status. This would ask for more information like `Specify time period like 20s, 10m, 2h, 1d:`

	```c
	$ ./client 

	Please specify the option below:
	1. Send hello to server
	2. Get peer list
	3. Nodes which are alive during the last x secs/mins/hrs/days etc
	4. Msg peer nodes
	5. Quit/Shutdown client
	1

	Please specify the option below:
	1. Send hello to server
	2. Get peer list
	3. Nodes which are alive during the last x secs/mins/hrs/days etc
	4. Msg peer nodes
	5. Quit/Shutdown client
	2
	IP addresses:Ports*peer_info_list
	127.0.0.1:48452*peer_info_list*

	Please specify the option below:
	1. Send hello to server
	2. Get peer list
	3. Nodes which are alive during the last x secs/mins/hrs/days etc
	4. Msg peer nodes
	5. Quit/Shutdown client
	3
	Specify time period like 20s, 10m, 2h, 1d:
	10s
	IP addresses:Ports
	127.0.0.1:48452


	```
	
- Specify `4` to message peer nodes. For this open another command prompt and send a hello to server first, now specifying `2` would show two clients listed as below `127.0.0.1:48452` and `127.0.0.1:51426`. Now specify `4` in the first command prompt and specify the second client's IP address and port number which is `127.0.0.1:51426`. Now specifying `2` again would list `127.0.0.1:51426` as a peer for `127.0.0.1:48452`. For now the client 1's IP address is not added to client 2's list (this can be taken as an improvement). 

	```c
	$ ./client 

	Please specify the option below:
	1. Send hello to server
	2. Get peer list
	3. Nodes which are alive during the last x secs/mins/hrs/days etc
	4. Msg peer nodes
	5. Quit/Shutdown client
	2
	IP addresses:Ports*peer_info_list
	127.0.0.1:48452*peer_info_list*
	127.0.0.1:51426*peer_info_list*

	Please specify the option below:
	1. Send hello to server
	2. Get peer list
	3. Nodes which are alive during the last x secs/mins/hrs/days etc
	4. Msg peer nodes
	5. Quit/Shutdown client
	4
	Specify peer's IP address and port (eg. 127.0.0.1:47851):
	127.0.0.1:51426

	Please specify the option below:
	1. Send hello to server
	2. Get peer list
	3. Nodes which are alive during the last x secs/mins/hrs/days etc
	4. Msg peer nodes
	5. Quit/Shutdown client
	2
	IP addresses:Ports*peer_info_list
	127.0.0.1:48452*peer_info_list*127.0.0.1:51426*
	127.0.0.1:51426*peer_info_list*

	```
- Specify `5` to shutdown the client.
	Please specify the option below:
	1. Send hello to server
	2. Get peer list
	3. Nodes which are alive during the last x secs/mins/hrs/days etc
	4. Msg peer nodes
	5. Quit/Shutdown client
	5
	Client shutting down

- Further the client periodically (every 10 seconds) sends a ping and peer list to the server, if the server doesn't receive a ping for more than 10 seconds it assumes the client has died and proceeds to remove the client from the node list it maintains. 
- This feature can be tested by opening multiple clients from various command prompts and registering with the server by sending a `1` from all the clients and closing one of them and waiting for 10 seconds or so and issuing a `2` from the active client.
- Further, debug logs can be enabled by defining PRINT_LOGS in `src/util/util.h`

	```
	#define PRINT_LOGS                  (PRINT_ERROR | PRINT_INFO | PRINT_DEBUG)
	```
- My testing were done on Ubuntu 20.04 running in Virtual box.
