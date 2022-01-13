// Yasmeen Al-Harbi & Jordyn Espenant
// NSIDs: yaa300 & jde843
// CMPT 332 Section 1
// Assignment 4

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>

/* The port senders will be connecting to. */
#define PORTSEND "30420"

/* The port receivers will be connecting to. */
#define PORTRECV "30421"

/* The number of pending connections the queue will hold. */
#define BACKLOG 10 // how many pending connections queue will hold

/* Data received and sent. */
char buffer[BUFSIZ];

/* Mutex and condition variable for thread usage. */
pthread_mutex_t buffer_mutex;
pthread_cond_t buffer_cond;

/* A struct used to pass an IP address and file descriptor to senderRoutine(). */
typedef struct s_connection_args {
	int new_fd;
	char* ip;
} connection_args;

/*
A function that reaps all dead processes.
*/
void sigchld_handler(int s)
{
	(void)s; // quiet unused variable warning

	// waitpid() might overwrite errno, so we save and restore it:
	int saved_errno = errno;

	while (waitpid(-1, NULL, WNOHANG) > 0)
		;

	errno = saved_errno;
}

/*
A function that gets sockaddr, IPv4 or IPv6.
*/
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET)
	{
		return &(((struct sockaddr_in *)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

/**
 * Wrapper function for accepting connections from sender clients.
 */
void *senderRoutine(void *arg)
{
	// Dereference the passed argument and get the IP and file descriptor.
	connection_args args = *(connection_args*) arg;
	int new_sock = args.new_fd;
	char* ip = args.ip;

	int temp;
	char temp_buf[BUFSIZ];

	// While there are messages being received.
	while ((temp = recv(new_sock, temp_buf, BUFSIZ, 0)) > 0)
	{
		// Lock the mutex and add the IP and port number to the message, then unlock the mutex.
		if (pthread_mutex_lock(&buffer_mutex) != 0) {
			printf("Failed to lock mutex\n");
			return NULL;
		}

		char message[BUFSIZ] = "";
		strcat(message, ip);
		strcat(message, ", ");
		strcat(message, PORTSEND);
		strcat(message, ": ");
		strcat(message, temp_buf);

		strncpy(buffer, message, BUFSIZ-1);
		buffer[BUFSIZ] = '\0';
		printf("server: message received\n");
		if (pthread_mutex_unlock(&buffer_mutex) != 0) {
			printf("Failed to lock mutex\n");
			return NULL;
		}

		// Since the data has been successfully received and the buffer is no longer empty, we can broadcast to all other threads.
		pthread_cond_broadcast(&buffer_cond);
	}


	if (temp == 0) // If a sender disconnected.
	{
		printf("A sender disconnected!\n");
	}

	// Close the passed socket and free the passed arguments.
	close(new_sock);
	free(arg);

	return NULL;
}

/**
 * Wrapper function for handling all sender connections.
 */
void *senderStart(void* arg) 
{
	/*~~~~~~~~~~~~~~~~~~ Set up connection for senders ~~~~~~~~~~~~~~~~~~*/
	// Listen on sock_fd, get new connections on new_fd.
	int sockfd, new_fd;

	// Connector's address information.
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr;

	// Size of the address,
	socklen_t sin_size;

	struct sigaction sa;
	int yes = 1;

	// IP address of new incoming connection.
	char s[INET6_ADDRSTRLEN];

	// Return value from getaddrinfo().
	int rv;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	// Get the address information from the incoming connection.
	if ((rv = getaddrinfo(NULL, PORTSEND, &hints, &servinfo)) != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return NULL;
	}

	/*~~~~~~~~~~~~~~~~~~~~ Bind to port for senders ~~~~~~~~~~~~~~~~~~~~*/
	// Loop through all the results and bind to the first port we can.
	for (p = servinfo; p != NULL; p = p->ai_next)
	{
		// Create a new socket.
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
							 p->ai_protocol)) == -1)
		{
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
					   sizeof(int)) == -1)
		{
			perror("setsockopt");
			exit(1);
		}


		// Bind the socket to the given address.
		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
		{
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	freeaddrinfo(servinfo); // All done with this structure.

	if (p == NULL) // Error check if the new socket was bound to the address.
	{
		fprintf(stderr, "server: failed to bind\n");
		exit(1);
	}

	// Listen to all incoming connections.
	if (listen(sockfd, BACKLOG) == -1)
	{
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; // Reap all dead processes.
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1)
	{
		perror("sigaction");
		exit(1);
	}

	printf("server: waiting for sender connections...\n");

	/* Main accept() loop */
	sin_size = sizeof their_addr;
	while ((new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size)))
	{
		// ~~~~~~~~~~~~~~~~~~~ ACCEPT CONNECTION FOR SENDERS ~~~~~~~~~~~~~~~~~~~
		inet_ntop(their_addr.ss_family,
				  get_in_addr((struct sockaddr *)&their_addr),
				  s, sizeof s);

		printf("server: got connection from %s\n", s);

		// Add the new socket's file descriptor and the IP address of the sender to the structure.
		connection_args* args = (connection_args*) malloc(sizeof(connection_args));
		args->new_fd = new_fd;
		args->ip = s;

		// Create a detatched thread and pass the args structure as an argument.
		pthread_t new_connection;
		pthread_attr_t attr;
		if (pthread_attr_init(&attr) != 0) {
			printf("server: pthread_attr_init() error\n");
			return NULL;
		}
		if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0) {
			printf("server: pthread_attr_setdetachstate() error\n");
			return NULL;
		}
		
		if (pthread_create(&new_connection, &attr, senderRoutine, (void*) args) != 0) {
			printf("server: pthread_create() error\n");
			return NULL;
		} 
	}

	// Close the socket.
	close(sockfd);

	// If there was an error with accepting the connection.
	if (new_fd == -1)
	{
		perror("accept");
		return NULL;
	}
	
	return NULL;
}

