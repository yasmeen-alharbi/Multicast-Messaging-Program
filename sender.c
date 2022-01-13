// Yasmeen Al-Harbi & Jordyn Espenant
// NSIDs: yaa300 & jde843
// CMPT 332 Section 1
// Assignment 4

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

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

int main(int argc, char *argv[])
{
	int sockfd, numbytes;

	// Address information.
	struct addrinfo hints, *servinfo, *p;

	// Return value from getaddrinfo().
	int rv;

	// IP address of new connection.
	char s[INET6_ADDRSTRLEN];

	// The port the sender will be sending to.
	char *port;

	if (argc != 3) // Error check the command line variables.
	{
		fprintf(stderr, "usage: sender hostname port\n");
		exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	// Get the port from command line argument
	port = argv[2];

	// Get the address information from the connection.
	if ((rv = getaddrinfo(argv[1], port, &hints, &servinfo)) != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// Loop through all the results and connect to the first we can.
	for (p = servinfo; p != NULL; p = p->ai_next)
	{
		// Create a stream socket.
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
							 p->ai_protocol)) == -1)
		{
			perror("sender: socket");
			continue;
		}

		// Create a TCP connection with the server.
		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
		{
			perror("sender: connect");
			close(sockfd);
			continue;
		}

		break;
	}

	if (p == NULL) // Error check if the connection failed.
	{
		fprintf(stderr, "sender: failed to connect\n");
		return 2;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
			  s, sizeof s);
	printf("sender: connecting to %s\n", s);

	freeaddrinfo(servinfo); // All done with this structure.

	// Read a message and save only BUFSIZ characters.
	char *buffer;
	size_t bufsize = 0;
	size_t chars;
	buffer = (char *)malloc(sizeof(char) * (bufsize + 1));
		
	chars = getline(&buffer, &bufsize, stdin);
	buffer[chars - 1] = '\0';

	// While the user hasn't terminated the connection or the end of file hasn't been reached.
	while (strcmp(buffer, "exit") != 0 && !feof(stdin))
	{
		// Send a message to the server.
		if ((numbytes = send(sockfd, buffer, BUFSIZ, 0)) == -1)
		{
			perror("recv");
			exit(1);
		}

		printf("sender: message sent.\n");

		// Read another message.
		chars = getline(&buffer, &bufsize, stdin);
		buffer[chars - 1] = '\0';
	}

	// If  the user entered exit, then send that "exit" to the server and print a notification.
	if (strcmp(buffer, "exit") == 0)
	{
		if ((numbytes = send(sockfd, buffer, BUFSIZ, 0)) == -1)
		{
			perror("recv");
			exit(1);
		}

		printf("sender: exiting.\n");
	}

	// Close the socket and free the buffer.
	close(sockfd);
	free(buffer);

	return 0;
}