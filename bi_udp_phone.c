#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include <sys/types.h>
#include "proto.h"

#define INFO printf

#define TIME_OUT_SEC    10
#define TIME_OUT_USEC   0

static int g_UUID = 0;

int initRecvSock(int port, struct sockaddr_in *si_recv)
{
    int sock;
    //struct sockaddr_in si_recv;

    if ((sock=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
        perror("socket()");
        return -1;
    }

    memset(si_recv, 0, sizeof(struct sockaddr_in));

    si_recv->sin_family = AF_INET;
    si_recv->sin_port = htons(port);
    si_recv->sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(sock, (struct sockaddr*)si_recv, sizeof(struct sockaddr_in)) == -1)
    {
        perror("bind()");
        return -1;
    }

    return sock;
}

int initSendSock()
{
    int sock;

    if ((sock=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
        perror("socket()");
    }

    return sock;
}

int fillRemoteInfo(char *server_ip, int server_port, struct sockaddr_in *si_remote)
{
    si_remote->sin_family = AF_INET;
    si_remote->sin_port = htons(server_port);

    if (inet_aton(server_ip , &si_remote->sin_addr) == 0)
    {
        perror("inet_aton()");
    }

    return 0;
}

int register_UUID(int sock, struct sockaddr_in *si_remote)
{
    MSG_T msg;
    memset(&msg, 0, sizeof(MSG_T));
    msg.event = REGISTER;
    msg.conn.SRC_UUID = g_UUID;

    return sendRequest(sock, si_remote, &msg);
}

int apply_make_hole(int sock, struct sockaddr_in *si_remote, int uuid, MSG_T *msg)
{
    msg->event = APPLY_MAKE_A_HOLE;
    msg->conn.SRC_UUID = g_UUID;
    msg->conn.DEST_UUID = uuid;
    return sendRequest(sock, si_remote, msg);
}

int main(int argc, char *argv[])
{
    fd_set rset;
    struct sockaddr_in si_remote;
    int slen = sizeof(struct sockaddr_in);
    int ret;
    MSG_T msg;

    int send_sock = initSendSock();
    memset(&si_remote, 0, sizeof(struct sockaddr_in));
    fillRemoteInfo(SERVER_IP, SERVER_PORT, &si_remote);

    memset(&msg, 0, sizeof(MSG_T));

    printf("Input a UUID for registering: ");
    scanf("%d", &g_UUID);

    ////////////////////////////////////////
    if (0 != register_UUID(send_sock, &si_remote))
    {
        printf("Register %d failed\n", g_UUID);
        return -1;
    }

    /////////////////////////////////////////////
    printf("1. Send NAT msg.\n2. Recv NAT msg.\n");
    printf("# ");
    scanf("%d", &ret);

    if (1 == ret) // send
    {
        int uuid;

        printf("Input a UUID for sending: ");
        scanf("%d", &uuid);

        memset(&msg, 0, sizeof(MSG_T));
        msg.event = APPLY_MAKE_A_HOLE;
        msg.conn.SRC_UUID = g_UUID;
        msg.conn.DEST_UUID = uuid;
        if (0 != sendRequest(send_sock, &si_remote, &msg))
        {
            printf("[%d]UUID %d want make_a_hole to UUID %d. Return -> failed\n", __LINE__, g_UUID, uuid);
            return -1;
        }
        printf("[%d]UUID %d want make_a_hole to UUID %d. Return -> succeed \n", __LINE__, g_UUID, uuid);

        // wait hole is ready
        // memset(&msg, 0, sizeof(MSG_T));
        // if (recvfrom(send_sock, &msg, sizeof(MSG_T), 0, (struct sockaddr*)&si_remote, &slen) == -1)
        // {
        //     perror("recvfrom");
        //    return -1;
        // }

        // all thing done, than start sending
        if (HOLE_IS_READY == getEvent(&msg))
        {
            if (-1 == sendto(send_sock, "Hello, I'm here", sizeof("Hello, I'm here"), 0, (struct sockaddr*)&msg.conn.nat_si, slen))
            {
                printf("Local UUID %d send Nat Msg to UUID %d failed.\n", getSRCUUID(&msg), getDESTUUID(&msg));
                return -1;
            }
            printf("[%d]Local UUID %d send Nat Msg to UUID %d finish.\n", __LINE__, getSRCUUID(&msg), getDESTUUID(&msg));
        }
        else
        {
            printf("[%d]The event should be HOLE_IS_READY, but %s is not what I want.\n", __LINE__, getEventStr(&msg));
        }
    }
    else //recv
    {
        MSG_T req_msg;
        int port = 9000, recv_sock;
        char buff[20] = {0};
        struct sockaddr_in si_recv;

        //printf("Input a local recv port: ");
        //scanf("%d", &port);
        recv_sock = initRecvSock(port, &si_recv);
        printf("[%d]Start waiting MAKE_A_HOLE ...\n", __LINE__);

        memset(&msg, 0, sizeof(MSG_T));
        memset(&si_remote, 0, sizeof(struct sockaddr_in));

        if (recvfrom(send_sock, &msg, sizeof(MSG_T), 0, (struct sockaddr*)&si_remote, &slen) == -1)
        {
            perror("recvfrom");
           return -1;
        }

        // rely ACK
        memset(&req_msg, 0, sizeof(MSG_T));
        req_msg.event = ACK;
        if (0 != sendOneWay(recv_sock, &si_recv, &req_msg))
        {
            printf("Reply ACK failed.\n");
        }

        if (MAKE_A_HOLE == getEvent(&msg))
        {
            printf("[%d]From UUID %d want make_a_hole to UUID %d, Return -> ",__LINE__, getSRCUUID(&msg), getDESTUUID(&msg));

            // make a hole, if event arrived, than server tell the other side HOLE_IS_READY
            memset(&req_msg, 0, sizeof(MSG_T));
            req_msg.event = HOLE_IS_READY;
            getConnFromMsg(&msg, &req_msg.conn);
            if (0 != sendOneWay(recv_sock, &si_recv, &req_msg))
            {
                printf("failed.\n");
            }
            else
            {
                printf("succeed.\n");
            }
//printf("[%s][%d][%s]\n", __FILE__, __LINE__, __FUNCTION__);
            printf("[%d]Reply MAKE_A_HOLE succeed, and wait msg from the hole ...\n", __LINE__);

            // all thing ready, than start retrieving
            if (recvfrom(recv_sock, &buff, sizeof(buff), 0, (struct sockaddr*)&si_recv, &slen) == -1)
            {
               return -1;
            }
            printf("Recv from NAT: \n", buff);
        }
        else
        {
            printf("[%d]The event should be MAKE_A_HOLE, but %s is not what I want.\n", __LINE__, getEventStr(&msg));
        }
    }
}
