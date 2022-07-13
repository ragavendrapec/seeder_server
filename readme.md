## Seeder Server


| Directory		| Description								|
| ----------------------- | -------------------------------------------------------------	|
| build			| Build directory with shell script				|
| doc			| documentation							|
| install			| compiled binaries which can be run out of box	|
| src				| source files								|

### Build

In a fresh Ubuntu environment run the below script only once to install 
the necessary dependencies. Admin persmissions would be required to install


    $ cd build
    $ ./build_dependencies.sh

For building the source kindly issue the following command
        
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
