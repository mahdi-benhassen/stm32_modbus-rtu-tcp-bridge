#include "main.h"
#include "lwip/netif.h"
#include "lwip/etharp.h"
#include "lwip/tcpip.h"
#include "netif/ethernet.h"
#include <string.h>

/*
 * ============================================================
 *  lwIP Ethernet Interface — Minimal Stub
 *
 *  The standalone stm32f4xx_hal_driver repo ships a refactored
 *  ETH HAL with a different API than the classic CubeF4 mono-
 *  repo.  This stub provides the required symbols (gnetif,
 *  ethernetif_init, HAL_ETH_RxCpltCallback) so the project
 *  compiles and links.  Replace with a full implementation
 *  once the correct HAL ETH API is matched.
 * ============================================================ */

ETH_HandleTypeDef heth;

struct netif gnetif;

static const uint8_t default_mac[6] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x01 };

err_t ethernetif_init(struct netif *netif)
{
    LWIP_ASSERT("netif != NULL", netif != NULL);

    netif->name[0] = 'm';
    netif->name[1] = 'b';
    netif->hwaddr_len = ETH_HWADDR_LEN;
    memcpy(netif->hwaddr, default_mac, ETH_HWADDR_LEN);
    netif->mtu = 1500;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;

#if LWIP_IPV4 && LWIP_ARP
    netif->output = etharp_output;
#endif
    netif->linkoutput = NULL;

    return ERR_OK;
}

/*
 * Weak callback override — receives completions from the ETH IRQ.
 * Currently a no-op since the full ETH TX/RX path isn't wired up.
 */
void HAL_ETH_RxCpltCallback(ETH_HandleTypeDef *heth_arg)
{
    (void)heth_arg;
}

/*
 * TX callback placeholder.
 */
void HAL_ETH_TxCpltCallback(ETH_HandleTypeDef *heth_arg)
{
    (void)heth_arg;
}

/*
 * Error callback placeholder.
 */
void HAL_ETH_ErrorCallback(ETH_HandleTypeDef *heth_arg)
{
    (void)heth_arg;
}
