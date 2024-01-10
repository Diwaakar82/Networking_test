#include <sys/socket.h>  
#include <sys/types.h>  
#include <resolv.h>  
#include <string.h>  
#include <stdlib.h>  
#include <pthread.h>  
#include <unistd.h>  
#include <netdb.h> //hostent  
#include <arpa/inet.h>  

struct serverInfo  
{  
  	int client_fd;  
  	char ip [100];  
  	char port [100];  
};  

void error (const char *msg) 
{
    perror (msg);
    exit (0);
}

//Set socket variables
void set_socket_variables (struct sockaddr_in *sd, char port [])
{
	memset (sd, 0, sizeof (*sd));  
	  
   	sd -> sin_family = AF_INET;  
   	sd -> sin_port = htons (atoi (port));   
   	sd -> sin_addr.s_addr = INADDR_ANY;
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

//Fetch internet address
void *get_in_addr (struct sockaddr *sa)
{
	if (sa -> sa_family == AF_INET)
		return &(((struct sockaddr_in *) sa) -> sin_addr);
		
	return &(((struct sockaddr_in6 *) sa) -> sin6_addr);
}

//Read and send data from one point to another	
void get_data (int source_fd, int dest_fd)
{
	char buffer [65535];
	int bytes = 0;
	
	memset (&buffer, '\0', sizeof (buffer));  
   	bytes = read (source_fd, buffer, sizeof (buffer));
   	  
   	if (bytes > 0)  
   	{	
        write (dest_fd, buffer, sizeof (buffer));                     
        printf ("From client :\n");                    
        fputs (buffer, stdout);         
   	}
}

// A thread for each client request  
void *runSocket (void *vargp)  
{  
	struct serverInfo *info = (struct serverInfo *)vargp;  
	char buffer [65535];  
	int bytes = 0;
	
  	printf ("client:%d\n", info -> client_fd);  
  	fputs (info -> ip, stdout);  
  	fputs (info -> port, stdout);  
  	
  	//code to connect to main server via this proxy server  
  	int server_fd = 0;  
  	struct sockaddr_in server_sd;  
  	
  	// create a socket  
  	server_fd = socket (AF_INET, SOCK_STREAM, 0);  
  	if (server_fd < 0)   
  	{
       	printf ("\nserver socket not created\n");
       	close (server_fd);
       	close (info -> client_fd);
       	exit (0);
    }
  	printf ("\nserver socket created\n");
  	       
  	set_socket_variables (&server_sd, info -> port);
  	  
  	//connect to main server from this proxy server  
  	if ((connect (server_fd, (struct sockaddr *)&server_sd, sizeof (server_sd))) < 0)   
  	{
       	printf ("server connection not established\n");
       	close (server_fd);
	   	close (info -> client_fd);
	   	exit (0);
   	}
   	
  	printf ("server socket connected\n");
  	  
  	while (1)  
  	{  
      	//receive data from client
      	get_data (info -> client_fd, server_fd);
      	fflush (stdout);
      	
      	get_data (server_fd, info -> client_fd);      	
       	printf ("\n");
  	};    
	return NULL;  
} 

// main entry point  
int main (int argc, char *argv [])  
{  
 	pthread_t tid;
 	char port [100], ip [100];  
 	char *hostname = argv [1];  
 	char proxy_port [100];
 	int n;
 	
    // accept arguments from terminal  
    strcpy (ip, argv [1]); // server ip  
    strcpy (port, argv [2]);  // server port  
    strcpy (proxy_port, argv[3]); // proxy port  
    
    printf ("server IP : %s and port %s\n", ip, port);   
    printf ("proxy port is %s", proxy_port);        
    printf ("\n");  
  	//socket variables  
  	
  	int proxy_fd = 0, client_fd = 0;
  	char ip_mask [100];
  	struct sockaddr_in proxy_sd, target_addr;
  	struct hostent *proxy_server;
  	  
	// Server exits when client exits  
	signal (SIGPIPE, SIG_IGN);
	 
  	// create a socket  
  	if ((proxy_fd = socket (AF_INET, SOCK_STREAM, 0)) < 0)  
  	{
      	printf ("\nFailed to create socket");
      	exit (0);
    }
  	printf ("Proxy created\n"); 
  	
  	set_socket_variables (&proxy_sd, proxy_port);
/*  	proxy_server = gethostbyname ("127.0.0.1");*/
/*  	bcopy ((char *)proxy_server -> h_addr, (char *)&proxy_sd.sin_addr.s_addr, proxy_server -> h_length);*/
  	 
  	// bind the socket  
  	if((bind (proxy_fd, (struct sockaddr*)&proxy_sd, sizeof (proxy_sd))) < 0)
  	{ 
       	printf ("Failed to bind a socket"); 
       	close (proxy_fd); 
       	exit (0);
    }
  
  	// start listening to the port for new connections  
  	if ((listen (proxy_fd, SOMAXCONN)) < 0)
  	{    
       	printf ("Failed to listen"); 
       	close (proxy_fd);
     	exit (0);  	
    } 
    
    //Uncomment for ip filtering
    
    //printf ("Enter the IP mask: ");
    //scanf ("%s", ip_mask);
    
  	printf ("waiting for connection..\n");

  	//accept all client connections continuously  
  	while (1)  
  	{  
  		struct sockaddr_storage client_addr;
  		socklen_t sin_size = sizeof (client_addr);
  		char ip_addr [100];
  		
  		//Find client ip address
  		
  		//Uncomment for IP filtering
  		
       	if ((client_fd = accept (proxy_fd, (struct sockaddr *)&client_addr, &sin_size)) < 0)
       	{
            perror ("Error on accept");
            exit (0);
        }
       	/*inet_ntop (client_addr.ss_family, get_in_addr ((struct sockaddr *)&client_addr), ip_addr, sizeof (ip_addr));
       	
       	
       	if (!is_ip_in_subnet (ip_addr, ip_mask))
       	{
       		printf ("Blocked IP address: %s\n", ip_addr);
       		close (client_fd);
       		continue;
       	}*/
        
       	printf ("client no. %d connected\n", client_fd);
       	char buffer [1000];
       	
       	bzero (buffer, sizeof(buffer));
        n = read (client_fd, buffer, sizeof(buffer) - 1);
        if (n < 0)
            error("Error reading from client");

        // Extract the target host and port from the CONNECT request
        char target_host [256];
        int target_port, target_sock;
        sscanf (buffer, "CONNECT %s:%d HTTP/1.1", target_host, &target_port);
 		
        // Connect to the target server
        target_sock = socket (AF_INET, SOCK_STREAM, 0);
        if (target_sock < 0)
            error("Error opening socket to target server");

        // Get the target server information
        int i = 0;
        while (target_host [i++] != ':');
        target_host [i - 1] = '\0';
        
        struct hostent *target_server = gethostbyname (target_host);
        printf ("Target host: %s\n", target_host);
        
        if (!target_server)
        {
        	printf ("@");
            error("Error getting target server address");
        }

		printf ("#");
        // Configure the target address structure
        bzero ((char *)&target_addr, sizeof (target_addr));
        target_addr.sin_family = AF_INET;
        target_addr.sin_port = htons (target_port);
        bcopy ((char *)target_server -> h_addr, (char *)&target_addr.sin_addr.s_addr, target_server -> h_length);

        // Connect to the target server
        if (connect(target_sock, (struct sockaddr *)&target_addr, sizeof(target_addr)) < 0)
            error("Error connecting to target server");

        // Send a success response to the client
        const char *success_response = "HTTP/1.1 200 Connection established\r\n\r\n";
        printf ("Response: %s\n", success_response);
        write (client_fd, success_response, strlen (success_response));

        // Forward data between the client and the target
        fd_set read_fds;
        int max_fd = (client_fd > target_sock) ? client_fd : target_sock;

        while (1) {
            FD_ZERO (&read_fds);
            FD_SET (client_fd, &read_fds);
            FD_SET (target_sock, &read_fds);

            select (max_fd + 1, &read_fds, NULL, NULL, NULL);

            if (FD_ISSET(client_fd, &read_fds)) 
            {
                n = read (client_fd, buffer, sizeof(buffer));
                if (n <= 0)
                    break;

                write (target_sock, buffer, n);
            }

            if (FD_ISSET (target_sock, &read_fds)) 
            {
                n = read (target_sock, buffer, sizeof(buffer));
                if (n <= 0)
                    break;

                write (client_fd, buffer, n);
            }
        }

        // Close the sockets
        close(client_fd);
        close(target_sock);
    }

    // Close the proxy server socket (not reached in this example)
    close(proxy_fd);
       	
       	
       	//Old Code
/*       	if (client_fd > 0)  */
/*       	{  */
/*        	//multithreading variables      */
/*            struct serverInfo *item = malloc (sizeof (struct serverInfo));  */
/*            item -> client_fd = client_fd;*/
/*              */
/*            strcpy (item -> ip, ip);  */
/*            strcpy (item -> port, port);*/
/*              */
/*            pthread_create (&tid, NULL, runSocket, (void *) item);  */
/*            sleep (1);  */
/*       	}  */  
  	return 0;  
}  