/**
 * Wrapper function for accepting connections from receiver clients.
 */
void *receiverRoutine(void *arg)
{
	// Dereference the passed argument to get the file descriptor.
	int new_sock = *(int*) arg;

	int temp;

	// Lock the mutex and wait until some data has been written into the buffer.
	if (pthread_mutex_lock(&buffer_mutex) != 0) {
		printf("Failed to lock mutex\n");
	}

	while (1) 
	{
		if (pthread_cond_wait(&buffer_cond, &buffer_mutex) != 0) {
			printf("Failed to wait for buffer\n");
			return NULL;
		}
		else {
			temp = send(new_sock, buffer, BUFSIZ, MSG_NOSIGNAL);
			if (temp <= 0) { // If a receiver disconnected, unlock the mutex and break.
				printf("A receiver disconnected!\n");
				if (pthread_mutex_unlock(&buffer_mutex) != 0) {
					printf("Failed to unlock mutex\n");
				}
				break;
			}

			// Otherwise, unlock the mutex normally and loop again.
			printf("server: message sent\n");
			if (pthread_mutex_unlock(&buffer_mutex) != 0) {
				printf("Failed to unlock mutex\n");
			}
		}
	}

	// Close the passed socket and free the passed arguments.
	close(new_sock);
	free(arg);

	return NULL;
}

/**
 * Wrapper function for handling all receiver connections.
 */
