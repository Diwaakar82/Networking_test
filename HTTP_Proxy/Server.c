#include <string.h>  
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h> 
#include <sys/poll.h>

#define SA struct sockaddr 
#define BACKLOG 10 
#define PORT "8050"

int flag = 0;

// defining HTTP header and send data to the client
void send_response (int client_socket, const char* status, const char* content_type, const char* content)
{
	char response [2048]; 
	sprintf (response, "HTTP/1.1 %s\r\nContent-Type: %s\r\n\r\n%s", status, content_type, content);
	send (client_socket, response, strlen (response), 0);
}

// retrieve all data from the file in the list form
void getalldata (int client_socket, char content [1024], char fileName [100])
{
	    // Open the file for reading
	    FILE *file = fopen (fileName, "r");
	    if (file == NULL) 
	    {
			perror ("Error opening file");
			send_response (client_socket, "500 Internal Server Error", "text/plain", "Error opening file");
			flag = 1;
			return;
	    }

	    //get data from file
	    char line [100];
	    int len = 0;
	    while (fgets(line,100,file))
	    {
			char line2[100];
			sprintf(line2,"<li>\n%s</li>",line);
			strcat(content,line2);
	    }
	    content [strlen (content)] = '\0';
	    fclose (file);
}

//handling get request without query parameters
void handle_get_request (int client_socket, char fileName [100]) 
{
    char buff [1024];
	
    // get all data from the specified file
    getalldata (client_socket, buff, fileName); 
    if(flag)
		return;
    
    //create html content for displaying data
    char html_content [1024];
    sprintf (html_content, "<html>\n<body>\n<ul>\n%s</ul>\n</body>\n</html>", buff);
  
    send_response (client_socket, "200 OK", "text/html", html_content);
}

// give IPV4 or IPV6 port number based on the family set in the sa
int get_in_port (struct sockaddr *sa)
{
	if (sa -> sa_family == AF_INET)
		return (((struct sockaddr_in*)sa) -> sin_port);	
	return (((struct sockaddr_in6*)sa) -> sin6_port);
}

// give IPV4 or IPV6  based on the family set in the sa
void *get_in_addr (struct sockaddr *sa)
{
	if (sa -> sa_family == AF_INET)
		return &(((struct sockaddr_in*)sa) -> sin_addr);	
	return &(((struct sockaddr_in6*)sa) -> sin6_addr);
}

//Create server
int server_creation()
{
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int yes = 1;
	int rv;
	
	memset (&hints,0,sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;// my ip
	
	// set the address of the server with the port info.
	if ((rv = getaddrinfo (NULL, PORT, &hints, &servinfo)) != 0)
	{
		fprintf (stderr, "getaddrinfo: %s\n", gai_strerror (rv));	
		return 1;
	}
	
	// loop through all the results and bind to the socket in the first we can
	for (p = servinfo; p != NULL; p = p -> ai_next)
	{
		sockfd = socket (p -> ai_family, p -> ai_socktype, p -> ai_protocol);
		if (sockfd == -1)
		{ 
			perror ("server: socket\n"); 
			continue; 
		} 
		
		//Reuse sockets
		if (setsockopt (sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
		{
			perror ("setsockopt");
			exit (1);	
		}
		
		// it will help us to bind to the port.
		if (bind (sockfd, p -> ai_addr, p -> ai_addrlen) == -1) 
		{
			close (sockfd);
			perror ("server: bind");
			continue;
		}
	}

	// server will be listening with maximum simultaneos connections of BACKLOG
	if (listen (sockfd, BACKLOG) == -1)
	{ 
		perror ("listen");
		exit (1); 
	} 
	return sockfd;
}

//connection establishment with the client
int connection_accepting (int sockfd, struct pollfd **poll_fds, int *max_fds, int *num_fds)
{
	int connfd;
	struct sockaddr_storage their_addr;
	char s [INET6_ADDRSTRLEN];
	socklen_t sin_size;
	
	sin_size = sizeof (their_addr); 
	connfd = accept (sockfd, (SA*)&their_addr, &sin_size);
	
	if(connfd == -1)
	{ 
		perror ("\naccept error\n");
		return -1;
	} 
	
	if (*num_fds == *max_fds) 
	{
        *poll_fds = realloc (*poll_fds, (*max_fds + 5) * sizeof (struct pollfd));
        if (*poll_fds == NULL) 
        {
            perror ("realloc");
            exit (1);
        }

        *max_fds += 5;
    }
	
	//printing the client name
	inet_ntop (their_addr.ss_family, get_in_addr ((struct sockaddr *)&their_addr), s, sizeof (s));
	
	//Block connections not coming from the proxy
	if (strcmp ("::ffff:127.0.0.1", s) || ntohs(get_in_port ((struct sockaddr *)&their_addr)) != 10000)
	{
		printf ("Connect via proxy!!\n");
		close (connfd);
		return -1;
	}

	(*num_fds)++;

    ((*poll_fds) + *num_fds - 1) -> fd = connfd;
    ((*poll_fds) + *num_fds - 1) -> events = POLLIN;
    ((*poll_fds) + *num_fds - 1) -> revents = 0;
	
	printf ("\nserver: got connection from %s\n", s);
	return connfd;
}

void cleanup (struct pollfd *poll_fds)
{
	close (poll_fds -> fd);
	poll_fds -> fd *= -1;
}

//simple webserver with support to http methods such as get as well as post (basic functionalities)
void simple_webserver (struct pollfd *poll_fds)
{
	int c = 0;
	int connfd = poll_fds -> fd;
	char buff [2048];
	char method [10];// to store the method name
	
	// default route to be parsed
	char fileName [100] = "output.txt";
	char route [100];//route data
	
	// receiving the message from the client either get request or post request
	c = recv (connfd, buff, sizeof (buff), 0);
	if (c == 0)
	{
		printf ("CLient closed\n");
		cleanup (poll_fds);
		return;
	}
	if (c < 0)
	{
		printf ("msg not received\n"); 
		exit (0); 
	}
	buff [c] = '\0';

	printf ("%s", buff);
	sscanf (buff, "%s /%s", method, route);
	
	//GET without query parameters		
	if (strcmp (method, "GET") == 0)
		handle_get_request (connfd, fileName);
	else
		send_response (connfd, "501 Not Implemented", "text/plain", "Method Not Implemented");
}

int main()
{ 
	int sockfd,connfd; 
	struct pollfd *poll_fds;
	int max_fds = 0, num_fds = 0, nfds;
	
	//server creation
	sockfd = server_creation ();
	
	if ((poll_fds = malloc (5 * sizeof (struct pollfd))) == NULL)
	{
		perror ("malloc");
		exit (0);
	}
	max_fds = 5;
	
	poll_fds -> fd = sockfd;
	poll_fds -> events = POLLIN;
	poll_fds -> revents = 0;
	num_fds = 1;
	
	printf("server: waiting for connections...\n");
	while (1)
	{ 
		nfds = num_fds;
		if (poll (poll_fds, nfds, -1) == -1)
		{
			perror ("poll");
			exit (0);
		}
		
		for (int fd = 0; fd < nfds; fd++)
		{
			if ((poll_fds + fd) -> fd <= 0)
				continue;
				
			if (((poll_fds + fd) -> revents & POLLIN) == POLLIN)
			{
				if ((poll_fds + fd) -> fd == sockfd)
					connection_accepting (sockfd, &poll_fds, &max_fds, &num_fds);
				else
					simple_webserver (poll_fds + fd);
			}
		}	 
	} 
	close (sockfd); 
	return 0;
} 

