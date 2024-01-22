#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
#include <fcntl.h>

int main() {
    int server_socket, client_socket;
    int yes = 1;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }
	
	if (setsockopt (server_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof (int)) == -1)
	{
		perror ("setsockopt");
		exit (1);
	}
    // Initialize server address struct
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8080);

    // Bind the socket
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error binding socket");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
	
	fcntl (server_socket, F_SETFL, O_NONBLOCK);
	
    // Listen for incoming connections
    if (listen(server_socket, 5) == -1) {
        perror("Error listening for connections");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port 8080...\n");

    // Accept incoming connections
    client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_addr_len);
    if (client_socket == -1) {
        perror("Error accepting connection");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Shutdown the reading side of the socket (poll for write events)
    if (shutdown(client_socket, SHUT_RD) == -1) {
        perror("Error shutting down socket");
        close(server_socket);
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    // Use poll to wait for write events
    struct pollfd poll_fds[1];
    poll_fds[0].fd = client_socket;
    poll_fds[0].events = POLLIN | POLLOUT;

    int poll_result = poll(poll_fds, 1, -1); // -1: wait indefinitely
	
    if (poll_result == -1) {
        perror("Error in poll");
        close(server_socket);
        close(client_socket);
        exit(EXIT_FAILURE);
    } else if (poll_result == 0) {
        printf("No events occurred within the timeout period.\n");
    } else {
    	printf ("revents: %d\n", poll_fds [0].revents);
        if (poll_fds[0].revents & POLLIN) {
            printf("Socket is ready for writing.\n");
            
            char buf [1000];
            if (recv (client_socket, buf, 1000, 0) == -1)
				perror ("recv");
            
            printf ("&");
            // Perform write operations or other necessary actions here
        }
    }

    // Clean up
    close(server_socket);
    close(client_socket);

    return 0;
}

