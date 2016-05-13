#include <stdio.h>
#include <netinet/in.h>

#define SERVER_IP           "45.78.29.98"
#define SERVER_PORT         9096
#define MAGICID             0xAAAA
#define MAX_REGISTERS       10

typedef enum EVENT_e
{
    REGISTER = 1,
    UNREGISTER,
    APPLY_MAKE_A_HOLE,
    MAKE_A_HOLE,
    HOLE_IS_READY,
    ACK,
    EVENT_BUTT
}EVENT_E;

typedef struct CONNS_s
{
    unsigned int SRC_UUID;
    unsigned int DEST_UUID;
    struct sockaddr_in reg_si;
    struct sockaddr_in nat_si;
}CONNS_T;

typedef struct MSG_s
{
    unsigned int magicID;
    unsigned int event;
    CONNS_T conn;
}MSG_T;
