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
#include <linux/net_tstamp.h>

#define PORT "8000"			//Port used for connections
#define MAXDATASIZE 1000

void *get_in_addr (struct sockaddr *sa)
{
	if (sa -> sa_family == AF_INET)
		return &(((struct sockaddr_in *)sa) -> sin_addr);
		
	return &(((struct sockaddr_in6 *)sa) -> sin6_addr);
}

int main (int argc, char *argv [])
{
	int sockfd, numbytes;
	char buf [MAXDATASIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s [INET6_ADDRSTRLEN];
	
	if (argc != 2)
	{
		fprintf (stderr, "usage: client hostname\n");
		exit (1);
	}
	
	memset (&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	
	if ((rv = getaddrinfo (argv [1], PORT, &hints, &servinfo)) != 0)
	{
		fprintf (stderr, "getaddrinfo: %s\n", gai_strerror (rv));
		return 1;
	}
	
	for (p = servinfo; p != NULL; p = p -> ai_next)
	{
		if ((sockfd = socket (p -> ai_family, p -> ai_socktype, p -> ai_protocol)) == -1)
		{
			perror ("client: socket");
			continue;
		}
		
		int val = SOF_TIMESTAMPING_RAW_HARDWARE;
		if (setsockopt (sockfd, SOL_SOCKET, SO_TIMESTAMPING, &val, sizeof (int)) == -1)
		{
			perror ("setsockopt timestamp");
			exit (1);
		}
		
		if (connect (sockfd, p -> ai_addr, p -> ai_addrlen) == -1)
		{
			close (sockfd);
			perror ("client: connect");
			continue;
		}
		
		break;
	}
	
	if (p == NULL)
	{
		fprintf (stderr, "client: failed to connect\n");
		return 2;
	}
	
	inet_ntop (p -> ai_family, get_in_addr ((struct sockaddr *) p -> ai_addr), s, sizeof s);
	
	printf ("client: connecting to %s\n", s);
	
	freeaddrinfo (servinfo);
	
	if (send (sockfd, "Hi", 4, 0) == -1)
	{
		perror ("send");
		exit (0);
	}
	
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
	
	
	
	if ((numbytes = recv (sockfd, buf, sizeof (buf), 0)) == -1)
	{
		perror ("recv");
		exit (0);
	}
	
	char buff[1000];
    		
	cmsg = CMSG_FIRSTHDR(&msg);
	struct timespec *ts = (struct timespec *)CMSG_DATA(cmsg);       
    timespec_get (ts, TIME_UTC);
    
	strftime(buff, sizeof buff, "%D %T", gmtime(&ts -> tv_sec));
	
	printf ("client recieved Time: %s\n%s\n", buff, buf);
	close (sockfd);
	
	return 0;
}
