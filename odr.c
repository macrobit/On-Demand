#include <string.h>
#include <stdlib.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>

#include "odr.h"
#include "api.h"
#include "odr_internal.h"

#define SOCKMAX 20
#define PROTOCOL 30957
#define ETH_HDRLEN 14  // Ethernet header length

// todo: free lists

static int stale = 0x0FFFF;
//static int socks[SOCKMAX];
static struct hwa_info* head = NULL;
static struct hwa_info* sockinfos[SOCKMAX];
static int icano = 0;
static int nsock = 0;
static int sockpack = 0;

static int sockdom = 0;
static struct sockaddr_un domaddr;

static struct msg_list requests;
static struct cli_list clients;
static struct ip_list ips;

static int broadcast = 1;

int isempty(char* addr) {
    int i;

    for (i=0;i<6;i++) {
        if (addr[i] != 0)
            return 0;
    }

    return 1;
}

void getaddrs() {
    struct hwa_info* p;

    head = Get_hw_addrs();
    if (!head)
        return;

    p = head;
    for(;p;p = p->hwa_next) {
        if (isempty(p->if_haddr))
            continue;

        if (0 == strcmp(p->if_name, "lo"))
            continue;

        if (0 == strcmp(p->if_name, "eth0"))
            icano = nsock;

        sockinfos[nsock] = p;
        nsock++;
    }
}

void sockdominit() {
    struct sockaddr_un servaddr;
    int on = 1;

    sockdom = Socket(AF_LOCAL, SOCK_DGRAM, 0);

    unlink(ODRFILE);

    bzero(&domaddr, sizeof(domaddr));   /* bind an address for us */
    domaddr.sun_family = AF_LOCAL;
    strcpy(domaddr.sun_path, ODRFILE);

    Bind(sockdom, (SA *) &domaddr, sizeof(domaddr)); 

    memset(&requests, 0, sizeof(requests));
    memset(&clients, 0, sizeof(clients));
    memset(&ips, 0, sizeof(ips));
    
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sun_family = AF_LOCAL;
    strcpy(servaddr.sun_path, SERVFILE);

    addport(&servaddr, SERVPORT);
}

void sockpackinit() {
#if 0
    int i;
    for (i=0;i<nsock;i++) {
        socks[i] = Socket(PF_PACKET, SOCK_RAW, PROTOCOL);
       
        Bind(socks[i], sockinfos[i]->ip_addr, sizeof(struct sockaddr_in));
    }
#endif
    sockpack = Socket(PF_PACKET, SOCK_RAW, htons(PROTOCOL));
    //sockpack = Socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
}

struct msg_odr* msgadd(char* buf, int len) {
    struct msg_odr* msg;
    struct msg_list* msgl;
    struct hwa_info* cano;
    //int i;

    msg = (struct msg_odr*)malloc(len);
    if (!msg)
        return NULL;

    msgl = (struct msg_list*)malloc(sizeof(struct msg_list));
    if (!msgl)
        return NULL;

    memcpy(msg, buf, len);
    msgl->msg = msg;
    msgl->len = len;
    msgl->next = requests.next;

    requests.next = msgl;

    xprintf("msg add\n");
    return msg;
}

void msgsend(struct ip_list* routedst, struct sockaddr_ll* device) {
    struct msg_list* prev = &requests;
    struct msg_list* p = requests.next;
    struct hwa_info* ifi;
    int i;

    xprintf("msg send\n");

    while (p) {
        if (!strcmp(p->msg->ipdst, routedst->ip)) {
            for (i=0;i<nsock;i++) {
                ifi = sockinfos[i];
                if (ifi->if_index != device->sll_ifindex)
                    continue;

                packsend(ifi, routedst, p->msg, p->len);
                break;
            }

            prev->next = p->next;
            free(p->msg);
            free(p);
            p = prev->next;
        } else {
            prev = prev->next;
            p = p->next;
        }
    }
}

void getipsrc(char* buf, int len) {
    struct hwa_info* info;
    int i;

    info = NULL;
#if 0
    for (i=0;i<vmsnum;i++) {
        info = findifi(vms[i]);
        if (info)
            break;
    }
#endif
    if (!info) {
        info = sockinfos[icano];
    }

    struct sockaddr_in* p = (struct sockaddr_in*)info->ip_addr;
    inet_ntop(AF_INET, &p->sin_addr, buf, len);
    //printf("getipsrc: %s\n", buf);
}

