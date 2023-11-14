
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
#include <netinet/in.h>
#include <pthread.h>
#include <sys/time.h>
#include "queue.h"
#include "../aesd-char-driver/aesd_ioctl.h"

#define FILE "/var/tmp/aesdsocketdata"
#define PORT 9000
#define MAX_BUFFER_SIZE (1024)
#define MAXIMUM_CONNECTIONS   (10)

#define SUCCESS      (0)
#define FAILURE      (-1)

int file_fd = 0;
int socket_fd = 0;
int connection = 0;
bool flag = false;

static void signal_handler(int signo);
static int function_daemon(void);
static void exit_function(void);
void *manage_thread(void *thread_node);
static void *timer_function(void *thread_node);

typedef struct socket_data {
    pthread_t thread_id;
    bool thread_complete;
    int connection;
    pthread_mutex_t *thread_mutex;
    SLIST_ENTRY(socket_data) node_count;
}socket_data_t;


pthread_mutex_t thread_mutex = PTHREAD_MUTEX_INITIALIZER;

static void signal_handler(int signo)
{
    printf("signal handler function entry\n");
	if (signo == SIGINT || signo == SIGTERM)
	{
        flag = true;
		syslog(LOG_DEBUG, "Caught the signal, exiting...");
        
        exit(0);
	}	
    printf("signal handler function exit\n");
}

static void exit_function(void)
{
    printf("exit function entry\n");
    if(unlink(FILE) == -1) {
        syslog(LOG_ERR, "Error removing data file: %m"); 
    }
    if(shutdown(socket_fd, SHUT_RDWR) == -1){
        syslog(LOG_ERR, "Error in shutdown socket_fd: %m"); 
    }
    close(socket_fd);
    close(connection);
    closelog();
    remove(FILE);
    printf("exit function exit\n");
    exit(0);
}

static int function_daemon(void)
{
    printf("daemon function entry\n");
	pid_t pid = fork();

	if (pid < 0) 
    {
        perror("fork");
        syslog(LOG_ERR, "Error while forking: %m");
        return -1;
    }

	if (pid > 0) 
    {
        exit(EXIT_SUCCESS);
    }

	if (setsid() < 0) {  
        perror("setsid failed"); 
		syslog(LOG_ERR, "Error while creating daemon session: %m");
        return -1; 
    }

	if (chdir("/") < 0) {  
        perror("chdir failed"); 
		syslog(LOG_ERR, "Error while changing working directory: %m");
        return -1;
    }

	close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    return 0;
    printf("daemon function exit\n");
}


static void *timer_function(void *thread_node){
    socket_data_t *node = NULL;
    int status = FAILURE;
    struct timespec time_period;
    time_t current_time;
    int file_fd = -1;
    struct tm *temp;
    int written_bytes = 0;
    char output[MAX_BUFFER_SIZE] = {'\0'};
    

    if (NULL == thread_node)
    {
        return NULL;
    }
    node = (socket_data_t *)thread_node;
    
    while (!flag)
    {
        if (SUCCESS != clock_gettime(CLOCK_MONOTONIC, &time_period))
        {
            syslog(LOG_ERR, "clock_gettime: %s", strerror(errno));
            status = FAILURE;
            goto exit;
        
        }
        time_period.tv_sec += 10;
        
        if (SUCCESS != clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &time_period, NULL))
        {
            syslog(LOG_ERR, "clock_nanosleep: %s", strerror(errno));
            status = FAILURE;
            goto exit;       
        }
        current_time = time(NULL);
        if (FAILURE == current_time)
        {
            syslog(LOG_ERR, "time: %s", strerror(errno));
            status = FAILURE;
            goto exit;      
        }       
        temp = localtime(&current_time);
        if (NULL == temp)
        {
            syslog(LOG_ERR, "localtime: %s", strerror(errno));
            status = FAILURE;
            goto exit;      
        }
        
        if (0 == strftime(output, sizeof(output), "timestamp: %Y %B %d, %H:%M:%S\n", temp))
        {
            syslog(LOG_ERR, "strftime: %s", strerror(errno));
            status = FAILURE;
            goto exit;        
        }
        
        file_fd = open(FILE, O_CREAT|O_RDWR|O_APPEND, 
                       S_IWUSR | S_IRUSR | S_IWGRP | S_IRGRP | S_IROTH);
        if (FAILURE == file_fd)
        {
            syslog(LOG_ERR, "Error opening %s file: %s", FILE, strerror(errno));
            status = FAILURE;
            goto exit;
        }       
        if (SUCCESS != pthread_mutex_lock(node->thread_mutex))
        {
            syslog(LOG_PERROR, "pthread_mutex_lock: %s", strerror(errno));
            status = FAILURE;
            goto exit;
        }
        
        written_bytes = write(file_fd, output, strlen(output));
        if (written_bytes != strlen(output))
        {
            syslog(LOG_ERR, "Error writing %s to %s file: %s", output, FILE,
                       strerror(errno));
            status = FAILURE;
            pthread_mutex_unlock(node->thread_mutex);
            goto exit;
        }
        if (SUCCESS != pthread_mutex_unlock(node->thread_mutex))
        {
            syslog(LOG_PERROR, "pthread_mutex_unlock: %s", strerror(errno));
            status = FAILURE;
            goto exit;
        }
        status = SUCCESS;
        close(file_fd);
    }

exit:
     (status == FAILURE) ? (node->thread_complete = false) : 
                           (node->thread_complete = true);
     return thread_node;
}

