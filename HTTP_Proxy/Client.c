#include <sys/socket.h>  
#include <sys/types.h>  
#include <resolv.h>  
#include <stdlib.h>
#include <string.h>  

#define TARGET_HOST "127.0.0.1"

//Set socket variables
void set_socket_variables (struct sockaddr_in *sd, char port [])
{
	memset (sd, 0, sizeof (*sd));  
	  
   	sd -> sin_family = AF_INET;  
   	sd -> sin_port = htons (atoi (port));   
   	sd -> sin_addr.s_addr = INADDR_ANY;
}

//Server response 
void receive_from_server (int sd)
{
	char buffer [65535];
	
	printf ("\nServer response:\n\n");  
    read (sd, buffer, sizeof(buffer));  
    fputs (buffer, stdout);  
    printf("\n"); 
}

//Send data to server
void send_to_server (int sd)
{
	char buffer [65535];
	
	printf ("Type here:");  
    fgets (buffer, sizeof (buffer), stdin);  
    printf ("buf: %s", buffer);
    //write (sd, buffer, sizeof (buffer));
    send (sd, buffer, sizeof (buffer), 0);
}

// main entry point  
int main(int argc, char* argv[])  
{  
	struct sockaddr_in client_sd;
		
  	//socket variables  
  	char IP [200] = "127.0.0.1";  
  	char port [200]; 
  	char request [256];
  	int sd;
  	  
  	//Get proxy port
  	printf ("\nEnter a port:");  
  	fgets (port, sizeof ("5000\n") + 1, stdin);  
  	fputs (port, stdout);
  	 
   	// create a socket  
   	if ((sd = socket (AF_INET, SOCK_STREAM, 0)) < 0)    
    	printf ("socket not created\n");  
   
   	set_socket_variables (&client_sd, port);
   	
   	// connect to proxy server at mentioned port number
   	if (connect (sd, (struct sockaddr *)&client_sd, sizeof (client_sd)) == -1)
   	{
   		close (sd);
   		exit (0);
   	}
   	
   	// Construct the HTTP CONNECT request
    snprintf (request, sizeof (request), "CONNECT %s:%d HTTP/1.1\r\nHost: %s\r\n\r\n", TARGET_HOST, 8050, TARGET_HOST);
	
	//Establish HTTP connection
   	write (sd, request, sizeof (request));
   	//receive_from_server (sd);
   	
   	//send and receive data contunuously  
   	while (1)
    {
    	send_to_server (sd);
        receive_from_server (sd);
   	};
   	
   	close(sd);  
 
  	return 0;  
}
