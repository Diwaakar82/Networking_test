#include <sys/socket.h>  
#include <sys/types.h>  
#include <resolv.h>  
#include <string.h>  
#include <pthread.h>  
#include <unistd.h>  
 
// A thread is created for each accepted client connection  
void *runSocket(void *vargp)  
{  
	int c_fd = (int) vargp; // get client fd from arguments passed to the thread  
	char buffer [65535];  
	int bytes = 0;

  	while (1)  
  	{  
       	//receive data from client  
       	memset (&buffer, '\0', sizeof (buffer));  
       	bytes = read (c_fd, buffer, sizeof (buffer));

       	if (bytes < 0)
        	perror ("read");
       	else if (bytes != 0)  
     	{
            write (c_fd, buffer, sizeof (buffer));                     
            fputs (buffer, stdout);       
       	}  
        fflush (stdout);  
  	};       
	return NULL;  
}  

//Set socket variables
void set_socket_variables (struct sockaddr_in *sd)
{
	memset (sd, 0, sizeof (*sd));  
	  
   	sd -> sin_family = AF_INET;  
   	sd -> sin_port = htons (5010);   
   	sd -> sin_addr.s_addr = INADDR_ANY;
}

// main entry point  
int main ()
{  
  	int client_fd;  
  	char buffer [100];  
  	int fd = 0;  

  	struct sockaddr_in server_sd; 
  	 
	// add this line only if server exits when client exits  
	signal (SIGPIPE, SIG_IGN);

  	// create a socket  
  	fd = socket (AF_INET, SOCK_STREAM, 0);  
  	
  	printf ("Server started\n");
	set_socket_variables (&server_sd);

  	// bind socket to the port  
 	bind (fd, (struct sockaddr*)&server_sd, sizeof (server_sd));  

  	// start listening at the given port for new connection requests  
  	listen (fd, SOMAXCONN);  
  	// continuously accept connections in while(1) loop  

  	while (1)  
  	{  
       	// accept any incoming connection  
       	client_fd = accept (fd, (struct sockaddr*)NULL, NULL);  

       	// if true then client request is accpted  
       	if (client_fd > 0)  
       	{  
            //multithreading variables    
        	printf ("proxy connected\n"); 
        	    
            pthread_t tid;  
            pthread_create (&tid, NULL, runSocket, (void *) client_fd);   
       	}  
  	}  
  	close(client_fd);
  	return 0;
}
