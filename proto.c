#include <string.h>
//#include <malloc.h>
#include "proto.h"

unsigned int getMagicID(MSG_T *msg)
{
    return msg->magicID;
}

int getEvent(MSG_T *msg)
{
    return msg->event;
}

char *getEventStr(MSG_T *msg)
{
    switch(msg->event)
    {
    case REGISTER:
        return "REGISTER";
    case UNREGISTER:
        return "UNREGISTER";
    case APPLY_MAKE_A_HOLE:
        return "APPLY_MAKE_A_HOLE";
    case MAKE_A_HOLE:
        return "MAKE_A_HOLE";
    case HOLE_IS_READY:
        return "HOLE_IS_READY";
    case ACK:
        return "ACK";
    }
    return "Error";
}

int getSRCUUID(MSG_T *msg)
{
    return msg->SRC_UUID;
}

int getDESTUUID(MSG_T *msg)
{
    return msg->DEST_UUID;
}

int sendOneWay(int sock, struct sockaddr_in *si_remote, MSG_T *msg)
{
    int slen = sizeof(struct sockaddr_in);

    msg->magicID = MAGICID;
    if (-1 == sendto(sock, msg, sizeof(MSG_T), 0, (struct sockaddr*)si_remote, slen))
    {
        perror("sendto");
        return -1;
    }
    return 0;
}

int sendRequest(int sock, struct sockaddr_in *si_remote, MSG_T *msg)
{
    int slen = sizeof(struct sockaddr_in);

    msg->magicID = MAGICID;
    if (-1 == sendto(sock, msg, sizeof(MSG_T), 0, (struct sockaddr*)si_remote, slen))
    {
        perror("sendto");
        return -1;
    }

    // wait confirm
    memset(msg, 0, sizeof(MSG_T));
    if (recvfrom(sock, msg, sizeof(MSG_T), 0, (struct sockaddr*)si_remote, &slen) == -1)
    {
        perror("recvfrom");
       return -1;
    }

    printf("[%d][EVENT:%s][SRCUUID:%d][DESTUUID:%d]", __LINE__, getEventStr(msg), getSRCUUID(msg), getDESTUUID(msg));

    return 0;
}
