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

#include <string.h>

#include <sys/stat.h>

// opendir and readdir
#include <sys/types.h>
#include <dirent.h>

// atoi
#include <stdlib.h>

#define DEFAULT_PORT 21

#define MAX_CONNECTION_NUMBER 20
#define PATH_BUFFER_SIZE 100
// marco definition of Response

#define MSG150 "150-Here comes the directory listing.\r\n"
#define MSG200 "200 Type set to I.\r\n"
#define MSG200PORT "200 PORT command successful.\r\n"
#define MSG215 "215 UNIX Type: L8.\r\n"
#define MSG220 "220 Anonymous FTP server ready.\r\n"
#define MSG221 "221 Goodbye.\r\n"
#define MSG226 "226 Directory send OK.\r\n"
#define MSG230 "230 Login Successful.\r\n"
#define MSG250 "250 cmd success\r\n"
#define MSG331 "331 User name okay, need password\r\n"
#define MSG501 "501 Syntax error.\r\n"
#define MSG530 "530 Please login with USER and PASS.\r\n"
#define MSG553 "553 Cannot rename file.\r\n"
#define MSG550CW "550 Cannot change directory.\r\n"
#define MSG550RM "550 Cannot remove directory.\r\n"
#define MSG550MK "550 Cannot create directory.\r\n"
#define MSG435 "425 Please use PASV or PROT firstly.\r\n"

struct serverState {
    int port;   
    char rootDir[PATH_BUFFER_SIZE];
    int client_number;
};

struct clientState {
    int living;   // zero for death, positive for pos in array
    int login;
    int password;
    enum mode{PASV_t, PORT_t, NOT_SET} conn_mode;
    int data_connection_fd_listen;
    int data_connection_fd_conn;
    int control_connection_fd;

    char path[PATH_BUFFER_SIZE];
    char to_rename[PATH_BUFFER_SIZE];
};

struct clientState client_array[MAX_CONNECTION_NUMBER];
struct serverState server;

// thread entry for new connection socket

enum VERB{USER, PASS, PORT, PASV, LIST, CWD, PWD, MKD, RMD, SYST, TYPE, QUIT, ABOR, RNFR, RNTO};
char *VERB_STR[] = {"USER", "PASS", "PORT", "PASV", "LIST", "CWD", "PWD", "MKD", "RMD", "SYST", "TYPE", "QUIT", "ABOR", "RNFR", "RNTO"};
int cmd_number = sizeof(VERB_STR) / sizeof(char *);

struct new_client_connected_parameter {
    int control_connection_connfd;
};

int recv_msg(int connfd, char * sentence) {
    int p = 0;
    while (1) {
        int n = read(connfd, sentence + p, 8191 - p);
        if (n < 0) {
            printf("Error read(): %s(%d)\n", strerror(errno), errno);
            close(connfd);
            continue;
        } else if (n == 0) {
            break;
        } else {
            p += n;
            if (sentence[p - 1] == '\n') {
                break;
            }
        }
    }
    
    sentence[p - 1] = '\0';
    int len = p - 1;
}

enum VERB cmd_get_verb(char* verb_str) {
    for(int i = 0; i < cmd_number; i++) {
        if (strcmp(verb_str, VERB_STR[i]) == 0)
            return i;
    }
    return 0;
}

void convert_perm_to_str(int perm, char* target) {

    for(int i = 6; i>=0; i-=3){
        int curperm = ((perm & ALLPERMS) >> i ) & 0x7;

        char buffer[3];

        int read = (curperm >> 2) & 0x1;
        int write = (curperm >> 1) & 0x1;
        int exec = (curperm >> 0) & 0x1;

        sprintf(buffer, "%c%c%c", read ? 'r' : '-' ,write ? 'w' : '-', exec ? 'x' : '-');
        strcat(target, buffer);
    }
}

