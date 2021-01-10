/**
 ******************************************************************************
  * File Name          : LWIP.c
  * Description        : This file provides initialization code for LWIP
  *                      middleWare.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "pppd.h"
#include "lwip/init.h"
#include "lwip/netif.h"
#include <netif/ppp/pppos.h>
#include <lwip/dns.h>
#include <netif/ppp/pppapi.h>
#include <queue.h>
#include <web.h>
#include "netif/ppp/ppp.h"

/* The PPP control block */
ppp_pcb *ppp;

/* The PPP IP interface */
struct netif ppp_netif1;

#define DATA_SIZE 1024U
static u8_t ppp_data[DATA_SIZE];

extern osMessageQueueId_t web_queue_responseHandle;

static QueueHandle_t xQueue_pppos;
extern UART_HandleTypeDef huart6;
static uint8_t data;
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    BaseType_t xHigherPriorityTaskWokenByPost;
    // We have not woken a task at the start of the ISR.
    xHigherPriorityTaskWokenByPost = pdFALSE;
    xQueueSendFromISR(xQueue_pppos, &data, &xHigherPriorityTaskWokenByPost);
    (void)HAL_UART_Receive_IT(huart, &data, 1);
}

static int pppos_read(void *data_p, size_t size, uint32_t timeout) {
    size_t bytes = 0;
    auto *p = reinterpret_cast<uint8_t *>(data_p);
    uint32_t tickstart;

    /* Init tickstart for timeout management */
    tickstart = HAL_GetTick();

    while(true) {
        if ((timeout == 0U) || ((HAL_GetTick() - tickstart) > timeout)) {
            return bytes;
        }

        BaseType_t res = xQueueReceive(xQueue_pppos, p, 20 / portTICK_RATE_MS);
        if(res != 0) {
            bytes++;
            p++;
            tickstart = HAL_GetTick();
            if(bytes >= size)
                return bytes;
        }
    }
}

