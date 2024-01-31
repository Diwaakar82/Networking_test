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
#include <ifaddrs.h>
#include <time.h>
#include <sys/poll.h>

#define PORT "8080"			//Port used for connections
#define BACKLOG 2			//Pending connections queue can hold

//Fetch internet address
void *get_in_addr (struct sockaddr *sa)
{
	if (sa -> sa_family == AF_INET)
		return &(((struct sockaddr_in *) sa) -> sin_addr);
		
	return &(((struct sockaddr_in6 *) sa) -> sin6_addr);
}

//Connect to a socket
struct addrinfo *connect_to_socket (int *sockfd, struct addrinfo *servinfo)
{
	struct addrinfo *p;
	int yes = 1;
	
	for (p = servinfo; p != NULL; p = p -> ai_next)
	{   
		if ((*sockfd = socket (p -> ai_family, p -> ai_socktype, p -> ai_protocol)) == -1)
		{
			perror ("server: socket");
			continue;
		}
		if (setsockopt (*sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof (int)) == -1)
		{
			perror ("setsockopt");
			exit (1);
		}
		if (bind (*sockfd, p -> ai_addr, p -> ai_addrlen) == -1)
		{
			close (*sockfd);
			perror ("server: bind");
			continue;
		}
	}
	
	return p;
}

//Initialize address information
void initialize (struct addrinfo *hints)
{
	memset (hints, 0, sizeof *hints);
	
	hints -> ai_family = AF_UNSPEC;
	hints -> ai_socktype = SOCK_STREAM;
	hints -> ai_flags = INADDR_ANY;
}

//Send message
void send_message (int sockfd, char *msg)
{
	if (send (sockfd, msg, 1000, 0) == -1)
	{
		perror ("send");
		exit (0);
	}
}

//Receive message
int receive_message (int sockfd, char *buf)
{
	int numbytes;
	memset (buf, 0, 1000);
	
	if ((numbytes = recv (sockfd, buf, 1000, 0)) == -1)
		perror ("recv");
	
	printf ("Rcd: %d\n", numbytes);
	return numbytes;
}

int main ()
{
	int sockfd, new_fd;
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr;
	socklen_t sin_size;
	char s [INET6_ADDRSTRLEN], IP [INET6_ADDRSTRLEN];
	struct pollfd ufds [2];
	int rv;
	struct ifaddrs *ifaddr, *if_ptr;
	
	if (getifaddrs (&ifaddr) == -1)
	{
		perror ("getifaddrs");
		exit (0);
	}
	
	for (if_ptr = ifaddr; if_ptr != NULL; if_ptr = if_ptr -> ifa_next)
	{
		if (if_ptr -> ifa_addr == NULL)
			continue;
			
		printf ("Interface name: %s\n", if_ptr -> ifa_name);
		inet_ntop (AF_INET, get_in_addr ((struct sockaddr *) if_ptr -> ifa_addr), IP, sizeof IP);
		printf ("IP: %s\n", IP);
		
	}
	initialize (&hints);
	
	//Get server address
	if ((rv = getaddrinfo (NULL, PORT, &hints, &servinfo)) != 0)
	{
		fprintf (stderr, "getaddrinfo: %s\n", gai_strerror (rv));
		return 1;
	}
	
	p = connect_to_socket (&sockfd, servinfo);
	
	freeaddrinfo (servinfo);

	//Failed to listen
	if (listen (sockfd, BACKLOG) == -1)
	{
		perror ("listen");
		exit (1);
	}
	
	printf ("Server: waiting for connection: \n");

	while (1)
	{
		sin_size = sizeof their_addr;
        char buffer [1000], buff [1000];
        
		new_fd = accept (sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1)
		{
			perror ("accept");
			continue;
		}
		
		if (shutdown (new_fd, SHUT_RD) == -1) {
		    perror("Error shutting down socket");
		    close(new_fd);
		    close(sockfd);
		    exit(EXIT_FAILURE);
		}
		
		//Set polling variables
		ufds [0].fd = sockfd;
		ufds [0].events = POLLIN;
		ufds [0].revents = 0;
		
		ufds [1].fd = new_fd;
		ufds [1].events = POLLIN;
		ufds [1].revents = 0;
		
		//Start polling
		rv = poll (ufds, 2, -1);
		
		if (rv == -1) 
			perror ("poll");
		else if (rv == 0)
			printf ("Timeout occurred!\n");
		
		//Find server IP address
		inet_ntop (their_addr.ss_family, get_in_addr ((struct sockaddr *)&their_addr), s, sizeof s);
		printf ("Server: got connection from %s\n", s);
		
		//Read incoming message
		printf ("Can rcv msg: %d\n", ufds [1].revents & POLLIN);
		if (ufds [1].revents & POLLIN)
			receive_message (new_fd, buffer);
		printf ("Messsage: %s\n", buffer);
		
		//Find system time
		time_t current_time;
		time (&current_time);
		
		//Modify received message
		memset (buff, '\0', 1000);	
		sprintf (buff, "%sMessage: %s\n",ctime (&current_time), buffer);
		sprintf (buffer, "Server recieved time: %s", buff);
		
		//sleep (5);
		
		//Send message to client
		printf ("Buffer: %s\n", buffer);
		printf ("Can send msg: %d\n", ufds [1].revents & POLLOUT);
		if (ufds [1].revents & POLLOUT)
			send_message (new_fd, buffer);
		memset (buffer, '\0', 1000);
		
		close (new_fd);
	}
	close (sockfd);
	
	return 0;
}