/*
    return 0 for normal behavior
    return -1 for QUIT cmd
    return 1 for syntax error
*/
int cmd_handler(char * buffer, int client_id) {
    char cmd_content[10][PATH_BUFFER_SIZE];
    int n = strlen(buffer); // number of params

    int num_params = 0;
    int prev = 0;
    for(int i = 0; i < n; i++) {
        if (buffer[i] == ' ' || i == n - 1) {
            for (int j = prev; j < i; j++) {
                cmd_content[num_params][j-prev] = buffer[j];
            }
            cmd_content[num_params][i-prev] = 0;
            num_params += 1;
            prev = i + 1;
        }
    }
    num_params -= 1;
    enum VERB cmd_verb = cmd_get_verb(cmd_content[0]);

    if (cmd_verb != USER && client_array[client_id].login == 0) {
        write(client_array[client_id].control_connection_fd, MSG530, strlen(MSG530));
        return 0;
    }

    if (cmd_verb == USER) {
        if (num_params != 1) {
            return 1;
        }
        else {
            if (strcmp("anonymous", cmd_content[1]) == 0) {
                if (client_array[client_id].login == 0) {
                    client_array[client_id].login = 1;
                    char *ret_msg = MSG331;
                    write(client_array[client_id].control_connection_fd, ret_msg, strlen(ret_msg) + 1);

                }
            } else {
                const char* tmpmsg = "530 Invalid username\r\n";
                write(client_array[client_id].control_connection_fd, tmpmsg, strlen(tmpmsg));
            }
        }
    } else if (cmd_verb == PASS) {
        if (num_params != 1) {
            return 1;
        } else {
            if (client_array[client_id].password == 0) {
                client_array[client_id].password = 1;
                write(client_array[client_id].control_connection_fd, MSG230, strlen(MSG230));
            } else {
                const char* tmpmsg = "500 Invalid username or password\r\n";
                write(client_array[client_id].control_connection_fd, tmpmsg, strlen(tmpmsg));
            }
        }
    } else if (cmd_verb == PORT) {
        if (num_params == 1) {

            // error
            int h1,h2,h3,h4,p1,p2;
            sscanf(cmd_content[1], "%d,%d,%d,%d,%d,%d", &h1, &h2, &h3, &h4, &p1, &p2);
            client_array[client_id].conn_mode = PORT_t;
            int port = p1 * 256 + p2;
            char ipbuf[PATH_BUFFER_SIZE];
            sprintf(ipbuf, "%d.%d.%d.%d", h1, h2, h3, h4);
            // create a conn socket to client

            int sockfd;
            struct sockaddr_in addr;
            char sentence[8192];
            int len;
            int p;

            //create socket
            if ((sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
                printf("Error socket(): %s(%d)\n", strerror(errno), errno);
                return 1;
            }

            //set ip and port
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            if (inet_pton(AF_INET, ipbuf, &addr.sin_addr) <= 0) {			//??????ip??????:???????????????-->?????????
                printf("Error inet_pton(): %s(%d)\n", strerror(errno), errno);
                return 1;
            }

            // connect to client by the new socket
            if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                printf("Error connect(): %s(%d)\n", strerror(errno), errno);
                return 1;
            }

            client_array[client_id].data_connection_fd_conn = sockfd;
            client_array[client_id].conn_mode = PORT_t;
            write(client_array[client_id].control_connection_fd, MSG200PORT, strlen(MSG200PORT));

        } else {
            return 1;
        }
    } else if (cmd_verb == PASV) {
        
        // return localhost IP
        int random_port = rand() % 20000 + 10000;
        int p1 = random_port / 256;
        int p2 = random_port % 256;

        const char * local_ip_str = "127,0,0,1";

        // create new listening socket

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(random_port);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);

        if ((client_array[client_id].data_connection_fd_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
            printf("creating control connection socket failed");
            return 1;
        }

        // binding socket
        if (bind(client_array[client_id].data_connection_fd_listen, (struct sockaddr_in *)(&addr), sizeof(addr)) == -1) {
            perror("binding control listen socket failed");
            return -1;
        }

        if (listen(client_array[client_id].data_connection_fd_listen, MAX_CONNECTION_NUMBER) == -1) {
            perror("ERROR: listening control socket failed");
            return -1;
        }
        // return the msg by control connection and set the conn mode
        dprintf(client_array[client_id].control_connection_fd, "227 Entering Passive Mode (%s,%d,%d) \r\n", local_ip_str, p1, p2);
        client_array[client_id].conn_mode = PASV_t;

        // listening on the random port, blocking the current thread
        client_array[client_id].data_connection_fd_conn = accept(client_array[client_id].data_connection_fd_listen, NULL, NULL);
        printf("Got the connect from client \n");

    } else if (cmd_verb == LIST) {
        if (num_params == 1) {
            
            if (client_array[client_id].conn_mode == NOT_SET) {
                write(client_array[client_id].control_connection_fd, MSG435, strlen(MSG435));
                return 0;
            }

            // for file stat
            struct stat statbuf;

            // code for get LIST msg

            char current_path[PATH_BUFFER_SIZE];
            char origin_path[PATH_BUFFER_SIZE];
            memset(current_path, 0, sizeof(current_path));
            memset(origin_path,  0, sizeof(origin_path ));

            // get the current cwd
            getcwd(origin_path, sizeof(origin_path));

            // chdir to the new cwd
            chdir(cmd_content[1]);

            // get new path
            getcwd(current_path, sizeof(current_path));
            DIR* dir = opendir(current_path);
            struct dirent* entry;
            
            // following code for transmit
            if (client_array[client_id].conn_mode != NOT_SET) {
                write(client_array[client_id].control_connection_fd, MSG150, strlen(MSG150));
                while(entry = readdir(dir)) {

                    // stat the current dir
                    stat(entry->d_name,&statbuf);

                    char *perms = malloc(9);
                    memset(perms,0,9);

                    /* Convert time_t to tm struct */
                    time_t rawtime = statbuf.st_mtime;
                    struct tm* time = localtime(&rawtime);
                    char * timebuff[100];
                    strftime(timebuff,80,"%b %d %H:%M",time);
                    convert_perm_to_str((statbuf.st_mode & ALLPERMS), perms);

                    dprintf(client_array[client_id].data_connection_fd_conn, "%c%s %5d %4d %4d %8d %s %s\r\n", 
                        (entry->d_type==DT_DIR) ? 'd' : '-',
                        perms,statbuf.st_nlink,
                        statbuf.st_uid, 
                        statbuf.st_gid,
                        statbuf.st_size,
                        timebuff,
                        entry->d_name
                    );
                }
                dprintf(client_array[client_id].data_connection_fd_conn, "\r\n");
                close(client_array[client_id].data_connection_fd_conn);
                if (client_array[client_id].conn_mode == PASV_t) {
                    close(client_array[client_id].data_connection_fd_listen);
                }
                client_array[client_id].conn_mode = NOT_SET;
            } 
            chdir(origin_path);

            write(client_array[client_id].control_connection_fd, MSG226, strlen(MSG226));
        } else {
            return 1;
        }
    } else if (cmd_verb == PWD) {
        char tmp_buf[PATH_BUFFER_SIZE];
        char * target = getcwd(tmp_buf, PATH_BUFFER_SIZE);
        dprintf(client_array[client_id].control_connection_fd, "257 %s \r\n", target);
    } else if (cmd_verb == CWD) {
        if (num_params == 1) {
            if (chdir(cmd_content[1]) < 0) {
                write(client_array[client_id].control_connection_fd, MSG550CW, strlen(MSG550CW));
            } else {
                write(client_array[client_id].control_connection_fd, MSG250, strlen(MSG250));
            }
        } else {
            return 1;
        }
    } else if (cmd_verb == MKD) {
        if (num_params == 1) {
            int res = mkdir(cmd_content[1], S_IRWXU);
            if (res < 0) {
                write(client_array[client_id].control_connection_fd, MSG550MK, strlen(MSG550MK));
                
            } else {
                dprintf(client_array[client_id].control_connection_fd, "257 %s has been created\r\n", cmd_content[1]);
            }
        } else {
            return 1;
        }
    } else if (cmd_verb == SYST) {
        write(client_array[client_id].control_connection_fd, MSG215, strlen(MSG215));
    } else if (cmd_verb == TYPE) {
        write(client_array[client_id].control_connection_fd, MSG200, strlen(MSG200));
    } else if (cmd_verb == QUIT || cmd_verb == ABOR) {
        write(client_array[client_id].control_connection_fd, MSG221, strlen(MSG221));
        return -1;
    } else if (cmd_verb == RMD) {
        if (num_params == 1) {
            if (rmdir(cmd_content[1]) < 0) {
                write(client_array[client_id].control_connection_fd, MSG550RM, strlen(MSG550RM));
            } else {
                const char *tmp = "250 Directory removed.\r\n";
                write(client_array[client_id].control_connection_fd, tmp, strlen(tmp));
            }
        } else {
            return 1;
        }
    } else if (cmd_verb == RNFR) {
        if (num_params == 1) {
            strcpy(client_array[client_id].to_rename, cmd_content[1]);
            write(client_array[client_id].control_connection_fd, MSG250, strlen(MSG250));
        } else {
            return 1;
        }
    } else if (cmd_verb == RNTO) {
        if (num_params == 1) {
            printf("%s %s\n", client_array[client_id].to_rename, cmd_content[1]);
            if (rename(client_array[client_id].to_rename, cmd_content[1]) < 0) {
                write(client_array[client_id].control_connection_fd, MSG553, strlen(MSG553));
            } else {
                write(client_array[client_id].control_connection_fd, MSG250, strlen(MSG250));
            }
        } else {
            return 1;
        }
    }
    
    return 0;
}


