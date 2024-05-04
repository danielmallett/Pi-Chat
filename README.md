# Pi-Chat
Multi-threaded chat-server written in C (originally for a systems programming class at the University of Tennessee - Knoxville).
Fun to run on a raspberry pi on your local network.


## Usage
compile with `make`

**Running the Server:**
`bin/chat-server <port> <chat-room-names ...>`

eg: `bin/chat-server 8910 General Movies Video-Games`


**Connecting to the Server:**
`nc <hostname> <port>`

Connect to the server from another machine with [netcat](https://netcat.sourceforge.net/). 

eg: `nc pi 8910`


**Example Output:**
```
$ nc localhost 8910
Chat Rooms:

General:
Movies:
Video-Games: Melina

Enter your chat name (no spaces):
tarnished
Enter chat room:
Video-Games
tarnished has joined
Margit has joined
yo
tarnished: yo
Melina: Greetings. Traveller from beyond the Fog. I offer you an accordion.
Margit: foul tarnished...someone must extinguish thy flame
Margit has left
...
```
