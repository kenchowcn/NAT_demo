#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include <sys/types.h>
#include <time.h>

#include "proto.h"

#define TIME_OUT_SEC    120
#define TIME_OUT_USEC   0

CONNS_T g_conns_t[MAX_REGISTERS] = {0};

int initRecvSock(int port)
{
    int sock;
    struct sockaddr_in si_recv;

    if ((sock=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
        perror("socket()");
    }

    memset((char *) &si_recv, 0, sizeof(si_recv));

    si_recv.sin_family = AF_INET;
    si_recv.sin_port = htons(port);
    si_recv.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(sock, (struct sockaddr*)&si_recv, sizeof(si_recv) ) == -1)
    {
        perror("bind()");
    }

    return sock;
}

int addRegister(unsigned int UUID, struct sockaddr_in *si_remote)
{
    int i;

    printf("[%d]Register UUID %d, Return -> ", __LINE__, UUID);

    // full
    if (0 != g_conns_t[MAX_REGISTERS-1].SRC_UUID)
    {
        printf("space full.\n");
        return -1;
    }

    // find empty one
    for (i=0; i<MAX_REGISTERS; i++)
    {
        if (0 == g_conns_t[i].SRC_UUID)
            break;
        if (UUID == g_conns_t[i].SRC_UUID)
        {
            printf("already exist.\n");
            return -1;
        }
    }

    memset(&g_conns_t[i], 0, sizeof(CONNS_T));
    g_conns_t[i].SRC_UUID = UUID;
    memcpy(&g_conns_t[i].reg_si, si_remote, sizeof(struct sockaddr_in));

    printf("succeed.\n", UUID);
    return 0;
}

int delRegister(unsigned int UUID)
{
    int i;
    for (i=0; i<MAX_REGISTERS; i++)
    {
        if (UUID == g_conns_t[i].SRC_UUID)
        {
            memset(&g_conns_t[i], 0, sizeof(CONNS_T));
            memmove(&g_conns_t[i], &g_conns_t[i]+sizeof(CONNS_T), (MAX_REGISTERS-i-1)*sizeof(CONNS_T));
            return 0;
        }
    }

    return -1;
}

int getConnBySRCUUID(unsigned int UUID, CONNS_T *conn)
{
    int i;
    printf("[%d]Get Conn by src UUID %d, Return -> ", __LINE__, UUID);

    for (i=0; i<MAX_REGISTERS; i++)
    {
        if (UUID == g_conns_t[i].SRC_UUID)
        {
            memcpy(conn, &g_conns_t[i], sizeof(CONNS_T));
            printf("succeed.\n");
            return 0;
        }
    }
    printf("failed.\n");
    return -1;
}

int getConnByDESTUUID(unsigned int UUID, CONNS_T *conn)
{
    int i;
    printf("[%d]Get Conn by Dest UUID %d, Return -> ", __LINE__, UUID);

    for (i=0; i<MAX_REGISTERS; i++)
    {
        if (UUID == g_conns_t[i].DEST_UUID)
        {
            memcpy(conn, &g_conns_t[i], sizeof(CONNS_T));
            printf("succeed.\n");
            return 0;
        }
    }
    printf("failed.\n");
    return -1;
}

int updateHoleAddr(unsigned int UUID, struct sockaddr_in *si_remote)
{
    int i;
    printf("Update Hole addr UUID %d, Return -> ", UUID);
    for (i=0; i<MAX_REGISTERS; i++)
    {
        if (UUID == g_conns_t[i].SRC_UUID)
        {
            memcpy(&g_conns_t[i].nat_si, si_remote, sizeof(struct sockaddr_in));
            printf("succeed.\n");
            return 0;
        }
    }
    printf("UUID not exist.\n");
    return -1;
}

int ParseMsg(int sock)
{
    MSG_T msg, req_msg;;
    struct sockaddr_in si_remote;
    int slen = sizeof(struct sockaddr_in);

    memset(&msg, 0, sizeof(MSG_T));
    memset(&req_msg, 0, sizeof(MSG_T));
    memset(&si_remote, 0, sizeof(struct sockaddr_in));

    if (-1 == recvfrom(sock, &msg, sizeof(MSG_T), 0, (struct sockaddr*)&si_remote, &slen))
    {
        perror("recvfrom");
        return -1;
    }
    printf("[%d]UUID %d, Event %s, RecvMsg From %s:%d\n", __LINE__, getSRCUUID(&msg), getEventStr(&msg), inet_ntoa(si_remote.sin_addr), ntohs(si_remote.sin_port));

    if (MAGICID != getMagicID(&msg))
    {
        printf("[%d]Invaild Msg\n", __LINE__);
        return -1;
    }

    switch(getEvent(&msg))
    {
        case REGISTER:
        {
            addRegister(getSRCUUID(&msg), &si_remote);
            req_msg.event = ACK;
            sendOneWay(sock, &si_remote, &req_msg);
            break;
        }
        case UNREGISTER:
        {
            delRegister(getSRCUUID(&msg));
            req_msg.event = ACK;
            sendOneWay(sock, &si_remote, &req_msg);
            break;
        }
        case APPLY_MAKE_A_HOLE:
        {
            // reply requesting
            // req_msg.event = ACK;
            // sendOneWay(sock, &si_remote, &req_msg);

            // assume the end point haven't make hole yet
            req_msg.event = MAKE_A_HOLE;
            // get destination info, and send him to make a hole requesting
            getConnBySRCUUID(getDESTUUID(&msg), &req_msg.conn);
            req_msg.conn.SRC_UUID = getSRCUUID(&msg);
            req_msg.conn.DEST_UUID = getDESTUUID(&msg);
            sendRequest(sock, &req_msg.conn.reg_si, &req_msg);
            break;
        }
        case HOLE_IS_READY: // record end point hole addr
        {
            updateHoleAddr(getSRCUUID(&msg), &si_remote);
            req_msg.event = HOLE_IS_READY;
            getConnBySRCUUID(getSRCUUID(&msg), &req_msg.conn);
            sendRequest(sock, &req_msg.conn.reg_si, &req_msg);
            break;
        }
        default:
            break;
    }

    return 0;
}

int main()
{
    fd_set rset;
    char time_str[64];
    int nready;
    struct timeval timeout = {0};

    int recv_sock = initRecvSock(SERVER_PORT);

    FD_ZERO(&rset);

    printf("Prepare Receiving...\n");

    while(1)
    {
        FD_SET(recv_sock, &rset);
        timeout.tv_sec = TIME_OUT_SEC;
        timeout.tv_usec = TIME_OUT_USEC;

        nready = select(recv_sock+1, &rset, NULL, NULL, &timeout);
        if (nready == -1)
        {
            perror("select()");
        }
        else if (nready == 0)
        {
            //
        }
        else
        {
            if (FD_ISSET(recv_sock, &rset))
            {
                ParseMsg(recv_sock);
            }
        }
    }

}