void close_session(struct new_client_connected_parameter * session_parameter) {
    close(session_parameter->control_connection_connfd);
    free(session_parameter);
}

void* new_client_connected(void * t_param) {
    struct new_client_connected_parameter * param = (struct new_client_connected_parameter *)t_param;

    // initialize the client State
    int client_id = server.client_number++;
    client_array[client_id].living = client_id;
    client_array[client_id].login = 0;
    client_array[client_id].password = 0;
    client_array[client_id].control_connection_fd = param->control_connection_connfd;
    client_array[client_id].conn_mode = NOT_SET;

    strcpy(client_array[client_id].path, server.rootDir);
    char buffer[1000];

    int connfd = param->control_connection_connfd;

    char hello_msg[] = MSG220;
    int write_res = write(connfd, hello_msg, sizeof(hello_msg));

    while (1) {
        char *buffer = (char *)malloc(1000);
        int len = recv_msg(connfd, buffer);
        if (len < 0) {
            printf("Disconnected\n");
            break;
        }
        printf("read msg %s \n", buffer);
        int cmd_res = cmd_handler(buffer, client_id);
        free(buffer);

        if (cmd_res == -1)
            break;
        else if (cmd_res == 1) {
            write(connfd, MSG501, strlen(MSG501));
        }
    }
    
    close_session(param);
    return ((void*)0);
}

