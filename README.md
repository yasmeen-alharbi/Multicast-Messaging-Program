<h1 align="center"> Multicast-Messaging-Program </h1>
<h3 align="center"> A basic server with sender and receiver clients using TCP. </h3>

<h2 align="left"> To Run: </h2>

- Type 'make'
- Set up server by typing 'server'

- Set up sender(s) by typing 'sender <hostname> <port>'
    - You can use port 30420 (as specified by sender port in server)

- Set up receiver(s) by typing 'receiver <hostname> <port>'
    - You can use port 30421 (as specified by receiver port in server)

<h2 align="left"> Overview: </h2>

<h3 align="left"> server.c: </h3>

- We use ports 30420 and 30421
- We create a global buffer with a lock and condition variable
- We create two threads to do the bulk of the work: one for setting up 
sender connections and one for setting up receiver connections
    - The sender connection thread will spawn a new detached thread for every new sender that connects to the server
    - The receiver connection thread will spawn a new detached thread for every new receiver that connects to the server
- The two main threads are initialized and called in main, and then everything 
is destroyed in main when those threads return

<h3 align="left"> sender.c: </h3>

- Takes a hostname and port as command line arguments
- Makes a connection to that port
- Begins a loop of reading a line from standard input and sending each line 
to the socket
    - If the user ctrl+c program or types "exit", the program terminates

<h3 align="left"> receiver.c: </h3>

- Takes a hostname and a port as command line arguments
- Makes a connection to that port
- Begins a loop of receiving bytes from the server and printing each line to 
output
    - If the connection is lost, the program terminates
    - The program can also terminate with ctrl+c

<h2 align="left"> Limitations: </h2>

- We can only send/receive/process messages with a size equal to the BUFSIZ 
macro (which is machine dependent)
- If a receiver client disconnects, the server will not know about it until 
after it receives two more messages (a result of the TCP connection)
- If the server terminates, the receivers will terminate, but the senders 
will only terminate after trying to send two more messages (a result of the 
TCP connection)
- The server can successfully process most normal standard input (including 
piping), HOWEVER, it cannot handle having a file redirected as standard input
- The BACKLOG macro means there will be only a max of 10 pending connections 
in the queue
- If the ports defined as macros in the server are in use, connections are not 
possible until they are manually changed within the server to free ports