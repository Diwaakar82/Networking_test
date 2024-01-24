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
#include <openssl/ssl.h>
#include <openssl/err.h>

#define PROXY_PORT "5020"
#define SA struct sockaddr 

SSL_CTX *ssl_server_ctx;
SSL_CTX *ssl_client_ctx;	

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

// Forward the data between client and destination
void message_handler (int client_socket, int destination_socket, char data_buffer [], SSL *ssl_server, SSL *ssl_client)
{
	ssize_t n;

	n = SSL_read (ssl_server, data_buffer, sizeof (data_buffer));
	if (n <= 0)
		return;

	data_buffer [n] = '\0';

	SSL_write (ssl_client, data_buffer, 1024);
	
	while ((n = SSL_read (ssl_client, data_buffer, 1024)) > 0)
		SSL_write (ssl_server, data_buffer, n);
}

void message_handler_http(int client_socket,int destination_socket,char data[])
{
	// Forward the data between client and destination sockets
	ssize_t n;
	n = write (destination_socket, data, 2048);
	
	while ((n = recv (destination_socket, data, 2048, 0)) > 0)
		send (client_socket, data, n, 0);
}

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

    if (strcmp (method, "CONNECT") == 0) 
    {
        // Handling CONNECT method
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
		
		SSL *ssl_client = SSL_new (ssl_client_ctx);
		SSL_set_fd (ssl_client, destination_socket);
		
		if (SSL_connect (ssl_client) <= 0) 
		{
			ERR_print_errors_fp (stderr);
			close (client_socket);
			close (destination_socket);
			return;
		}
			
		SSL *ssl_server = SSL_new (ssl_server_ctx);
		SSL_set_fd (ssl_server, client_socket);
		
		if (SSL_accept (ssl_server) <= 0)
		{
			ERR_print_errors_fp (stderr);
			SSL_shutdown(ssl_client);
			SSL_free (ssl_client);
			close (client_socket);
			close (destination_socket);
			return;
		}		
	
        message_handler (client_socket, destination_socket, data_buffer, ssl_server, ssl_client);
			
        // Clean up
		close (destination_socket);
        close (client_socket);
        SSL_shutdown(ssl_server);
		SSL_shutdown(ssl_client);
		SSL_free (ssl_server);
		SSL_free (ssl_client);
	}
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
	
	return connfd;
}

int main () 
{
    int yes = 1, proxy_socket, client_socket;
    char ip_mask [100];
    
    int dummy;
    scanf ("%d", &dummy);
    
    SSL_library_init ();
    SSL_load_error_strings ();
    
    ssl_server_ctx = SSL_CTX_new (SSLv23_server_method ());
	ssl_client_ctx = SSL_CTX_new (SSLv23_client_method ());
    
    if (!ssl_server_ctx) 
    {
        perror ("Error creating SSL context");
        exit (EXIT_FAILURE);
    }
    
    if (!ssl_client_ctx) 
    {
        perror ("Error creating SSL context");
        exit (EXIT_FAILURE);
    }

    // Load the server certificate and private key
    if (SSL_CTX_use_certificate_file (ssl_server_ctx, "server.crt", SSL_FILETYPE_PEM) <= 0) 
    {
        ERR_print_errors_fp (stderr);
        exit (EXIT_FAILURE);
    }

    if (SSL_CTX_use_PrivateKey_file (ssl_server_ctx, "server.key", SSL_FILETYPE_PEM) <= 0) 
    {
        ERR_print_errors_fp (stderr);
        exit (EXIT_FAILURE);
    }
    
    proxy_socket = server_creation ();

	//IP filtering
	//printf ("Enter the IP mask: ");
    //scanf ("%s", ip_mask);
	
    printf ("Proxy server is running on port %s\n", PROXY_PORT);
	
    while (1) 
    {	
		client_socket = connection_accepting (proxy_socket);
		if (client_socket == -1)
			continue;
		//////IP filtering
		
       	/*if (!is_ip_in_subnet (ip_addr, ip_mask))
       	{
       		printf ("Blocked IP address: %s\n", ip_addr);
       		close (client_socket);
       		continue;
       	}*/
       	
       	//////IP Filtering above
       	
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

	SSL_CTX_free (ssl_server_ctx);
	SSL_CTX_free (ssl_client_ctx);
	close (proxy_socket);
    return 0;
}
