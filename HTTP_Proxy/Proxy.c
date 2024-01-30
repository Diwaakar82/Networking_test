#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netdb.h>
#include <poll.h>

#define PROXY_PORT "5020"
#define SA struct sockaddr 

//Read and write data from src to dest
void get_data (int source_fd, int dest_fd)
{
	char buffer [2048];
	int bytes = 0;
	
	memset (&buffer, '\0', sizeof (buffer));  
   	bytes = read (source_fd, buffer, sizeof (buffer));
   	  
   	if (bytes > 0)  
   	{	
        write (dest_fd, buffer, sizeof (buffer));                                  
        fputs (buffer, stdout);         
   	}
}

//Fetch internet address
void *get_in_addr (struct sockaddr *sa)
{
	if (sa -> sa_family == AF_INET)
		return &(((struct sockaddr_in *) sa) -> sin_addr);
		
	return &(((struct sockaddr_in6 *) sa) -> sin6_addr);
}

//Create client socket and connect to proxy
int client_creation (char* port, char* destination_server_addr)
{
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	char s [INET6_ADDRSTRLEN];
	int rv;

	memset (&hints, 0, sizeof (hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo (destination_server_addr, port, &hints, &servinfo)) != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n",gai_strerror(rv));	
		return -1;
	}
	
	struct sockaddr_in proxy_addr;
	memset (&proxy_addr, 0, sizeof (proxy_addr));
	proxy_addr.sin_family = AF_INET;
	proxy_addr.sin_port = htons (10000);
	proxy_addr.sin_addr.s_addr = INADDR_ANY;

	for (p = servinfo; p != NULL; p = p -> ai_next)
	{
		sockfd = socket (p -> ai_family, p -> ai_socktype, p -> ai_protocol);
		if (sockfd == -1)
		{ 
			perror ("client: socket\n"); 
			continue; 
		}
		
		int yes = 1;
		
		if (setsockopt (sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
		{
			perror ("setsockopt");
			exit (1);	
		}
		
		// connect will help us to connect to the server with the addr given in arguments.
		if (connect (sockfd, p -> ai_addr, p -> ai_addrlen) == -1) 
		{
			close(sockfd);
			perror("client: connect");
			continue;
		} 
		break;
	}

	if (p == NULL)
	{
		fprintf (stderr, "client: failed to connect\n");
		return -1;	
	}
	
	//printing ip address of the server.
	inet_ntop (p -> ai_family, get_in_addr ((struct sockaddr *)p -> ai_addr), s, sizeof (s));
	
	printf ("proxy client: connecting to %s\n", s);
	freeaddrinfo (servinfo);
	
	return sockfd;
}

// Forward the data between client and destination in an HTTPS connection
void message_handler (int client_socket, int destination_socket, char data_buffer [])
{
	struct pollfd pollfds [2];
    pollfds [0].fd = client_socket;
    pollfds [0].events = POLLIN;
    pollfds [0].revents = 0;
    pollfds [1].fd = destination_socket;
    pollfds [1].events = POLLIN;
    pollfds [1].revents = 0;
	ssize_t n;

	while (1)
	{
		if (poll (pollfds, 2, -1) == -1)
		{
			perror ("poll");
			exit (1);
		}
		
		
		for (int fd = 0; fd < 2; fd++)
		{
			//Message from client to server
			if (pollfds [fd].revents & POLLIN && !fd)
			{
				n = read (pollfds [0].fd, data_buffer, 1024);
				if (n <= 0)
					return;

				data_buffer [n] = '\0';

				write (pollfds [1].fd, data_buffer, n);
			}
			
			//Message from server to client
			if (pollfds [fd].revents & POLLIN && fd)
			{
				n = read (pollfds [1].fd, data_buffer, 1024);
				if (n <= 0)
					return;

				data_buffer [n] = '\0';

				write (pollfds [0].fd, data_buffer, n);
			}
		}
	}
}

// Forward the data between client and destination in an HTTP connection
void message_handler_http (int client_socket, int destination_socket, char data[])
{
	ssize_t n;
	n = write (destination_socket, data, 2048);
	
	while ((n = recv (destination_socket, data, 2048, 0)) > 0)
		send (client_socket, data, n, 0);
}

//Handle the request to be sent to the client
void handle_client (int client_socket) 
{
    // Receive the client's request
    char buffer [4096];
    
    //Receive connect method message
    int n = read (client_socket, buffer, sizeof (buffer));
    if (n <= 0)
    {
    	close (client_socket);
    	exit (0);
    }
    
    buffer [n] = '\0';
   	
    // Extract the method and host from the request
    char method [16];
    char host [256];
    char data_buffer [4096];
    
    strcpy (data_buffer, buffer);
    printf ("%s\n", data_buffer);
    sscanf (buffer, "%s %s", method, host);

	// Handling CONNECT method
    if (strcmp (method, "CONNECT") == 0) 
    {
        char *port_str = strchr (host, ':');
        char https_port [10] = "443";
        char *port;
        
        if (port_str != NULL) 
        {
            *port_str = '\0';
            port = port_str + 1;
        }
        else
        	port = https_port;

        // Create a socket to connect to the destination server
        int destination_socket = client_creation (port, host);
        if (destination_socket == -1) 
        {
            perror ("socket");
            close (client_socket);
            exit (EXIT_FAILURE);
        }
		
		const char *response = "HTTP/1.1 200 Connection established\r\n\r\n";
	    int r = write (client_socket, response, strlen (response));
	
        message_handler (client_socket, destination_socket, data_buffer);
	}
	//Handle http requests
    else
    {
		char *host_str = strstr (buffer, "Host: ") + 6;
		char *host_end = strstr (host_str, "\r\n");
		*host_end = '\0';
		
		char* port;
        char http_port [10] = "80";
        char* port_str = strchr (host_str, ':');
    	if (port_str != NULL) 
    	{
			*port_str = '\0';
			port = port_str + 1;
		}
    	else
    		port = http_port;
	 
		int destination_socket = client_creation (port, host_str);
		if (destination_socket == -1) 
	    {
	        perror ("socket");
	        close (client_socket);
	        exit (EXIT_FAILURE);
	    }
		
	    message_handler_http (client_socket, destination_socket, data_buffer);
			
	    // Clean up
		close (destination_socket);
	    close (client_socket);
    }
        
}

//Create a server and bind to socket and start listening for connections
int server_creation ()
{
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int yes = 1;
	int rv;
	
	memset (&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;// my ip
	
	// set the address of the server with the port info.
	if ((rv = getaddrinfo (NULL, PROXY_PORT, &hints, &servinfo)) != 0)
	{
		fprintf (stderr, "getaddrinfo: %s\n", gai_strerror(rv));	
		return 1;
	}
	
	// loop through all the results and bind to the socket in the first we can
	for (p = servinfo; p != NULL; p = p -> ai_next)
	{
		sockfd = socket (p -> ai_family, p -> ai_socktype, p -> ai_protocol);
		if (sockfd == -1)
		{ 
			perror("server: socket\n"); 
			continue; 
		} 
		
		//Reuse sockets
		if (setsockopt (sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof (int)) == -1)
		{
			perror("setsockopt");
			exit(1);	
		}
		
		// it will help us to bind to the port.
		if (bind (sockfd, p -> ai_addr, p -> ai_addrlen) == -1) 
		{
			close(sockfd);
			perror("server: bind");
			continue;
		}
	}
	
	/*if (p == NULL)
	{
		fprintf (stderr, "server: failed to bind\n"	);
		exit (0);	
	}*/
	
	// server will be listening with maximum simultaneos connections of BACKLOG
	if (listen (sockfd, 10) == -1)
	{ 
		perror ("listen");
		exit (1); 
	} 
	return sockfd;
}

//connection establishment with the client
int connection_accepting (int sockfd)
{
	int connfd;
	struct sockaddr_storage their_addr;
	char s [INET6_ADDRSTRLEN];
	socklen_t sin_size;
	
	sin_size = sizeof (their_addr); 
	connfd = accept (sockfd, (SA*)&their_addr, &sin_size);
	
	if (connfd == -1)
	{ 
		perror ("\naccept error\n");
		return -1;
	} 
	
	//printing the client name
	inet_ntop (their_addr.ss_family, get_in_addr ((struct sockaddr *)&their_addr), s, sizeof (s));
	printf ("\nserver: got connection from %s\n", s);
	
	return connfd;
}

int main () 
{
    int yes = 1, proxy_socket, client_socket;
    
    proxy_socket = server_creation ();
	
    printf ("Proxy server is running on port %s\n", PROXY_PORT);
	
    while (1) 
    {	
		client_socket = connection_accepting (proxy_socket);
		if (client_socket == -1)
			continue;
       	
        // In the child process (handle the client request) 
        if (!fork ())
        {
        	close (proxy_socket);	
        	
            handle_client (client_socket);
            close (client_socket);
			exit (0);
        }
        close (client_socket);
    }
    
	close (proxy_socket);
    return 0;
}
