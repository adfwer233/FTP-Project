#include <sys/socket.h>
#include <netinet/in.h>

#include <unistd.h>
#include <errno.h>

#include <ctype.h>
#include <string.h>
#include <memory.h>
#include <stdio.h>
#include <pthread.h>

#define MAX_CONNECTION_NUMBER 20

// thread entry for new connection socket
void new_client_connected() {
    printf("new thread is running ! \n");
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
        if (pthread_create(&new_thread_id, NULL, (void *)new_client_connected, NULL) == -1) {
            perror("Error: create new thread failed");
        }

    }
    
    close(control_connection_listenfd);
}