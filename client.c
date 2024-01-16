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
#include <time.h>

#define PORT "8000"			//Port used for connections
#define MAXDATASIZE 1000

//Return the IPv4 or IPv6 address
void *get_in_addr (struct sockaddr *sa)
{
	if (sa -> sa_family == AF_INET)
		return &(((struct sockaddr_in *)sa) -> sin_addr);
		
	return &(((struct sockaddr_in6 *)sa) -> sin6_addr);
}

//Connect to a socket
struct addrinfo *connect_to_socket (int *sockfd, struct addrinfo *servinfo)
{
	struct addrinfo *p;
	
	for (p = servinfo; p != NULL; p = p -> ai_next)
	{
		//Socket not free
		if ((*sockfd = socket (p -> ai_family, p -> ai_socktype, p -> ai_protocol)) == -1)
		{
			perror ("client: socket");
			continue;
		}
/*		*/
/*		//Set option for hardware timestampping*/
/*		int val = SOF_TIMESTAMPING_RAW_HARDWARE;*/
/*		if (setsockopt (*sockfd, SOL_SOCKET, SO_TIMESTAMPING, &val, sizeof (int)) == -1)*/
/*		{*/
/*			perror ("setsockopt timestamp");*/
/*			exit (1);*/
/*		}*/
		
		//Connect to socket
		if (connect (*sockfd, p -> ai_addr, p -> ai_addrlen) == -1)
		{
			close (*sockfd);
			perror ("client: connect");
			continue;
		}
		break;
	}
	
	return p;
}

//Initialize address information
void initialize (struct addrinfo *hints)
{
	memset (hints, 0, sizeof *hints);
	
	hints -> ai_family = AF_UNSPEC;
	hints -> ai_socktype = SOCK_STREAM;
	hints -> ai_flags = AI_PASSIVE;
}

//Send message
void send_message (int sockfd, char *msg)
{
	if (send (sockfd, msg, sizeof (msg), 0) == -1)
	{
		perror ("send");
		exit (0);
	}
}

//Receive message
int receive_message (int sockfd, char *buf)
{
	int numbytes;
	if ((numbytes = recv (sockfd, buf, 1000, 0)) == -1)
	{
		perror ("recv");
		exit (0);
	}
	
	return numbytes;
}

/*//Find current time*/
/*void find_time (struct cmsghdr *cmsg, struct msghdr msg, char buff [])*/
/*{*/
/*	cmsg = CMSG_FIRSTHDR(&msg);*/
/*	struct timespec *ts = (struct timespec *)CMSG_DATA(cmsg);       */
/*    timespec_get (ts, TIME_UTC);*/
/*    */
/*	strftime(buff, 1000, "%D %T", gmtime(&ts -> tv_sec));*/
/*}*/

int main (int argc, char *argv [])
{
	int sockfd, numbytes;
	char buf [MAXDATASIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s [INET6_ADDRSTRLEN];
	
	//Hostname not provided
	if (argc != 2)
	{
		fprintf (stderr, "usage: client hostname\n");
		exit (1);
	}
	
	initialize (&hints);
	
	//Get server address
	if ((rv = getaddrinfo (argv [1], PORT, &hints, &servinfo)) != 0)
	{
		fprintf (stderr, "getaddrinfo: %s\n", gai_strerror (rv));
		return 1;
	}
	
	p = connect_to_socket (&sockfd, servinfo);
	
	if (p == NULL)
	{
		fprintf (stderr, "client: failed to connect\n");
		return 2;
	}
	
	inet_ntop (p -> ai_family, get_in_addr ((struct sockaddr *) p -> ai_addr), s, sizeof s);
	
	printf ("client: connecting to %s\n", s);
	
	//Server information not required anymore
	freeaddrinfo (servinfo);
	
	send_message (sockfd, "Hi");
	
	numbytes = receive_message (sockfd, buf);
	
	//Find system time
	time_t current_time;
	time (&current_time);

	printf ("Client received Time: %s%s\n", ctime (&current_time), buf);
	close (sockfd);
	
	return 0;
}