void *receiverStart(void* arg) 
{
	/*~~~~~~~~~~~~~~~~~~ Set up connection for receivers ~~~~~~~~~~~~~~~~~~*/
	// Listen on sock_fd, get new connections on new_fd.
	int sockfd, new_fd;

	// Connector's address information.
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr;

	// Size of the address.
	socklen_t sin_size;

	struct sigaction sa;
	int yes = 1;

	// IP address of new incoming connection.
	char s[INET6_ADDRSTRLEN];

	// Return value from getaddrinfo().
	int rv;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	// Get the address information from the incoming connection.
	if ((rv = getaddrinfo(NULL, PORTRECV, &hints, &servinfo)) != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return NULL;
	}

	/*~~~~~~~~~~~~~~~~~~~~ Bind to port for receivers ~~~~~~~~~~~~~~~~~~~~*/
	// Loop through all the results and bind to the first port we can.
	for (p = servinfo; p != NULL; p = p->ai_next)
	{
		// Create a new socket.
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
							 p->ai_protocol)) == -1)
		{
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
					   sizeof(int)) == -1)
		{
			perror("setsockopt");
			exit(1);
		}

		// Bind the socket to the given address.
		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
		{
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	freeaddrinfo(servinfo); // All done with this structure.

	if (p == NULL) // Error check if the new socket was bound to the address.
	{
		fprintf(stderr, "server: failed to bind\n");
		exit(1);
	}

	// Listen to all incoming connections.
	if (listen(sockfd, BACKLOG) == -1)
	{
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; // Reap all dead processes.
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1)
	{
		perror("sigaction");
		exit(1);
	}

	printf("server: waiting for receiver connections...\n");

	/* Main accept() loop */
	sin_size = sizeof their_addr;
	while ((new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size)))
	{
		// ~~~~~~~~~~~~~~~~~~~ ACCEPT CONNECTION FOR RECEIVERS ~~~~~~~~~~~~~~~~~~~
		inet_ntop(their_addr.ss_family,
				  get_in_addr((struct sockaddr *)&their_addr),
				  s, sizeof s);

		printf("server: got connection from %s\n", s);

		int *new_sock = malloc(1);
		*new_sock = new_fd;

		// Create a detatched thread and pass the new socket's file descriptor as an argument.
		pthread_t new_connection;
		pthread_attr_t attr;
		if (pthread_attr_init(&attr) != 0) {
			printf("server: pthread_attr_init() error\n");
			return NULL;
		}
		if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0) {
			printf("server: pthread_attr_setdetachstate() error\n");
			return NULL;
		}
		if (pthread_create(&new_connection, &attr, receiverRoutine, (void*) new_sock) != 0) {
			printf("server: pthread_create() error\n");
			return NULL;
		} 
	}

	// Close the socket.
	close(sockfd);

	// If there was an error with accepting the connection.
	if (new_fd == -1)
	{
		perror("accept");
		return NULL;
	}
	
	return NULL;
}

int main(void)
{
	printf("server: sender clients use port %s\n", PORTSEND);
	printf("server: receiver clients use port %s\n", PORTRECV);

	// Initialize the mutex and condition variable.
	if (pthread_mutex_init(&buffer_mutex, NULL) != 0) 
	{
		printf("Failed to init mutex\n");
		return -1;
	}
	if (pthread_cond_init(&buffer_cond, NULL) != 0) 
	{
		printf("Failed to init condition variable\n");
		return -1;
	}

	// Sender thread:
	pthread_t sender;
	// Receiver thread:
	pthread_t receiver;


	// Create a thread for a connection with a sender client and a thread for a connection with a receiver client.
	if (pthread_create(&sender, NULL, &senderStart, NULL) != 0)
	{
		printf("Failed to create a thread for a sender connection.\n");
		return 1;
	}
	if (pthread_create(&receiver, NULL, &receiverStart, NULL) != 0)
	{
		printf("Failed to create a thread for a receiver connection.\n");
		return 1;
	}

	// After the threads finish executing, join them.
	if (pthread_join(sender, NULL) != 0)
	{
		printf("Failed to join a thread for a sender connection.\n");
		return 1;
	}
	if (pthread_join(receiver, NULL) != 0)
	{
		printf("Failed to join a thread for a receiver connection.\n");
		return 1;
	}

	// Destroy the mutex and condition variable.
	if (pthread_mutex_destroy(&buffer_mutex) != 0)
	{
		printf("Failed to destroy mutex.\n");
		return 1;
	}
	if (pthread_cond_destroy(&buffer_cond) != 0)
	{
		printf("Failed to destroy mutex.\n");
		return 1;
	}

	return 0;
}