void *manage_thread(void *thread_node){

    printf("manage thread function entry\n");
    struct socket_data *node = (struct socket_data *)thread_node;
    char *Buffer = (char *)malloc(sizeof(char) * MAX_BUFFER_SIZE);
    ssize_t received_bytes;
    ssize_t read_bytes;

    if (Buffer == NULL) {
        syslog(LOG_ERR, "Error in allocating memory"); 
        exit(EXIT_FAILURE); 
    }

    memset(Buffer, 0, MAX_BUFFER_SIZE);
    
    int file = open(FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (file == -1) {
        perror("File open failed"); 
		syslog(LOG_ERR, "File open failed: %m"); 
        free(Buffer);
        exit(EXIT_FAILURE);
    }
    
    while ((received_bytes = recv(node->connection, Buffer, MAX_BUFFER_SIZE, 0)) > 0) {
        write(file, Buffer, received_bytes);
        if (memchr(Buffer, '\n', received_bytes) != NULL) {
            break;
        }
    }

    Buffer[received_bytes+1] = '\0';
    close(file);
    lseek(file, 0, SEEK_SET);

    file = open(FILE, O_RDONLY);
    if (file == -1) {
        perror("file open failed"); 
		syslog(LOG_ERR, "Error in opening file: %m"); 
        free(Buffer);
        exit(EXIT_FAILURE);
    }

    memset(Buffer, 0, sizeof(char) * MAX_BUFFER_SIZE);

    while ((read_bytes = read(file, Buffer, sizeof(char) * MAX_BUFFER_SIZE)) > 0) {
        send(node->connection, Buffer, read_bytes, 0);
    }

    free(Buffer);
    close(file);
    close(node->connection);

    node->thread_complete = true;
    pthread_exit(node);

    syslog(LOG_INFO, "Connection closed");
    printf("manage thread function exit\n");
    return node;
}


int main(int argc, char **argv)
{
    printf("int main function entry\n");
	bool daemon_state = false;
    const int reuse = 1;
    socket_data_t *data_ptr = NULL;

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
        function_daemon();
        syslog(LOG_DEBUG, "Daemon is successfully created ");
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    pthread_mutex_init(&thread_mutex, NULL);
    SLIST_HEAD(socket_head, socket_data) head;
    SLIST_INIT(&head);

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1)
    {
        perror("Socket create failed");
        syslog(LOG_ERR, "Error while creating a socket: %m");
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY; 

    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) 
    {
        perror("setsockopt");
        syslog(LOG_ERR, "Error while setting socket options: %m");
        close(socket_fd);
        return -1;
    }

    if (bind(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Socket bind failed"); 
        syslog(LOG_ERR, "Socket bind failed: %m"); 
        close(socket_fd); 
        return -1; 
    }

    if (listen(socket_fd, MAXIMUM_CONNECTIONS) == -1) {
        perror("Listen failed"); 
        syslog(LOG_ERR, "Listen failed: %m"); 
        close(socket_fd); 
        return -1; 
    }

    data_ptr = (socket_data_t *)malloc(sizeof(socket_data_t));
    data_ptr->thread_complete = false;
    data_ptr->thread_mutex = &thread_mutex;
        
    int timer_function_status = pthread_create(&data_ptr->thread_id, NULL, timer_function, data_ptr);
        
    if(timer_function_status != 0)
    {
        syslog(LOG_ERR, "pthread_create failed: %s", strerror(errno));
        free(data_ptr);
        data_ptr = NULL;
        return -1;
    } 
    SLIST_INSERT_HEAD(&head, data_ptr, node_count);

    while (!flag) 
    {
        char client_ip[INET_ADDRSTRLEN];
        struct sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);
    
        connection = accept(socket_fd, (struct sockaddr *)&clientAddr, &clientAddrLen); 
        if (connection == -1) {
            perror("accept");
            syslog(LOG_ERR, "Error accepting client connection: %m");
            continue;
        }else{

            getpeername(connection, (struct sockaddr *)&clientAddr, &clientAddrLen); 
            inet_ntop(AF_INET, &(clientAddr.sin_addr), client_ip, INET_ADDRSTRLEN);
            syslog(LOG_INFO, "Accepted connection from %s", client_ip);

            data_ptr = (socket_data_t *)malloc(sizeof(socket_data_t));
            data_ptr->connection = connection;
            data_ptr->thread_complete = false;
            data_ptr->thread_mutex = &thread_mutex;
            SLIST_INSERT_HEAD(&head, data_ptr, node_count);

            int manage_thread_status = pthread_create(&data_ptr->thread_id, NULL, manage_thread, data_ptr);
        
            if(manage_thread_status != 0)
            {
                syslog(LOG_ERR, "pthread_create failed: %s", strerror(errno));
                free(data_ptr);
                data_ptr = NULL;
                return -1;
            }      
    
            data_ptr = NULL;
            SLIST_FOREACH(data_ptr, &head, node_count)
            {
                if (data_ptr->thread_complete == true)
                {
                    syslog(LOG_INFO, "1 Joined thread id: %ld", data_ptr->thread_id);
                    pthread_join(data_ptr->thread_id, NULL);
                    SLIST_REMOVE(&head, data_ptr, socket_data, node_count);
                    free(data_ptr);
                    data_ptr = NULL;
                    break;
                }
            }
        }
    }

    exit_function();
    pthread_mutex_destroy(&thread_mutex);
    pthread_join(data_ptr->thread_id, NULL);
    printf("int main function exit\n");
    return 0;
}
