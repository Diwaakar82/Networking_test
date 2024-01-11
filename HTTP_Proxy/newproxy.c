#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

#define PROXY_PORT 5023


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

void sigchld_handler (int s) 
{
    (void) s;
    while (waitpid (-1, NULL, WNOHANG) > 0);
}

int main () 
{
    int proxy_socket = socket (AF_INET, SOCK_STREAM, 0);
    if (proxy_socket == -1) 
    {
        perror ("socket");
        exit (EXIT_FAILURE);
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

    printf ("Proxy server is running on port %d\n", PROXY_PORT);

    while (1) 
    {
        int client_socket = accept (proxy_socket, NULL, NULL);
        if (client_socket == -1) 
        {
            perror ("accept");
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
