//
// Created by hristo on 1/7/21.
//

#ifndef STM32F469_DISC_ESP32_WEB_H
#define STM32F469_DISC_ESP32_WEB_H

#if __cplusplus
extern "C" {
#endif

    typedef enum {
        httpc_OK,
        httpc_ERROR,
        httpc_BUSY,
        httpc_READY,
    }httpsClientState;

    typedef enum {
        GET,
        POST,
        PUT,
        DELETE,
    }req_type;

    struct web_pkg {
        req_type type;
        const char *uri;
        char *in_buff;
        size_t in_buff_size;
        char *out_buff;
        size_t out_buff_size;
    };

#if __cplusplus
}
#endif

#endif //STM32F469_DISC_ESP32_WEB_H
