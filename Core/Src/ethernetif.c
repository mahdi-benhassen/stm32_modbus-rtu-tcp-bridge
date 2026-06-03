#include "main.h"
#include "lwip/netif.h"
#include "lwip/etharp.h"
#include "lwip/tcpip.h"
#include "lwip/dhcp.h"
#include "netif/ethernet.h"
#include "stm32f4xx_hal.h"
#include <string.h>

/*
 * ============================================================
 *  lwIP Ethernet Interface Driver
 *
 *  STM32F407 + LAN8720A / DP83848 PHY via RMII
 *
 *  Adapted from STM32CubeF4 lwIP example (ethernetif.c).
 *  Handles:
 *   - MAC initialization (ETH peripheral)
 *   - PHY init / link status via MDIO
 *   - lwIP netif linkage (rx/tx descriptors, callbacks)
 *   - ETH IRQ handling
 * ============================================================ */

#define ETH_RX_BUF_SIZE     ETH_MAX_PACKET_SIZE
#define ETH_TX_BUF_SIZE     ETH_MAX_PACKET_SIZE
#define ETH_RXBUFNB         4
#define ETH_TXBUFNB         4

/* MAC address - locally administered (LSB of first octet = 1) */
static uint8_t mac_addr[6] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x01 };

/* ETH Handle */
ETH_HandleTypeDef heth;

/* DMA descriptors */
ETH_DMADescTypeDef DMATxDescTab[ETH_TXBUFNB] __attribute__((aligned(4)));
ETH_DMADescTypeDef DMARxDescTab[ETH_RXBUFNB] __attribute__((aligned(4)));

/* Data buffers */
uint8_t Tx_Buff[ETH_TXBUFNB][ETH_TX_BUF_SIZE] __attribute__((aligned(4)));
uint8_t Rx_Buff[ETH_RXBUFNB][ETH_RX_BUF_SIZE] __attribute__((aligned(4)));

/* Global network interface */
struct netif gnetif;

/* Forward declarations */
static void ethernetif_input(void *argument);

/* ---- Low-level PHY/MAC Init ---- */

static void ETH_GPIO_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Enable clocks */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    /* RMII Pins: PA1(REF_CLK), PA2(MDIO), PA7(CRS_DV) */
    GPIO_InitStruct.Pin       = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_7;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* RMII Pins: PB11(TX_EN), PB12(TXD0), PB13(TXD1) */
    GPIO_InitStruct.Pin       = GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13;
    GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* RMII Pins: PC1(ETH_MDC), PC4(RXD0), PC5(RXD1) */
    GPIO_InitStruct.Pin       = GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5;
    GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* RMII Pins: PG11(TX_EN alt), PG13(TXD0 alt), PG14(TXD1 alt) */
    /* GPIO_InitStruct.Pin       = GPIO_PIN_11 | GPIO_PIN_13 | GPIO_PIN_14; */
    /* HAL_GPIO_Init(GPIOG, &GPIO_InitStruct); */
}

static void MAC_PHY_Config(void)
{
    ETH_GPIO_Config();

    heth.Instance              = ETH;
    heth.Init.AutoNegotiation  = ETH_AUTONEGOTIATION_ENABLE;
    heth.Init.Speed            = ETH_SPEED_100M;
    heth.Init.DuplexMode       = ETH_MODE_FULLDUPLEX;
    heth.Init.PhyAddress       = ETH_PHY_ADDR;
    heth.Init.MACAddr          = mac_addr;
    heth.Init.RxMode           = ETH_RXINTERRUPT_MODE;
    heth.Init.ChecksumMode     = ETH_CHECKSUM_BY_HARDWARE;
    heth.Init.MediaInterface   = ETH_MEDIA_INTERFACE_RMII;

    if (HAL_ETH_Init(&heth) != HAL_OK) {
        Error_Handler();
    }

    /* Initialize DMA descriptors */
    if (HAL_ETH_DMAInit(&heth, DMATxDescTab, DMARxDescTab,
                        Tx_Buff, Rx_Buff) != HAL_OK) {
        Error_Handler();
    }
}

/* ---- lwIP netif callbacks ---- */