uint16_t addport(struct sockaddr_un* cli, uint16_t port) {
    struct cli_list* p;
#if 0
    p = clients.next;
    while (p) {
        if (p->port == port)
            break;
        p = p->next;
    }

    if (p)
        return port;
#endif
    p = (struct cli_list*)malloc(sizeof(struct cli_list));
    if (!p)
        return 0;

    memcpy(&p->cli, cli, sizeof(struct sockaddr_un));
    //printf("addport %s %s\n", cli->sun_path, p->cli.sun_path);
    p->port = port;
    p->next = clients.next;
    clients.next = p;

    return p->port;
}

uint16_t getportsrc(struct sockaddr_un* cli) {
    struct cli_list* p = clients.next;
    int port = 0;

    while (p) {
        if (!strcmp(p->cli.sun_path, cli->sun_path))
            return p->port;

        if (p->port > port)
            port = p->port;

        p = p->next;
    }
    
    port = addport(cli, port+1);
    return port;
}

struct ip_list* addroute(char* buf, int len, struct sockaddr_ll* device) {
    struct ip_list* p;
    struct msg_odr* msg;

    msg = (struct msg_odr*)&buf[ETH_HDRLEN];
    
    p = ips.next;

    while (p) {
        if (!strcmp(p->ip, msg->ipsrc))
            break;

        p = p->next;
    }

    if (!p) {
        p = (struct ip_list*)malloc(sizeof(struct ip_list));
        if (!p)
            return NULL;

        strcpy(p->ip, msg->ipsrc);
        p->hop = 0;
        p->broadcast = 0;
        p->next = ips.next;
        ips.next = p;
    }
    
    if ((msg->type & F_FORCE) || ((msg->hop+1) < p->hop) || (!p->hop)) {
        memcpy(p->mac, buf+6, 6);
        p->time = time(NULL);
        p->ifi_index = device->sll_ifindex;
        p->hop = msg->hop+1;

        if (msg->broadcast > p->broadcast)
            p->broadcast = msg->broadcast;
    } else if (!memcmp(p->mac, buf+6, 6)) {
        p->time = time(NULL);
    }

    return p;
}
#if 0
void clearroute() {
    struct ip_list* prev = &ips;
    struct ip_list* p = cur->next;
    time_t cur;

    cur = time(NULL);
    while (p) {
        if (!stale || (p->time + stale) < cur) {
            prev->next = p->next;
            free(p);
            p = prev->next;
        } else {
            prev = prev->next;
            p = p->next;
        }
    }
}
#endif
struct hwa_info* findifi(const char* dst) {
    //char buf1[MAXLINE];
    //char buf2[MAXLINE];
    struct hwa_info* info;
    struct sockaddr_in* addr;
    struct in_addr dst_addr;
    int i;

    Inet_pton(AF_INET, dst, &dst_addr);
    for (i=0;i<nsock;i++) {
        info = sockinfos[i];
        addr = (struct sockaddr_in*)info->ip_addr;

        //Inet_ntop(AF_INET, &addr->sin_addr, buf1, sizeof(buf1));
        //Inet_ntop(AF_INET, &dst_addr, buf2, sizeof(buf2));
        //printf("findifi: %s %s %s\n", dst, buf1, buf2);
        //printf("findifi: %x %x\n", addr->sin_addr.s_addr, dst_addr.s_addr);
        if (addr->sin_addr.s_addr == dst_addr.s_addr)
            return info;
    }

    return NULL;
}

struct hwa_info* getifi(struct ip_list* route) {
    struct hwa_info* info;
    int i;


    for (i=0;i<nsock;i++) {
        info = sockinfos[i];
        if (info->if_index == route->ifi_index)
            return info;
    }

    return NULL;
}

struct ip_list* findroute(char* dst, int force) {
    struct ip_list* p;
    time_t cur;
    
    //clearroute();
    if (!stale && !force)
        return NULL;

    cur = time(NULL);
    p = ips.next;
    while (p) {
        if (!strcmp(p->ip, dst)) {
            if (((p->time + stale) < cur) && !force)
                return NULL;

            return p;
        }

        p = p->next;
    }

