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
#define BACKLOG 10			//Pending connections queue can hold

void sigchld_handler (int s)
{
	int saved_errno = errno;
	while (waitpid (-1, NULL, WNOHANG) > 0);
	
	errno = saved_errno;
}

void *get_in_addr (struct sockaddr *sa)
{
	if (sa -> sa_family == AF_INET)
		return &(((struct sockaddr_in *) sa) -> sin_addr);
		
	return &(((struct sockaddr_in6 *) sa) -> sin6_addr);
}

int main ()
{
	int sockfd, new_fd;
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr;
	socklen_t sin_size;
	struct sigaction sa;
	int yes = 1;
	char s [INET6_ADDRSTRLEN];
	int rv;
	
	memset (&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	
	if ((rv = getaddrinfo (NULL, PORT, &hints, &servinfo)) != 0)
	{
		fprintf (stderr, "getaddrinfo: %s\n", gai_strerror (rv));
		return 1;
	}
	
	for (p = servinfo; p != NULL; p = p -> ai_next)
	{
		if ((sockfd = socket (p -> ai_family, p -> ai_socktype, p -> ai_protocol)) == -1)
		{
			perror ("server: socket");
			continue;
		}
		if (setsockopt (sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof (int)) == -1)
		{
			perror ("setsockopt");
			exit (1);
		}
		
		int val = SOF_TIMESTAMPING_RAW_HARDWARE;
		if (setsockopt (sockfd, SOL_SOCKET, SO_TIMESTAMPING, &val, sizeof (int)) == -1)
		{
			perror ("setsockopt timestamp");
			exit (1);
		}
		if (bind (sockfd, p -> ai_addr, p -> ai_addrlen) == -1)
		{
			close (sockfd);
			perror ("server: bind");
			continue;
		}
		
		break;
	}
	
	freeaddrinfo (servinfo);
	
	if (p == NULL)
	{
		fprintf (stderr, "server: failed to bind\n");
		exit (0);
	}
	
	if (listen (sockfd, BACKLOG) == -1)
	{
		perror ("listen");
		exit (1);
	}
	
	sa.sa_handler = sigchld_handler;
	sigemptyset (&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	
	if (sigaction (SIGCHLD, &sa, NULL) == -1)
	{
		perror ("sigaction");
		exit (1);
	}
	
	printf ("Server: waiting for connection: \n");
	
	while (1)
	{
		sin_size = sizeof their_addr;
		
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
		
		if (!fork ())
		{
			close (sockfd);
			
			int numbytes;
			char buffer [1000];
    		char buff[1000];
    		
			if ((len = recv (new_fd, buffer, 1000, 0)) == -1)
				perror ("recv");
			
			cmsg = CMSG_FIRSTHDR(&msg);
			struct timespec *ts = (struct timespec *)CMSG_DATA(cmsg);       
	        timespec_get (ts, TIME_UTC);
	        
			strftime(buff, sizeof buff, "%D %T", gmtime(&ts -> tv_sec));

			
			sprintf (buff, "%s\nMessage: %s\n",buff, buffer);
			sprintf (buffer, "Server recieved time: %s", buff);
			sleep (5);
			
			if (send (new_fd, buffer, sizeof (buffer), 0) == -1)
			{
				perror ("send");
				exit (0);
			}

			close (new_fd);
			exit (0);
		}
		
		close (new_fd);
	}
	close (sockfd);
	
	return 0;
}
