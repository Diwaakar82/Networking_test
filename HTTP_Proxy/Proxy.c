#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/poll.h>
#include <netdb.h>

#define PROXY_PORT "5020"
#define SA struct sockaddr 
#define NUM_FDS 2

char ip_mask [100];

void get_data (int source_fd, int dest_fd)
{
	char buffer [2048];
	int bytes = 0;
	
	memset (&buffer, '\0', sizeof (buffer));  
   	bytes = recv (source_fd, buffer, sizeof (buffer), 0);
   	  
   	if (bytes > 0)  
   	{	
        send (dest_fd, buffer, sizeof (buffer), 0);                                  
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

// Function to convert an IP address string to a 32-bit unsigned integer
uint32_t ip_to_uint (char ip []) 
{
    uint32_t result = 0;
    unsigned char octets[4];

    if (sscanf(ip, "%hhu.%hhu.%hhu.%hhu", &octets[0], &octets[1], &octets[2], &octets[3]) == 4)
        result = (octets[0] << 24) | (octets[1] << 16) | (octets[2] << 8) | octets[3];

    return result;
}

// Function to check if an IP address is in a subnet
int is_ip_in_subnet(char ip_str [], char subnet_str []) 
{
    // Convert IP address and subnet mask to 32-bit unsigned integers
    uint32_t ip = ip_to_uint (ip_str);
    uint32_t subnet = ip_to_uint (subnet_str);
	
    // Perform bitwise AND operation
    uint32_t result = ip & subnet;
    
    // Check if the result is equal to the ip (indicating that the IP is in the subnet)
    return (result == ip);
}

void cleanup (struct pollfd *poll_fds)
{
	close (poll_fds -> fd);
	poll_fds -> fd *= -1;
}

int client_creation (char* port, char* destination_server_addr)
{
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	char s [INET6_ADDRSTRLEN];
	int rv;

	memset (&hints, 0, sizeof (hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	
	printf ("Dest: %s\nPort: %s\n", destination_server_addr, port);
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
		
		// it will help us to bind to the port.
		/*if (bind (sockfd, (SA*) &proxy_addr, sizeof (proxy_addr)) == -1) 
		{
			close (sockfd);
			perror ("client: bind");
			continue;
		}*/
		
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

// Forward the data between client and destination
void message_handler (int client_socket, int destination_socket, char data_buffer [])
{
	//char data_buffer[2048];
	ssize_t n;
	
	//while (1) 
	//{
	//n = recv (client_socket, data_buffer, sizeof (data_buffer), 0);
	//if (n <= 0)
	  //  break;

	//data_buffer [n]='\0';
	
	printf ("Data: %s\n", data_buffer);
	send (destination_socket, data_buffer, 1024, 0);
	
	while ((n = recv(destination_socket, data_buffer, 1024, 0)) > 0)
		send(client_socket, data_buffer, n, 0);
		
	
	//n = recv (destination_socket, data_buffer, sizeof (data_buffer), 0);
	//if (n <= 0)
	  //  break;

	//data_buffer [n] = '\0';
	//send (client_socket, data_buffer, 1024, 0);
	//}
}

void handle_client (struct pollfd *poll_fds) 
{
    // Receive the client's request
    char buffer [4096];
    int client_socket = poll_fds -> fd;
    ssize_t bytes_received = recv (client_socket, buffer, sizeof (buffer), 0);
	
    if (bytes_received <= 0) 
    {
        perror ("recv");
        cleanup (poll_fds);
        exit (EXIT_FAILURE);
    }
	buffer [bytes_received] = '\0';
	
    // Extract the method and host from the request
    char method [16];
    char data_buffer [4096];
    char host [256];
    
    strcpy (data_buffer, buffer);
    printf ("Buffer: %s\n", data_buffer);
    sscanf (buffer, "%s %s", method, host);
    printf ("Method: %s\nHost: %s", method, host);
	
    if (strcmp (method, "CONNECT") == 0) 
    {
        // Handling CONNECT method
        char *port_str = strchr (host, ':');
        if (port_str != NULL) 
        {
            *port_str = '\0';
            char *port = port_str + 1;

            // Create a socket to connect to the destination server
            int destination_socket = client_creation (port, host);
            if (destination_socket == -1) 
            {
                perror ("socket");
                cleanup (poll_fds);
                exit (EXIT_FAILURE);
            }

            // Notify the client that the connection is established
            //const char *response = "HTTP/1.1 200 Connection established\0";
			//send (client_socket, response, 37, 0);
			
            // Forward the data between client and destination
            ssize_t n;
            
            message_handler (client_socket, destination_socket, data_buffer);
			
            // Clean up
            close (destination_socket);
            cleanup (poll_fds);
        }
    }
    else
    {	
    	printf ("&");
    	char *host_start = strstr (buffer, "Host: ") + 6;
		char *host_end = strstr (host_start, "\r\n");
		*host_end = '\0';
		
		printf ("Host: %s\n", host_start);
		int destination_socket = client_creation ("80", host_start);
		if (destination_socket == -1) 
        {
            perror ("socket");
            cleanup (poll_fds);
            exit (EXIT_FAILURE);
        }
        
        message_handler (client_socket, destination_socket, data_buffer);
			
        // Clean up
        close (destination_socket);
        cleanup (poll_fds);
    }
}

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
	
	// server will be listening with maximum simultaneos connections of BACKLOG
	if (listen (sockfd, 10) == -1)
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
	
	if (connfd == -1)
	{ 
		perror ("\naccept error\n");
		return -1;
	} 
	
	if (*num_fds == *max_fds) 
	{
        *poll_fds = realloc (*poll_fds, (*max_fds + NUM_FDS) * sizeof (struct pollfd));
        if (*poll_fds == NULL) 
        {
            perror ("realloc");
            exit (1);
        }

        *max_fds += NUM_FDS;
    }

    ((*poll_fds) + *num_fds) -> fd = connfd;
    ((*poll_fds) + *num_fds) -> events = POLLIN;
    ((*poll_fds) + *num_fds) -> revents = 0;
	
	(*num_fds)++;
	
	//printing the client name
	inet_ntop (their_addr.ss_family, get_in_addr ((struct sockaddr *)&their_addr), s, sizeof (s));
	
	//////IP filtering
   	/*if (!is_ip_in_subnet (s + 7, ip_mask))
   	{
   		printf ("Blocked IP address: %s\n", s);
   		close (sockfd);
   		close (connfd);
   		
   		return;
   	}*/
   	
   	//////IP Filtering above
	
	printf ("\nserver: got connection from %s\n", s);
}

int main () 
{
    int yes = 1, proxy_socket;
    struct pollfd *poll_fds;
	int max_fds = 0, num_fds = 0, nfds;
    
    
    proxy_socket = server_creation ();
	
	if ((poll_fds = malloc (5 * sizeof (struct pollfd))) == NULL)
	{
		perror ("malloc");
		exit (0);
	}
	max_fds = 5;
	
	poll_fds -> fd = proxy_socket;
	poll_fds -> events = POLLIN;
	poll_fds -> revents = 0;
	num_fds = 1;
	
	//IP filtering
	printf ("Enter the IP mask: ");
    scanf ("%s", ip_mask);
	
    printf ("Proxy server is running on port %s\n", PROXY_PORT);
	
    while (1) 
    {
    	nfds = num_fds;
		if (poll (poll_fds, nfds, -1) == -1)
		{
			perror("poll");
			exit(1);
		}
		
		for (int fd = 0; fd < nfds; fd++)
		{
			if ((poll_fds + fd) -> fd <= 0)
				continue;
				
			if (((poll_fds + fd) -> revents & POLLIN) == POLLIN)
			{
				if ((poll_fds + fd) -> fd == proxy_socket)
					connection_accepting (proxy_socket, &poll_fds, &max_fds, &num_fds);
				else
					handle_client (poll_fds + fd);
			}
		}
    }

    close (proxy_socket);
    return 0;
}
