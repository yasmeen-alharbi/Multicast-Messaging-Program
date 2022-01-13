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

	// Data received from server.
	char buf[BUFSIZ];

	// Address information.
	struct addrinfo hints, *servinfo, *p;

	// Return value from getaddrinfo().
	int rv;

	// IP address of new connection.
	char s[INET6_ADDRSTRLEN];

	if (argc != 3) // Error check the command line variables.
	{
		fprintf(stderr, "usage: receiver hostname port\n");
		exit(1);
	}

	// The port the receiver will be receiving from.
	char *port = argv[2];

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

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
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
		{
			perror("receiver: socket");
			continue;
		}

		// Create a TCP connection with the server.
		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
		{
			perror("receiver: connect");
			close(sockfd);
			continue;
		}

		break;
	}

	if (p == NULL) // Error check if the connection failed.
	{
		fprintf(stderr, "receiver: failed to connect\n");
		return 2;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);

	printf("receiver: connecting to %s\n", s);

	freeaddrinfo(servinfo); // All done with this structure.

	// Continue receiving new messages from the server.
	while(1)
	{
		if ((numbytes = recv(sockfd, buf, BUFSIZ, 0)) <= 0)
		{
			perror("recv");
			return -1;
		}

		buf[numbytes] = '\0';
		printf("receiver: '%s'\n", buf);
	}

	// Close the socket and free the buffer.
	close(sockfd);

	return 0;
}