static err_t low_level_init(struct netif *netif)
{
    /* Set MAC address */
    netif->hwaddr_len = ETH_HWADDR_LEN;
    memcpy(netif->hwaddr, mac_addr, ETH_HWADDR_LEN);

    /* MTU */
    netif->mtu = 1500;

    /* Device capabilities */
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP |
                   NETIF_FLAG_LINK_UP;

    /* Initialize PHY/MAC */
    MAC_PHY_Config();

    /* Set netif link state */
    netif->linkoutput = NULL;

    return ERR_OK;
}

static err_t low_level_output(struct netif *netif, struct pbuf *p)
{
    (void)netif;
    err_t errval = ERR_OK;
    struct pbuf *q;
    uint8_t *buffer = (uint8_t *)Tx_Buff[0];
    uint32_t framelen = 0;

    /* Copy pbuf chain into contiguous TX buffer */
    for (q = p; q != NULL; q = q->next) {
        memcpy(buffer + framelen, q->payload, q->len);
        framelen += q->len;

        if (framelen > ETH_TX_BUF_SIZE) {
            errval = ERR_BUF;
            break;
        }
    }

    if (errval == ERR_OK) {
        /* Transmit via ETH DMA */
        if (HAL_ETH_TransmitFrame(&heth, framelen) != HAL_OK) {
            errval = ERR_IF;
        } else {
            /*
             * Wait for TX complete (with timeout).
             * In a production system this would be interrupt-driven,
             * but for simplicity we poll here (bridge task is FreeRTOS,
             * so this blocks the calling tcpip thread briefly).
             */
            uint32_t timeout = 1000;
            while ((HAL_ETH_GetTxDescStatus(&heth) & ETH_DMATXDESC_OWN) &&
                   timeout > 0) {
                timeout--;
            }
            if (timeout == 0) {
                errval = ERR_TIMEOUT;
            }
        }
    }

    return errval;
}

static struct pbuf *low_level_input(struct netif *netif)
{
    (void)netif;
    struct pbuf *p = NULL;
    struct pbuf *q;
    uint32_t framelen = 0;
    uint32_t i = 0;
    uint8_t *buffer;

    /* Get received frame length */
    framelen = HAL_ETH_GetRxDataLength(&heth);

    if (framelen == 0 || framelen > ETH_RX_BUF_SIZE) {
        HAL_ETH_FlushRxData(&heth);
        return NULL;
    }

    /* Allocate pbuf from lwIP pool */
    p = pbuf_alloc(PBUF_RAW, (uint16_t)framelen, PBUF_POOL);
    if (p != NULL) {
        buffer = (uint8_t *)Rx_Buff[0];
        for (q = p; q != NULL; q = q->next) {
            memcpy(q->payload, buffer + i, q->len);
            i += q->len;
        }
    }

    /* Flush the RX descriptor so it's ready for next frame */
    HAL_ETH_FlushRxData(&heth);

    return p;
}

/*
 * Ethernet input function called from tcpip_thread after
 * low_level_input returns a pbuf. This is the callback
 * registered with lwIP.
 */
static void ethernetif_input(void *argument)
{
    struct netif *netif = (struct netif *)argument;
    struct pbuf *p;

    /* Read a received packet from the Ethernet interface */
    p = low_level_input(netif);

    if (p == NULL) {
        return;
    }

    /* Pass the packet to lwIP */
    if (netif->input(p, netif) != ERR_OK) {
        pbuf_free(p);
    }
}

/*
 * lwIP netif initialization callback.
 * Called by netif_add() to initialize this interface.
 */
err_t ethernetif_init(struct netif *netif)
{
    LWIP_ASSERT("netif != NULL", netif != NULL);

    netif->name[0] = 'm';
    netif->name[1] = 'b';

    netif->output  = etharp_output;
    netif->input   = NULL;

    return low_level_init(netif);
}

/*
 * ETH IRQ Handler callback for received packets.
 * Called from ETH_IRQHandler in stm32f4xx_it.c.
 * This posts a message to tcpip_thread to process the packet.
 */
void HAL_ETH_RxCpltCallback(ETH_HandleTypeDef *heth_arg)
{
    if (heth_arg != &heth) return;

    /*
     * ethernetif_input must run in the tcpip_thread context.
     * Use tcpip_callback_with_block or tcpip_try_callback.
     */
    tcpip_try_callback(ethernetif_input, &gnetif);
}
