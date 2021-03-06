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

int initSendSock()
{
    int sock;
    struct sockaddr_in sin;

    if ((sock=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
        perror("socket()");
    }

	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_family = AF_INET;
	sin.sin_port = 0;

	if (0 > bind(sock, (struct sockaddr*)&sin, sizeof(sin)))
    {
        perror("bind()");
        return -1;
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
    msg.SRC_UUID = g_UUID;

    sendRequest(sock, si_remote, &msg);
    return waitMsg(sock, si_remote);
}

int apply_make_hole(int sock, struct sockaddr_in *si_remote, int uuid, MSG_T *msg)
{
    msg->event = APPLY_MAKE_A_HOLE;
    msg->SRC_UUID = g_UUID;
    msg->DEST_UUID = uuid;
    return sendRequest(sock, si_remote, msg);
}

int main(int argc, char *argv[])
{
    fd_set rset;
    struct sockaddr_in si_remote, hole_si;
    int slen = sizeof(struct sockaddr_in);
    int ret, i;
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

    if (1 == ret) // send 2
    {
        int uuid;

        printf("Input a UUID for sending: ");
        scanf("%d", &uuid);

        memset(&msg, 0, sizeof(MSG_T));
        msg.event = APPLY_MAKE_A_HOLE;
        msg.SRC_UUID = g_UUID;
        msg.DEST_UUID = uuid;
        if (0 != sendRequest(send_sock, &si_remote, &msg))
        {
            printf("[%d]UUID %d want make_a_hole to UUID %d. Return -> failed\n", __LINE__, g_UUID, uuid);
            return -1;
        }
        printf("[%d]UUID %d want make_a_hole to UUID %d. Return -> succeed \n", __LINE__, g_UUID, uuid);

        waitMsg(send_sock, &si_remote, &msg);

        // all thing done, than start sending
        if (HOLE_IS_READY == getEvent(&msg))
        {
            for (i=0; i<3; i++)
            {
                if (-1 == sendto(send_sock, "Hello, I'm here", sizeof("Hello, I'm here"), 0, (struct sockaddr*)&msg.nat_si, slen))
                {
                    perror("sendto()");
                    return -1;
                }
            }
            printf("[%d]Local UUID %d send Nat[%s:%d] Msg to UUID %d finish.\n", __LINE__, getDESTUUID(&msg), inet_ntoa(msg.nat_si.sin_addr), ntohs(msg.nat_si.sin_port), getSRCUUID(&msg));
        }
        else
        {
            printf("[%d]The event should be HOLE_IS_READY, but %s is not what I want.\n", __LINE__, getEventStr(&msg));
        }
    }
    else //recv 1
    {
        MSG_T req_msg;
        int port = 65330, recv_sock;
        char buff[20] = {0};
        struct sockaddr_in si_recv;

        //printf("Input a local recv port: ");
        //scanf("%d", &port);
        recv_sock = initSendSock();
        printf("[%d]Start waiting MAKE_A_HOLE ...\n", __LINE__);

        memset(&msg, 0, sizeof(MSG_T));
        memset(&si_remote, 0, sizeof(struct sockaddr_in));

        if (recvfrom(send_sock, &msg, sizeof(MSG_T), 0, (struct sockaddr*)&si_remote, &slen) == -1)
        {
            perror("recvfrom");
           return -1;
        }

        if (MAKE_A_HOLE == getEvent(&msg))
        {
            printf("[%d]From UUID %d want make_a_hole to UUID %d, Return -> ",__LINE__, getSRCUUID(&msg), getDESTUUID(&msg));
            // punching a hole
            memset(&req_msg, 0, sizeof(MSG_T));
            memcpy(&hole_si, &msg.nat_si, sizeof(struct sockaddr_in));
            if (0 != sendOneWay(recv_sock, &msg.nat_si, &req_msg))
            {
                printf("punching failed. ");
            }
            else
            {
                printf("punching succeed. ");
            }

            memset(&req_msg, 0, sizeof(MSG_T));
            req_msg.event = HOLE_IS_READY;
            req_msg.SRC_UUID = getDESTUUID(&msg);
            req_msg.DEST_UUID = getSRCUUID(&msg);
            // reply hole is ready
            if (0 != sendOneWay(send_sock, &si_remote, &req_msg))
            {
                printf("reply server failed.\n");
            }
            else
            {
                printf("reply server succeed.\n");
            }
//printf("[%s][%d][%s]\n", __FILE__, __LINE__, __FUNCTION__);
            printf("[%d]Reply MAKE_A_HOLE succeed, and wait msg from the hole ...%s:%d\n", __LINE__, inet_ntoa(hole_si.sin_addr), ntohs(hole_si.sin_port));

            // all thing ready, than start retrieving
            if (recvfrom(recv_sock, &buff[0], sizeof(buff), 0, (struct sockaddr*)&hole_si, &slen) == -1)
            {
               return -1;
            }
            printf("Recv from NAT: %s\n", buff);
        }
        else
        {
            printf("[%d]The event should be MAKE_A_HOLE, but %s is not what I want.\n", __LINE__, getEventStr(&msg));
        }
    }
}
