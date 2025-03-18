#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sys/wait.h>
#include <signal.h>

#define SERVER_PORT 1234
#define BACKLOG 10
#define MAX_SERVICE_CNT 20

#define MAX_MSG_SIZE 512

pid_t child_pids[MAX_SERVICE_CNT] = {0};

void exit_handler(int sig){ // retrieve child PID 
    printf("\nParent: Killing all child processes...\n");
    for (int i = 0; i < MAX_SERVICE_CNT; i++) {
        if (child_pids[i] > 0) {
            kill(child_pids[i], SIGTERM);  // 发送 SIGTERM 终止子进程
        }
    }
    exit(0);
}
void sigchld_handler(int sig){ // when child is exited retrieve PID back.
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {  // 处理所有已退出的子进程
        printf("Parent: Child %d exited with status %d.\n", pid, status);
        for(int i=0;i<MAX_SERVICE_CNT;i++){
            if(child_pids[i] == pid)
                child_pids[i] = 0;
        }
    }
}
int server_response(int client_fd){ // use as a child process handler
    char msg_buf[MAX_MSG_SIZE]; 
    while(1){
        int recv_len = recv(client_fd, msg_buf, MAX_MSG_SIZE-1, 0);
        if(recv_len <=0){
            close(client_fd);
            return -1;
        }
        msg_buf[recv_len] = '\0';
        printf("GET FROM CLIENT: %s\n", msg_buf);
    }
    return 0;
}
int main(int argc,char* argv[]) 
{
    int ret;
    struct sockaddr_in tsocket_server_addr = {0};
    struct sockaddr_in tsocket_client_addr = {0};
    int client_num = 0;

    signal(SIGINT, exit_handler);
    signal(SIGTERM, exit_handler);

    printf("start basic network practice!\n");

    int socket_server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); 
    if(socket_server == -1){
        printf("socket create failed\n");
        return -1;
    }
    tsocket_server_addr.sin_family = AF_INET;
    tsocket_server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    tsocket_server_addr.sin_port = htons(SERVER_PORT);
    ret = bind(socket_server, (const struct sockaddr*)&tsocket_server_addr, sizeof(struct sockaddr));
    if(ret == -1){
        printf("bind failed\n");
        return -1;
    }
    ret = listen(socket_server, BACKLOG);
    if(ret == -1){
        printf("listen failed\n");
        return -1;
    }
    while(1){
        socklen_t iddrlen = sizeof(struct sockaddr);
        printf("wait for request...\n");
        int socket_client = accept(socket_server, (struct sockaddr*)&tsocket_client_addr, &iddrlen);
        if(socket_client == -1){
            printf("socket client connect failed\n");
            continue;
        }
        client_num++;
        printf("get connection, IDX=%d: %s\n", client_num, inet_ntoa(tsocket_client_addr.sin_addr));
        pid_t pid = fork();
        if(pid != 0){ // child process
            server_response(socket_client);
        }
    }
    close(socket_server);
    return 0;
}