    return NULL;
}

int getbroadcast(char* src) {
    struct ip_list* ip;
    ip = findroute(src, 1);
    if (!ip)
        return 0;

    return ip->broadcast;
}

void packreq(struct msg_odr* msg, int ifisrc, int relay) {
    struct msg_odr msgreq;
    struct hwa_info* ifi;
    int i;

    // add req msg
    // msgadd((char*)&msgreq, sizeof(struct msg_odr));

    memcpy(&msgreq, msg, sizeof(struct msg_odr));
    msgreq.type &= (~TYPE_MASK);
    msgreq.type |= TYPE_REQ;
    if (msg->type & F_FORCE) {
        msgreq.type |= F_FORCE;
    }
    if (msg->type & F_SENT) {
        msgreq.type |= F_SENT;
    }

    msgreq.msglen = 0;
    if (relay) {
        msgreq.hop++;
    } else {
        msgreq.hop = 0;
        msgreq.broadcast = broadcast++;
    }

    //printf("pack request\n");
    for (i=0;i<nsock;i++) {
        ifi = sockinfos[i];
        if (ifi->if_index == ifisrc)
            continue;
#if 1
        if (i == icano)
            continue;
#endif

        packsend(ifi, NULL, &msgreq, sizeof(struct msg_odr));
    }
}

void packrep(struct msg_odr* msg, struct ip_list* routesrc, int hop, int relay) {
    struct msg_odr msgrep;
    struct hwa_info* ifi;
    int i;

    if (relay) {
        memcpy(&msgrep, msg, sizeof(struct msg_odr));
        msgrep.hop += 1;
    } else {
        msgrep.type = msg->type;
        strcpy(msgrep.ipsrc, msg->ipdst);
        strcpy(msgrep.ipdst, msg->ipsrc);
        msgrep.portsrc = msg->portdst;
        msgrep.portdst = msg->portsrc;

        msgrep.hop = hop;
    }

    msgrep.type &= (~TYPE_MASK);
    msgrep.type |= TYPE_REP;
    if (msg->type & F_FORCE) {
        msgrep.type |= F_FORCE;
    }

    msgrep.msglen = 0;

    //printf("pack response: %x\n", routesrc->ifi_index);
    for (i=0;i<nsock;i++) {
        ifi = sockinfos[i];
        if (ifi->if_index != routesrc->ifi_index)
            continue;

        packsend(ifi, routesrc, &msgrep, sizeof(struct msg_odr));
    }
}

void packsend(struct hwa_info* info, struct ip_list* dst, struct msg_odr* msg, int len) {
    struct sockaddr_ll device;
    char ether_frame[512];
    int frame_length;
    int type;
    int vmcur;
    int vmsrc;
    int vmdst;
    int i;

#if 0
    if (!dst && ((msg->type & TYPE_MASK) != TYPE_REQ)) {
	printf("packsend error: %s %s\n", msg->ipsrc, msg->ipdst);
	exit(0);
    }

    info = sockinfos[icano];
#endif

    memset (&device, 0, sizeof (device));
    device.sll_ifindex = info->if_index;
    device.sll_family = AF_PACKET;
    memcpy(device.sll_addr, info->if_haddr, 6 * sizeof (uint8_t));
    device.sll_halen = htons (6); 
#if 1
    if (!dst) {
        memset(ether_frame, 0xFF, 6*sizeof (uint8_t));
    } else {
        memcpy(ether_frame, dst->mac, 6*sizeof (uint8_t));
    }
    memcpy (ether_frame+6, info->if_haddr, 6*sizeof (uint8_t));
#else
    //memset(ether_frame, 0xFF, 6*sizeof (uint8_t));
    memcpy(ether_frame, info->if_haddr, 6*sizeof (uint8_t));
    memcpy (ether_frame+6, info->if_haddr, 6*sizeof (uint8_t));
#endif

    ether_frame[12] = PROTOCOL / 256;
    ether_frame[13] = PROTOCOL % 256;

    memcpy(ether_frame + ETH_HDRLEN, msg, len);

    frame_length = ETH_HDRLEN + len;

    vmcur = getvmcur();
    printf("======\n");
    printf("ODR at node vm%d sending:\n", vmcur);
    printf("frame header: ");
    //printf("packsend: %x\n", msg->type);
    for (i=0;i<ETH_HDRLEN;i++) {
        unsigned int x = ether_frame[i];
        printf("%02X ", x % 0x100);
    }
    printf("\n");
    type = (msg->type & TYPE_MASK);
    if (type  == TYPE_REQ) {
        printf("type RREQ, ");
    } else if (type == TYPE_REP) {
        printf("type RREP, ");
    } else if (type == TYPE_PAYLOAD) {
        printf("type PAYLOAD, ");
    }
    vmsrc = getvmdst(msg->ipsrc);
    vmdst = getvmdst(msg->ipdst);
    printf("src %s vm%d, ", msg->ipsrc, vmsrc);
    printf("dest %s vm%d\n", msg->ipdst, vmdst);
    printf("======\n");
    Sendto(sockpack, ether_frame, frame_length, 0, (struct sockaddr *) &device, sizeof (device));
}
#if 0
int odrsend(struct msg_odr* msg, int len) {
    struct ip_list* dst;
    struct hwa_info* info;

    dst = findroute(msg->ipdst, 0);
    if (!dst)
        return 0;
    
    info = sockinfos[dst->ifi_index];

    packsend(info, dst, msg, len);
    return 1;
}
#endif

