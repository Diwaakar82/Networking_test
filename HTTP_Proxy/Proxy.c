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

//Set socket variables
void set_socket_variables (struct sockaddr_in *sd, char port [])
{
	memset (sd, 0, sizeof (*sd));  
	  
   	sd -> sin_family = AF_INET;  
   	sd -> sin_port = htons (atoi (port));   
   	sd -> sin_addr.s_addr = INADDR_ANY;
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
       	printf ("server socket not created\n");
       	close (server_fd);
       	close (info -> client_fd);
       	exit (0);
    }
  	printf ("server socket created\n");
  	       
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
 	  
    // accept arguments from terminal  
    strcpy (ip, argv [1]); // server ip  
    strcpy (port, argv [2]);  // server port  
    strcpy (proxy_port, argv[3]); // proxy port  
    
    printf ("server IP : %s and port %s\n", ip, port);   
    printf ("proxy port is %s", proxy_port);        
    printf ("\n");  
  	//socket variables  
  	
  	int proxy_fd = 0, client_fd = 0;  
  	struct sockaddr_in proxy_sd;
  	  
	// Server exits when client exits  
	signal (SIGPIPE, SIG_IGN);
	 
  	// create a socket  
  	if ((proxy_fd = socket (AF_INET, SOCK_STREAM, 0)) < 0)  
  	{
      	printf ("\nFailed to create socket");
      	exit (0);
    }
  
  	printf ("Proxy created\n");  
  	memset (&proxy_sd, 0, sizeof (proxy_sd));
  	  
  	// set socket variables  
  	proxy_sd.sin_family = AF_INET;  
  	proxy_sd.sin_port = htons (atoi (proxy_port));  
  	proxy_sd.sin_addr.s_addr = INADDR_ANY;
  	  
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
 
  	printf ("waiting for connection..\n");
  	  
  	//accept all client connections continuously  
  	while (1)  
  	{  
       	client_fd = accept (proxy_fd, (struct sockaddr*)NULL, NULL);  
       	printf ("client no. %d connected\n", client_fd);
       	  
       	if (client_fd > 0)  
       	{  
        	//multithreading variables      
            struct serverInfo *item = malloc (sizeof (struct serverInfo));  
            item -> client_fd = client_fd;  
            strcpy (item -> ip, ip);  
            strcpy (item -> port, port);  
            pthread_create (&tid, NULL, runSocket, (void *) item);  
            sleep (1);  
       	}  
  	}  
  	return 0;  
}  

