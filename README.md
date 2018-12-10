# Overview
Isaac Gentz
CS457
Project 1 - Chat Server and Client

This is my project 1! I worked alone for this project and have a functional, multithreaded chat server as well as multithreaded chat client. You can confirm they are multithreaded because the server doesn't block waiting for messages or while sending messages, and the same for the client. You can also look at `htop` and see that a new thread gets launched on the server when a client connects. The client also has 3 threads running at any one time. I was able to get some blocking calls to NOT block by using timeouts of ~1 sec (such as when waiting for input). This let me launch the `recv` into a new thread and still be able to check that thread's status, so that I could have a graceful shutdown for both the server and the client.

There are two programs - the server and the client. 

# Server
**Building**
To build the server, simply go to the `server` directory and type `make`. 
This will create the `server` executable that can be invoked like `./server`. 
Use the `-H` option to see command line args you can pass to it

**Other Info**

 - You can configure the server by modifying the `conf/server.conf` file. If you use one of the command line flags, it will over-write any option specified in `server.conf`
 - You can also specify a list of banned users in the `db/banuser.txt` file. In here put a nickname on each line for each user you don't want to be able to log in. You can also put an IP address here!!
- You can also specify a banner message for what you want the server to say when someone logs in by putting one line into `db/banner.txt`
- The server will log events by default to `logs/server.log`. This can be changed with the `-L` command line flag or by specifying it in a configuration file
- If no config file is specified, the server will run with some defined constants
- When the server first starts you can see it display a list of commands you can type directly into the terminal to control the server. Commands should be prepended with a `/`


# Client-GUI
*note I left my old client in here under the 'client' directory for history's sake, but it will have some undefined behavior with this version of the server*

**Building**
 - Navigate to the `GUI` directory and type `qmake`. This will run QT and spit out makefiles that can be used to buld the project
 - Now simply type make and the project will be compiled, creating a `cs457_client` executable
 - Use `./cs457_client -H` to see command line arguments you can pass to it

**Other Info**
 - I didn't quite get through all 80% of the commands, but I got the essential ones plus a few niceties. You can type `/help` to see what IRC commands can be typed into the client. If no `/` is specified, the input will be considered a general message that will be sent to the server and then relayed to the channel you are currently in
 - When you first log in to the server, you will be placed on the `#general` channel. You can change channels using the `/join` command
 - You can also send `/privmsg` to either a user or a different channel, no matter which channel you are currently on
 - The `/connect` command is specified by the client automatically when first invoked, so it can't be called manually
 - You can also `/kill` another user, which will remove them from the server. They will get a message that they've been killed
 - You can also `/kick` a user, which will only remove them from their current channel and place them back on `#general`
 - `/ping` will return a `pong` from the server
 - Exit the client using the `/quit` command
 - `/info` Will get server info
 - `/time` will get the server time
 - `/userip` will get the ip address of another user
 - `/users` will list all the users currentlyu logged in
 - `/version` will get the server's version
 - The `/user` command is similar to the `/connect` command because it is invoked automatically to pass along the login information
- The client will log things by default to `client.log`
- You can pass in a custom `client.conf` file to override default options if you don't want to keep typing command line args each time
- Command line args will over-rule config file options
- You can switch between channels by double clicking on them in the left pane
- Send messages by typing in the bottom-right box and hitting `enter`