void modem_reset() {
    HAL_GPIO_WritePin(ESP32_RESET_GPIO_Port, ESP32_RESET_Pin, GPIO_PIN_RESET);
    (void)osDelay(1000 / portTICK_RATE_MS);
    HAL_GPIO_WritePin(ESP32_RESET_GPIO_Port, ESP32_RESET_Pin, GPIO_PIN_SET);
    (void)osDelay(1000 / portTICK_RATE_MS);
}
httpsClientState state;
__attribute__((noreturn))
void PPPoS_Task(void *argument) {
    (void) argument;

    xQueue_pppos = xQueueCreate(DATA_SIZE, sizeof( uint8_t ));

    /* Initialize the LwIP stack with RTOS */
    tcpip_init(nullptr, nullptr);

    modem_reset();

    while(true) {
        /*
        * Create a new PPPoS interface
        */
        ppp = pppapi_pppos_create(
                &ppp_netif1,
               [](ppp_pcb *pcb, u8_t *data, u32_t len, void *ctx) {
                    if(len > 0) {
                        (void)HAL_UART_Transmit(&huart6, data, len, 50);
//                        (void) printf("PPPoS transmit %lu byte(s)\n", len);
//                        (void) printf("PPPoS transmitted data \n%s\n", data);
                        return len - huart6.TxXferCount;
                    }
                    return static_cast<u32_t>(0);
               },
               [](ppp_pcb *pcb, int err_code, void *ctx) {
                   struct netif *pppif = ppp_netif(pcb);
                   LWIP_UNUSED_ARG(ctx);

                   switch (err_code) {
                       case PPPERR_NONE:
                           (void) printf("status_cb: Connected\n");
#if PPP_IPV4_SUPPORT
                           (void) printf("   our_ipaddr  = %s\n", ipaddr_ntoa(&pppif->ip_addr));
                           (void) printf("   his_ipaddr  = %s\n", ipaddr_ntoa(&pppif->gw));
                           (void) printf("   netmask     = %s\n", ipaddr_ntoa(&pppif->netmask));
#if LWIP_DNS
                           const ip_addr_t *ns;
                           ns = dns_getserver(0);
                           (void) printf("   dns1        = %s\n", ipaddr_ntoa(ns));
                           ns = dns_getserver(1);
                           (void) printf("   dns2        = %s\n", ipaddr_ntoa(ns));
#endif /* LWIP_DNS */
#endif /* PPP_IPV4_SUPPORT */
#if PPP_IPV6_SUPPORT
                           (void) printf("   our6_ipaddr = %s\n", ip6addr_ntoa(netif_ip6_addr(pppif, 0)));
#endif /* PPP_IPV6_SUPPORT */
                           state = httpc_READY;
                           (void)osMessageQueuePut(web_queue_responseHandle, &state, 24, 1000);
                           break;
                       case PPPERR_PARAM: (void) printf("status_cb: Invalid parameter\n");
                           break;
                       case PPPERR_OPEN: (void) printf("status_cb: Unable to open PPP session\n");
                           break;
                       case PPPERR_DEVICE: (void) printf("status_cb: Invalid I/O device for PPP\n");
                           break;
                       case PPPERR_ALLOC: (void) printf("status_cb: Unable to allocate resources\n");
                           break;
                       case PPPERR_USER: (void) printf("status_cb: User interrupt\n");
                           break;
                       case PPPERR_CONNECT: (void) printf("status_cb: Connection lost\n");
                           break;
                       case PPPERR_AUTHFAIL: (void) printf("status_cb: Failed authentication challenge\n");
                           break;
                       case PPPERR_PROTOCOL: (void) printf("status_cb: Failed to meet protocol\n");
                           break;
                       case PPPERR_PEERDEAD: (void) printf("status_cb: Connection timeout\n");
                           break;
                       case PPPERR_IDLETIMEOUT: (void) printf("status_cb: Idle Timeout\n");
                           break;
                       case PPPERR_CONNECTTIME: (void) printf("status_cb: Max connect time reached\n");
                           break;
                       case PPPERR_LOOPBACK: (void) printf("status_cb: Loopback detected\n");
                           break;
                       default: (void) printf("status_cb: Unknown error code %d\n", err_code);
                           break;
                   }
                   /*
                    * This should be in the switch case, this is put outside of the switch
                    * case for example readability.
                    */

                   if (err_code == PPPERR_NONE) {
                       return;
                   }

                   /* ppp_close() was previously called, don't reconnect */
                   if (err_code == PPPERR_USER) {
                       /* ppp_free(); -- can be called here */
                       (void)ppp_free(ppp);
                       return;
                   }
               },nullptr);

        /* Set this interface as default route */
        (void)pppapi_set_default(ppp);
        /* Ask the peer for up to 2 DNS server addresses. */
        ppp_set_usepeerdns(ppp, 1);
        /* Auth configuration, this is pretty self-explanatory */
        ppp_set_auth(ppp, PPPAUTHTYPE_NONE, "login", "password");

        (void) pppapi_connect(ppp, 0);

        netif_set_default(&ppp_netif1);
        netif_set_link_up(&ppp_netif1);

        xQueueReset(xQueue_pppos); // Purge

        (void)HAL_UART_Receive_IT(&huart6, &data, 1);
        uint32_t length;
        while (true) {
            length = pppos_read(ppp_data, DATA_SIZE, 50);
            if (length > 0) {
                (void) pppos_input_tcpip(ppp, ppp_data, length);
//                (void) printf("PPPoS received %lu byte(s)\n", length);
//                (void) printf("PPPoS received data \n%s\n", ppp_data);
            }

            if (ppp->phase == PPP_PHASE_DEAD) {
                /* This happens when the client disconnects */
                (void) pppapi_free(ppp);
                ppp = nullptr;
                modem_reset();
                break;
            }
        }
    }
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
