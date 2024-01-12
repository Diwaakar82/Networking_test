#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

#define PROXY_PORT 5020


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


// A thread for each client request  
void *runSocket (int client_fd, int server_fd)  
{  
  	  
  	while (1)  
  	{  
      	//receive data from client
      	printf ("From client :\n");
      	get_data (client_fd, server_fd);
      	
      	printf ("From server :\n");
      	get_data (server_fd, client_fd);      	
       	printf ("\n");
  	};    
	return NULL;  
}

void handle_client(int client_socket) 
{
    // Receive the client's request
    char buffer [4096];
    ssize_t bytes_received = recv (client_socket, buffer, sizeof (buffer), 0);

    if (bytes_received <= 0) 
    {
        perror ("recv");
        close (client_socket);
        exit (EXIT_FAILURE);
    }

    // Extract the method and host from the request
    char method [16];
    char host [256];
    sscanf (buffer, "%s %s", method, host);

    if (strcmp (method, "CONNECT") == 0) 
    {
        // Handling CONNECT method
        char *port_str = strchr (host, ':');
        if (port_str != NULL) 
        {
            *port_str = '\0';
            int port = atoi (port_str + 1);

            // Create a socket to connect to the destination server
            int destination_socket = socket (AF_INET, SOCK_STREAM, 0);
            if (destination_socket == -1) 
            {
                perror ("socket");
                close (client_socket);
                exit (EXIT_FAILURE);
            }
			
            // Set up the address structure for the destination server
            struct sockaddr_in destination_addr;
            memset (&destination_addr, 0, sizeof (destination_addr));
            destination_addr.sin_family = AF_INET;
            destination_addr.sin_port = htons (port);
            inet_pton (AF_INET, host, &destination_addr.sin_addr);
            
            //binding to the port 
            
            struct sockaddr_in proxy_addr;
			memset (&proxy_addr, 0, sizeof (proxy_addr));
			proxy_addr.sin_family = AF_INET;
			proxy_addr.sin_port = htons (10000);
			proxy_addr.sin_addr.s_addr = INADDR_ANY;

			if (bind (destination_socket, (struct sockaddr *)&proxy_addr, sizeof (proxy_addr)) == -1) 
			{
				perror ("bind");
				close (destination_socket);
				exit (EXIT_FAILURE);
			}

            // Connect to the destination server
            if (connect (destination_socket, (struct sockaddr *)&destination_addr, sizeof (destination_addr)) == -1) 
            {
                perror ("connect");
                close (client_socket);
                close (destination_socket);
                exit (EXIT_FAILURE);
            }

            // Notify the client that the connection is established
            const char *response = "HTTP/1.1 200 Connection established\r\n\r\n";

            // Forward the data between client and destination
            char data_buffer [4096];
            ssize_t n;
            
            while (1)
            	if (client_socket > 0)
				    runSocket(client_socket,destination_socket); 
			
            // Clean up
            close (client_socket);
            close (destination_socket);
            exit (EXIT_SUCCESS);
        }
    }

    close (client_socket);
    exit (EXIT_SUCCESS);
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

void sigchld_handler (int s) 
{
    (void) s;
    while (waitpid (-1, NULL, WNOHANG) > 0);
}

int main () 
{
    int yes = 1, proxy_socket = socket (AF_INET, SOCK_STREAM, 0);
    char ip_mask [100];
    
    if (proxy_socket == -1) 
    {
        perror ("socket");
        exit (EXIT_FAILURE);
    }
	
	if (setsockopt (proxy_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
	{
		perror ("setsockopt");
		exit (1);	
	}
		
    struct sockaddr_in proxy_addr;
    memset (&proxy_addr, 0, sizeof (proxy_addr));
    proxy_addr.sin_family = AF_INET;
    proxy_addr.sin_port = htons (PROXY_PORT);
    proxy_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind (proxy_socket, (struct sockaddr *)&proxy_addr, sizeof (proxy_addr)) == -1) 
    {
        perror ("bind");
        close (proxy_socket);
        exit (EXIT_FAILURE);
    }

    if (listen (proxy_socket, 10) == -1) 
    {
        perror ("listen");
        close (proxy_socket);
        exit (EXIT_FAILURE);
    }

    // Set up a signal handler to reap zombie processes
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction (SIGCHLD, &sa, NULL);

	//IP filtering
	printf ("Enter the IP mask: ");
    scanf ("%s", ip_mask);
	
    printf ("Proxy server is running on port %d\n", PROXY_PORT);
	
    while (1) 
    {
        struct sockaddr_storage client_addr;
        socklen_t sin_size = sizeof (client_addr);
        int client_socket = accept (proxy_socket, (struct sockaddr *)&client_addr, &sin_size);
  		char ip_addr [100];
        
        if (client_socket == -1) 
        {
            perror ("accept");
            continue;
        }
		
		//Find client ip address
		inet_ntop (client_addr.ss_family, get_in_addr ((struct sockaddr *)&client_addr), ip_addr, sizeof (ip_addr));
		
       	if (!is_ip_in_subnet (ip_addr, ip_mask))
       	{
       		printf ("Blocked IP address: %s\n", ip_addr);
       		close (client_socket);
       		continue;
       	}
		
        pid_t pid = fork ();
        if (pid == -1) 
        {
            perror ("fork");
            close (client_socket);
            continue;
        }
        // In the child process (handle the client request) 
        else if (pid == 0) 
        {
            close (proxy_socket);
            handle_client (client_socket);
        }
        // In the parent process (continue accepting new connections)
        else
            close (client_socket);
    }

    close (proxy_socket);
    return 0;
}
