#include <sys/socket.h>
#include <netinet/in.h>

#include <unistd.h>
#include <errno.h>

#include <ctype.h>
#include <string.h>
#include <memory.h>
#include <stdio.h>
#include <pthread.h>

#include <malloc.h>

#define MAX_CONNECTION_NUMBER 20

// thread entry for new connection socket

struct new_client_connected_parameter {
    int control_connection_connfd;
};

void close_session(struct new_client_connected_parameter * session_parameter) {
    close(session_parameter->control_connection_connfd);
    free(session_parameter);
}

void* new_client_connected(void * t_param) {
    struct new_client_connected_parameter * param = (struct new_client_connected_parameter *)t_param;

    char buffer[1000];

    int connfd = param->control_connection_connfd;

    char hello_msg[] = "hello, this is my FTP Server!";
    int write_res = write(connfd, hello_msg, sizeof(hello_msg));

    while (1) {
        int n = read(connfd, buffer, sizeof(buffer));
        if (n == 0) {
            printf("the session end\n");
            break;
        }
        buffer[n] = 0;
    }
    
    close_session(param);
    return ((void*)0);
}

int main() {
    int control_connection_listenfd, control_connection_connfd;
    
    // the local socket address
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(2333);
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // local 0.0.0.0, any
    // create control_connection socket
    if ((control_connection_listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        printf("creating control connection socket failed");
        return 1;
    }

    // binding socket
    if (bind(control_connection_listenfd, (struct sockaddr_in *)(&addr), sizeof(addr)) == -1) {
        perror("binding control listen socket failed");
        return -1;
    }

    // start listening socket connect to port 20
    if (listen(control_connection_listenfd, MAX_CONNECTION_NUMBER) == -1) {
        perror("ERROR: listening control socket failed");
        return -1;
    }


    while (1) {
        printf("here running\n");
        if ((control_connection_connfd = accept(control_connection_listenfd, NULL, NULL)) == -1) {
            perror('Error: connection fd accept failed');
            continue;
        }

        printf("new client come\n");

        pthread_t new_thread_id;

        struct new_client_connected_parameter * param = malloc(sizeof(struct new_client_connected_parameter));
        param->control_connection_connfd = control_connection_connfd;

        if (pthread_create(&new_thread_id, NULL, (void *)new_client_connected, (void *)param) == -1) {
            perror("Error: create new thread failed");
        }

    }
    
    close(control_connection_listenfd);
}