#ifndef ODR_H
#define ODR_H

#include "unp.h"

#define ODRFILE "/tmp/tmp-odr"
#define MAXIPLEN 20
#define F_FORCE 0x80
#define F_SENT 0x40
#define TYPE_MASK 0x03
#define TYPE_REQ 0
#define TYPE_REP 1
#define TYPE_PAYLOAD 2

struct msg_odr {
    uint32_t type;
    char ipsrc[MAXIPLEN];
    char ipdst[MAXIPLEN];
    uint16_t portsrc;
    uint16_t portdst;
    uint32_t hop;
    uint32_t broadcast;
    uint32_t msglen;
    char msg[0];
};

#endif
