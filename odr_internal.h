#ifndef ODR_INTERNAL_H
#define ODR_INTERNEL_H

#include "unp.h"
#include "hw_addrs.h"
#include "odr.h"

struct msg_list {
    struct msg_odr* msg;
    int len;
    struct msg_list* next;
};

struct cli_list {
    struct sockaddr_un cli;
    uint16_t port;
    struct cli_list* next;
};

struct ip_list {
    char ip[MAXIPLEN];
    uint8_t mac[6];
    time_t time;
    int ifi_index;
    int hop;
    int broadcast;
    struct ip_list* next;
};

uint16_t addport(struct sockaddr_un* cli, uint16_t port);
void packsend(struct hwa_info* info, struct ip_list* dst, struct msg_odr* msg, int len);
struct hwa_info* findifi(const char* dst);
struct hwa_info* getifi(struct ip_list* route);

#endif

