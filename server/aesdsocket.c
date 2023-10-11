/** Header files **/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <syslog.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>

/** Macros **/
#define FILE "/var/tmp/aesdsocketdata"
#define PORT 9000
#define MAX_BUFFER_SIZE (1024)

/** Global variable defination **/
int file_fd = 0;
int socket_fd = 0;
int connection = 0;


void signal_handler(int signo)
{
	if (signo == SIGINT || signo == SIGTERM)
	{
		syslog(LOG_DEBUG, "Caught the signal, exiting...");
		unlink(FILE); 
		close(connection);
		close(socket_fd);
		closelog();
	}
	exit(EXIT_SUCCESS);
}

void function_daemon()
{
	pid_t pid = fork();

	if (pid < 1) 
    {
        perror("fork");
        syslog(LOG_ERR, "Error while forking: %m");
        exit(EXIT_FAILURE);
    }

	if (pid > 0) 
    {
        exit(EXIT_SUCCESS);
    }

	if (setsid() < 0) {  
        perror("setsid failed"); 
		syslog(LOG_ERR, "Error while creating daemon session: %m");
        exit(EXIT_FAILURE); 
    }

	if (chdir("/") < 0) {  
        perror("chdir failed"); 
		syslog(LOG_ERR, "Error while changing working directory: %m");
        exit(EXIT_FAILURE); 
    }

	close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

static void client_connection(int client_fd) 
{
    char client_ip[INET_ADDRSTRLEN];
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);

    getpeername(client_fd, (struct sockaddr *)&clientAddr, &clientAddrLen);  
    
    inet_ntop(AF_INET, &(clientAddr.sin_addr), client_ip, INET_ADDRSTRLEN);

    syslog(LOG_INFO, "Accepted connection from %s", client_ip);

    char *buffer = (char *)malloc(sizeof(char) * MAX_BUFFER_SIZE);
    if (buffer == NULL) {
        syslog(LOG_ERR, "Memory allocation failed"); 
        exit(EXIT_FAILURE);
    }else{
        syslog(LOG_DEBUG, "Buffer created successfully"); 
    }

    int data_file = open(FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (data_file == -1) {
        perror("File open failed"); 
        syslog(LOG_ERR, "File open failed: %m"); 
        free(buffer);
        close(client_fd);
    }

    ssize_t recv_bytes;

    while ((recv_bytes = recv(client_fd, buffer, MAX_BUFFER_SIZE, 0)) > 0)  
    {
        write(data_file, buffer, recv_bytes);      
        if (memchr(buffer, '\n', recv_bytes) != NULL) 
        {
            break;
        }
    }

    syslog(LOG_INFO, "Received data from %s: %s, %d", client_ip, buffer, (int)recv_bytes);

    close(data_file);

    lseek(data_file, 0, SEEK_SET);  

    data_file = open(FILE, O_RDONLY);
    if (data_file == -1) {
        perror("open");
        syslog(LOG_ERR, "Unable to open file: %m");
        free(buffer);
        close(client_fd);
        return;
    }
    else
    {
        ssize_t bytes_read;
        memset(buffer, 0, sizeof(char) * MAX_BUFFER_SIZE);

        while ((bytes_read = read(data_file, buffer, sizeof(char) * MAX_BUFFER_SIZE)) > 0) {
            send(client_fd, buffer, bytes_read, 0);
        }

        free(buffer);
        close(data_file);
        close(client_fd);

        syslog(LOG_INFO, "Closed connection from %s", client_ip);
    }
}

int main(int argc, char* argv[])
{
	bool daemon_state = false;

	if (argc > 1 && strcmp(argv[1], "-d") == 0) 
    {
		syslog(LOG_INFO, "aesdsocket socket started");
        daemon_state = true;
    } 
    else 
    {
        syslog(LOG_ERR, "-d isn't passed");
    }

    if(daemon_state)
    {
        void function_daemon();
        syslog(LOG_DEBUG, "Daemon is successfully created ");
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1)
    {
        syslog(LOG_ERR, "Error while creating a socket: %m");
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY; 

    if (bind(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Socket bind failed"); 
        syslog(LOG_ERR, "Socket bind failed: %m"); 
        close(socket_fd); 
        return -1; 
    }

    if (listen(socket_fd, 1) == -1) {
        perror("Listen failed"); 
        syslog(LOG_ERR, "Listen failed: %m"); 
        close(socket_fd); 
        return -1; 
    }

    while (1) 
    {
        struct sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);

        connection = accept(socket_fd, (struct sockaddr *)&clientAddr, &clientAddrLen); 
        if (connection == -1) {
            perror("accept");
            syslog(LOG_ERR, "Error accepting client connection: %m");
            continue;
        }

        client_connection(connection);
    }

    close(socket_fd);
    if (unlink(FILE) == -1) {
        syslog(LOG_ERR, "Error removing data file: %m"); 
    }
    close(socket_fd);
    closelog();

    return 0;
}