int odrrecv(struct msg_odr* msg, int len) {
    struct sockaddr_un servaddr;
    struct cli_list* p;

    p = clients.next;
    while (p) {
        if (p->port == msg->portdst)
            break;
        p = p->next;
    }

    if (!p)
        return 0;
    
    //printf("odrrecv send: %s %x %x\n", p->cli.sun_path, p->port, len);
    Sendto(sockdom, (char*)msg, len, 0, (SA*)&p->cli, sizeof(p->cli));
}

int odrdom() {
    struct msg_odr* msg;
    struct sockaddr_un cliaddr;
    struct ip_list* routedst;
    struct hwa_info* info;
    socklen_t clilen;
    //uint16_t port;
    char buf[512];
    int len;
    int r;

    clilen = sizeof(cliaddr);
    len = Recvfrom(sockdom, buf, sizeof(buf), 0, (SA*)&cliaddr, &clilen);
    if (len <= 0)
        return;

    xprintf("odrdom: %s %x\n", cliaddr.sun_path, len);
    msg = (struct msg_odr*)buf;
    msg->portsrc = getportsrc(&cliaddr);
    if (!msg->portsrc)
        return 0;

    getipsrc(msg->ipsrc, MAXIPLEN);

    info = findifi(msg->ipdst);
    if (info) {
        xprintf("send msg to local\n");
        msg->hop = 0;
        msg->broadcast = 0;
        odrrecv(msg, len);
        return 1;
    }

    routedst = findroute(msg->ipdst, 0);
    if (!routedst || (msg->type & F_FORCE)) {
        xprintf("send request\n");
        msg = msgadd(buf, len);
        if (!msg)
            return 0;

        packreq(msg, -1, 0);
    } else {
        xprintf("send payload\n");
        info = getifi(routedst);
        if (info)
            packsend(info, routedst, msg, len);
    }
    
    return 1;
}