int main(int argc, char *argv[]) {

    printf("%d\n", argc);

    // random seed
    srand(time(0));

    if (argc == 1) {
        server.port = DEFAULT_PORT;
        getcwd(server.rootDir, sizeof(server.rootDir));
    } else if (argc == 3) {
        if (strcmp(argv[1], "--port") == 0) {
            server.port = atoi(argv[2]);
            getcwd(server.rootDir, sizeof(server.rootDir));
        } else if (strcmp(argv[1], "--root") == 0) {
            server.port = atoi(argv[2]);
        } else {
            perror("Wrong Parameters\n");
            return 0;
        }
    } else if (argc == 5) {
        if (strcmp(argv[1], "--port") == 0 && strcmp(argv[3], "--root") == 0) {
            server.port = atoi(argv[2]);
            if (chdir(argv[4]) < 0) {
                perror("wrong root directory path");
                return 0;
            }
            strcpy(server.rootDir, argv[4]);
        } else if (strcmp(argv[3], "--port") == 0 && strcmp(argv[1], "--root") == 0) {
            server.port = atoi(argv[4]);
            if (chdir(argv[2]) < 0) {
                perror("wrong root directory path");
                return 0;
            }
            strcpy(server.rootDir, argv[2]);
        } else {
            perror("Wrong Parameters\n");
            return 0;
        }
    } else {
            perror("Wrong number of Parameters\n");
            return 0;
    }

    chdir(server.rootDir);
    printf("Server running on port %d \n", server.port);

    int control_connection_listenfd, control_connection_connfd;
    
    strcpy(server.rootDir, "testDir");
    // the local socket address
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(server.port);
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
        if ((control_connection_connfd = accept(control_connection_listenfd, NULL, NULL)) == -1) {
            perror('Error: connection fd accept failed');
            continue;
        }

        pthread_t new_thread_id;

        struct new_client_connected_parameter * param = malloc(sizeof(struct new_client_connected_parameter));
        param->control_connection_connfd = control_connection_connfd;

        if (pthread_create(&new_thread_id, NULL, (void *)new_client_connected, (void *)param) == -1) {
            perror("Error: create new thread failed");
        }

    }
    
    close(control_connection_listenfd);
}