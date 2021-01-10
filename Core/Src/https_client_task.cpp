//
// Created by hristo on 1/5/21.
//

#include <cmsis_os2.h>
#include "HTTPClient.h"
#include "https_client_task.h"
#include "web.h"

HTTPClient web;

extern osMessageQueueId_t web_queue_requestHandle;
extern osMessageQueueId_t web_queue_responseHandle;

struct web_pkg web_pkg;

__attribute__((noreturn))
void HTTPSClient_Task(void *argument) {
    (void) argument;

    osStatus_t res;
    HTTPResult http_res;
    httpsClientState state = httpc_BUSY;

    while(true) {
        res = osMessageQueueGet(web_queue_requestHandle, &web_pkg, nullptr, osWaitForever);
        if(res != osOK) continue;

        HTTPText in_text(web_pkg.in_buff, web_pkg.in_buff_size);

        switch (web_pkg.type) {
            case GET:
                http_res = web.get(web_pkg.uri, &in_text);
                break;
            case POST: {
                HTTPText out_text(web_pkg.out_buff, web_pkg.out_buff_size);
                http_res = web.post(web_pkg.uri, out_text, &in_text);
                break;
            }
            case PUT: {
                HTTPText out_text(web_pkg.out_buff, web_pkg.out_buff_size);
                http_res = web.put(web_pkg.uri, out_text, &in_text);
                break;
            }
            case DELETE:
                http_res = web.del(web_pkg.uri, &in_text);
                break;
            default:
                http_res = HTTP_ERROR;
                break;
        }

        if(http_res != HTTP_OK)
            state = httpc_ERROR;
        else
            state = httpc_OK;

        (void)osMessageQueuePut(web_queue_responseHandle, &state, 24, 1000);
    }
}


