/**
 * design for test
 * build as gcc -o client ./client.c
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sys/wait.h>
#include <signal.h>

#define SERVER_PORT 1234
#define MAX_MSG_SIZE 512

int main(int argc,char* argv[]){
    int ret;
    int socket_client;
    struct sockaddr_in tsocket_server_addr = {0};
    unsigned char send_buf[MAX_MSG_SIZE];
    int send_len;

    if(argc != 2)
    {
        printf("Usage:\n");
        printf("%s <server_ip>\n", argv[0]);
		return -1;
    }
    socket_client = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP); 
    if(socket_client == -1){
        printf("socket create failed\n");
        return -1;
    }
    tsocket_server_addr.sin_family = AF_INET;
    tsocket_server_addr.sin_port = htons(SERVER_PORT);
    if (0 == inet_aton(argv[1], &tsocket_server_addr.sin_addr)){
		printf("invalid server_ip\n");
		return -1;
	}
    ret = connect(socket_client, (const struct sockaddr*)&tsocket_server_addr, sizeof(struct sockaddr));
    if(ret == -1){
        printf("connect error!\n");
        return -1;
    }

    while(1){
        if(fgets((char*)send_buf, MAX_MSG_SIZE-1, stdin)){
            send_len = send(socket_client, send_buf, strlen((const char*)send_buf), 0);
            if(send_len <= 0){
                close(socket_client);
                return -1;
            }
        }
    }
    
    close(socket_client);
    return 0;
}