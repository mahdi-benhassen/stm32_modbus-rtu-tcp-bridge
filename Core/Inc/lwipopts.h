#ifndef __LWIPOPTS_H
#define __LWIPOPTS_H

#define NO_SYS                          0
#define LWIP_SOCKET                     1
#define LWIP_NETCONN                    1

#define LWIP_DHCP                       1
#define LWIP_AUTOIP                     0
#define LWIP_IGMP                       0
#define LWIP_DNS                        0
#define LWIP_UDP                        1

#define MEM_ALIGNMENT                   4
#define MEM_SIZE                        (16 * 1024)
#define MEMP_NUM_PBUF                   8
#define MEMP_NUM_UDP_PCB                4
#define MEMP_NUM_TCP_PCB                8
#define MEMP_NUM_TCP_PCB_LISTEN         2
#define MEMP_NUM_TCP_SEG                16
#define MEMP_NUM_NETBUF                 4
#define MEMP_NUM_NETCONN                8
#define MEMP_NUM_SYS_TIMEOUT            12

#define PBUF_POOL_SIZE                  16
#define PBUF_POOL_BUFSIZE               256

#define LWIP_TCP                        1
#define TCP_TTL                         255
#define TCP_QUEUE_OOSEQ                 0
#define TCP_MSS                         536
#define TCP_SND_BUF                     (4 * TCP_MSS)
#define TCP_WND                         (4 * TCP_MSS)
#define TCP_SND_QUEUELEN                (4 * TCP_SND_BUF / TCP_MSS)
#define TCP_KEEPALIVE                   1
#define TCP_KEEPIDLE                    60000U    /* 60s idle before keepalive probe */
#define TCP_KEEPINTVL                   10000U    /* 10s between keepalive probes  */
#define TCP_KEEPCNT                     3U        /* 3 failed probes -> disconnect */

#define LWIP_ICMP                       1
#define LWIP_RAW                        0
#define PPP_SUPPORT                     0

#define CHECKSUM_BY_HARDWARE            0
#define LWIP_ACD                        0
#define LWIP_DHCP_DOES_ACD_CHECK        0

#define LWIP_STATS                      0
#define LWIP_STATS_DISPLAY              0
#define LINK_STATS                      0
#define ETHARP_STATS                    0
#define IP_STATS                        0
#define ICMP_STATS                      0
#define TCP_STATS                       0
#define MEM_STATS                       0
#define MEMP_STATS                      0
#define PBUF_STATS                      0
#define SYS_STATS                       0

#define LWIP_DEBUG                      0

#define ETHARP_SUPPORT_STATIC_ENTRIES   0

#define LWIP_NETIF_LINK_CALLBACK        1
#define LWIP_NETIF_STATUS_CALLBACK      1

#define LWIP_SO_RCVTIMEO                1

#define TCPIP_THREAD_NAME               "tcpip_thread"
#define TCPIP_THREAD_STACKSIZE          1024
#define TCPIP_THREAD_PRIO               (tskIDLE_PRIORITY + 2)
#define TCPIP_MBOX_SIZE                 16
#define DEFAULT_RAW_RECVMBOX_SIZE       4
#define DEFAULT_UDP_RECVMBOX_SIZE       4
#define DEFAULT_TCP_RECVMBOX_SIZE       8
#define DEFAULT_ACCEPTMBOX_SIZE         4

#define LWIP_ETHERNET                   1
#define ETH_PHY_ADDR                    0x01U
#define LWIP_NETIF_HOSTNAME             1

#endif /* __LWIPOPTS_H */
