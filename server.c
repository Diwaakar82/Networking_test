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
#include <time.h>
#include <linux/net_tstamp.h>

#define PORT "8000"			//Port used for connections
#define BACKLOG 1			//Pending connections queue can hold

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
	    printf("ai_flags: %d\n", p -> ai_flags);
		printf("ai_family: %d\n", p -> ai_family);
		printf("ai_socktype: %d\n", p -> ai_socktype);
		printf("ai_protocol: %d\n", p -> ai_protocol);
		printf("ai_addrlen: %zu\n", p -> ai_addrlen);
		printf("ai_addr: %p\n", (void*)p -> ai_addr);
		printf("ai_canonname: %s\n", p -> ai_canonname);
		printf("ai_next: %p\n", (void*)p -> ai_next);
	    
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
		
		int val = SOF_TIMESTAMPING_RAW_HARDWARE;
		if (setsockopt (*sockfd, SOL_SOCKET, SO_TIMESTAMPING, &val, sizeof (int)) == -1)
		{
			perror ("setsockopt timestamp");
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
	memset (hints, 0, sizeof hints);
	hints -> ai_family = AF_UNSPEC;
	hints -> ai_socktype = SOCK_STREAM;
	hints -> ai_flags = AI_PASSIVE;
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
	if ((numbytes = recv (sockfd, buf, 1000, 0)) == -1)
		perror ("recv");
	
	return numbytes;
}


//Find current time
void find_time (struct cmsghdr *cmsg, struct msghdr msg, char buff [])
{
	cmsg = CMSG_FIRSTHDR(&msg);
	struct timespec *ts = (struct timespec *)CMSG_DATA(cmsg);       
    timespec_get (ts, TIME_UTC);
    
	strftime(buff, 1000, "%D %T", gmtime(&ts -> tv_sec));
}

int main ()
{
	int sockfd, new_fd;
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr;
	socklen_t sin_size;
	char s [INET6_ADDRSTRLEN];
	int rv;
	
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
	
 	/*	kill_dead_processes ();*/
	
	printf ("Server: waiting for connection: \n");
	
	while (1)
	{
		sin_size = sizeof their_addr;
		
		//Initialize hardware timestamping variables
		char data[4096], ctrl[4096];
		struct msghdr msg;
		struct cmsghdr *cmsg;
		struct iovec iov;
		ssize_t len;
		
		memset(&msg, 0, sizeof(msg));
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = ctrl;
        msg.msg_controllen = sizeof(ctrl);
      	iov.iov_base = data;
        iov.iov_len = sizeof(data);
        
		new_fd = accept (sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1)
		{
			perror ("accept");
			continue;
		}
		
		inet_ntop (their_addr.ss_family, get_in_addr ((struct sockaddr *)&their_addr), s, sizeof s);
		printf ("Server: got connection from %s\n", s);
			
		int numbytes;
		char buffer [1000], buff [1000];
		
		receive_message (new_fd, buffer);
		
		printf ("Messsage: %s\n", buffer);
		scanf ("%d", &numbytes);
		find_time (cmsg, msg, buff);
		
		sprintf (buff, "%s\nMessage: %s\n",buff, buffer);
		sprintf (buffer, "Server recieved time: %s", buff);
/*			sleep (5);*/
		
		send_message (new_fd, buff);
		
		close (new_fd);
	}
	close (sockfd);
	
	return 0;
}