void odrpack() {
    struct sockaddr_ll device;
    struct hwa_info* srcinfo;
    struct hwa_info* info;
    struct msg_odr* msg;
    struct msg_odr* msg_new;
    struct ip_list* routesrc;
    struct ip_list* routedst;
    socklen_t device_len = sizeof(device);
    char buf[512];
    int len;
    int i;
    int req_relay = 0;
    int reponded = 0;
    int drop = 0;
    int broadcastsrc = 0;
    int type;

    len = Recvfrom(sockpack, buf, sizeof(buf), 0, (SA*)&device, &device_len);
    if (len <= 0)
        return;
    // todo: req, drop if repeated
    msg = (struct msg_odr*)&buf[ETH_HDRLEN];
    type = msg->type & TYPE_MASK;

    xprintf("odrpack: %x\n", type);
#if 0
    for (i=0;i<len;i++) {
        unsigned int x = buf[i];
        printf("%02X ", x % 0x100);
    }
    printf("\n");
#endif

    routesrc = findroute(msg->ipsrc, 0);

    if ((!routesrc) || ((msg->hop + 1) < routesrc->hop)) {
        req_relay = 1;
    }

    if (type == TYPE_REQ) {
	srcinfo = findifi(msg->ipsrc);
        broadcastsrc = getbroadcast(msg->ipsrc);
        if (srcinfo || msg->broadcast <= broadcastsrc) {
            if (routesrc && ((msg->hop+1) >= routesrc->hop))
                drop = 1;
        } 
    } else if (type == TYPE_REP) {
	srcinfo = findifi(msg->ipsrc);
	if (srcinfo || (routesrc && ((msg->hop+1) >= routesrc->hop)))
		drop = 1;
    }

    routesrc = addroute(buf, len, &device);
    routedst = findroute(msg->ipdst, 0);

    info = findifi(msg->ipdst);

    xprintf("odrpack route: %p %p %p\n", routesrc, routedst, info);

    if (type == TYPE_REQ) {
        if (drop) {
            xprintf("drop reqeust from %s to %s\n", msg->ipsrc, msg->ipdst);
        } else if (info) {
            xprintf("request for local\n");
            packrep(msg, routesrc, 0, 0);
        } else if (routedst && !(msg->type & F_FORCE)) {
            if (!(msg->type & F_SENT)) {
                xprintf("request for known\n");
                packrep(msg, routesrc, routedst->hop, 0);
            }
            if (req_relay) {
                xprintf("request relay\n");
                msg->type |= F_SENT;
                packreq(msg, device.sll_ifindex, 1);
            }
        } else {
            xprintf("request relay\n");
            packreq(msg, device.sll_ifindex, 1);
        }
    } else if (type == TYPE_REP) {
        if (routesrc) {
            msgsend(routesrc, &device);
        }

	if (drop) {
            xprintf("drop response from %s to %s\n", msg->ipsrc, msg->ipdst);
        } else if (!info && routedst) {
            xprintf("response relay\n");
            packrep(msg, routedst, 0, 1);
        }
    } else if (type == TYPE_PAYLOAD) {
        if (info) {
            xprintf("payload for local\n");
            odrrecv(msg, len-ETH_HDRLEN);
        } else if (routedst) {
            xprintf("payload for known\n");
            info = getifi(routedst);
            if (info) {
                msg->hop++;
                packsend(info, routedst, msg, len-ETH_HDRLEN);
            }
        } else {
            xprintf("payload to request\n");
            msg_new = msgadd((char*)msg, len-ETH_HDRLEN);
            packreq(msg_new, device.sll_ifindex, 1);
        }

        // todo
    }
}

void odrselect() {
    fd_set allset, rset;
    int maxfdp1 = 0;
    int nready;

    FD_ZERO(&allset);
    FD_SET(sockdom, &allset);
    FD_SET(sockpack, &allset);
    maxfdp1 = sockdom;
    if (maxfdp1 < sockpack)
        maxfdp1 = sockpack;
    maxfdp1+=1;
    //printf("select %x %x %x\n", sockdom, sockpack, maxfdp1);
    for ( ; ; ) {
        rset = allset;
        if ( (nready = select(maxfdp1, &rset, NULL, NULL, NULL)) < 0) {
			if (errno == EINTR)
				continue;		/* back to for() */
			else
				err_sys("select error");
		}

        if (FD_ISSET(sockdom, &rset)) {
            odrdom();
        }

        if (FD_ISSET(sockpack, &rset)) {
            odrpack();
        }
    }
}

void clean() {
    if (head)
        free_hwa_info(head);

    unlink(ODRFILE);
}

int main(int argc, char** argv) {
    //get_hw_addrs();
    //PF_PACKET SOCK_RAW select
    //PROTOCOL_value > 1536
    //packet
    //domain datagram
#if 0
    if (argc < 2) {
        printf("miss staleness argument\n");
        goto error;
    }
#endif

#if 1
    if (argc >= 2) {
        stale = atoi(argv[1]);
    }
#endif
    getaddrs();
    if (!nsock) {
        printf("fail to get interface info\n");
        goto error;
    }

    sockdominit();
    sockpackinit();

    odrselect();

error:
    clean();
    return 0;
}
