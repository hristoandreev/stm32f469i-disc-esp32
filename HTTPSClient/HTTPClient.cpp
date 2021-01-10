/* HTTPClient.cpp */
/* Copyright (C) 2012 mbed.org, MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 * and associated documentation files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish, distribute,
 * sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "../wolfSSL/wolfSSL.I-CUBE-wolfSSL_conf.h"
#include <cstdio>
//#include <iostream>
//Debug is disabled by default
#if 0
//Enable debug

#define DBG(x, ...) std::printf("[HTTPClient : DBG]" x"\r\n", ##__VA_ARGS__);

#define WOLF_DEBUG_ON   // wolfSSL_Debugging_ON() ;
#else
//Disable debug
#define DBG(x, ...)
#define WOLF_DEBUG_ON 
#endif

#define WARN(x, ...) std::printf("[HTTPClient : WARN]" x"\r\n", ##__VA_ARGS__)
#define ERR(x, ...) std::printf("[HTTPClient : ERR]" x"\r\n", ##__VA_ARGS__)

#define HTTP_PORT 80
#define HTTPS_PORT 443

#define OK 0

#define MIN(x,y) (((x)<(y))?(x):(y))
#define MAX(x,y) (((x)>(y))?(x):(y))

#include <cstring>

#include "wolfssl/wolfcrypt/settings.h"
#include "wolfssl/wolfcrypt/types.h"
#include "wolfssl/internal.h"
#include "wolfssl/ssl.h"

#include "HTTPClient.h"
#include "TCPSocketConnection.h"
#include <cmsis_os2.h>
//#include <cstring>

#define wait(a)  osDelay(a * 1000)

static  TCPSocketConnection m_sock;
#define CHUNK_SIZE    (256*4*8)
#define SEND_BUF_SIZE 512
static char send_buf[SEND_BUF_SIZE] ;
static char *send_buf_p ;

static int SocketReceive(WOLFSSL* ssl, char *buf, int sz, void *sock)
{
    int n ;
    int i ;
    
#define RECV_RETRY 3
    for(i=0; i<RECV_RETRY; i++) {
        n = ((TCPSocketConnection *)sock)->receive(buf, sz) ;
        if(n >= 0)return n ;
        WARN("Retry Recv");
        wait(0.2) ;
    }
    ERR("SocketReceive:%d/%d\n", n, sz)  ;
    return n ;

}

static int SocketSend(WOLFSSL* ssl, char *buf, int sz, void *sock)
{
    int n ;

    wait(0.1) ;
    n = ((TCPSocketConnection *)sock)->send(buf, sz);
    if(n > 0) {
        wait(0.3) ;
        return n ;
    } else  ERR("SocketSend:%d/%d\n", n, sz);
    return n ;
}

static void base64enc(char *out, const char *in)
{
    const char code[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=" ;
    int i = 0, x = 0, l = 0;

    for (; *in; in++) {
        x = x << 8 | *in;
        for (l += 8; l >= 6; l -= 6) {
            out[i++] = code[(x >> (l - 6)) & 0x3f];
        }
    }
    if (l > 0) {
        x <<= 6 - l;
        out[i++] = code[x & 0x3f];
    }
    for (; i % 4;) {
        out[i++] = '=';
    }
    out[i] = '\0' ;
}

HTTPClient::HTTPClient() :
    m_basicAuthUser(nullptr), m_basicAuthPassword(nullptr), m_httpResponseCode(0)
{
    WOLF_DEBUG_ON ;
    ctx = nullptr ;
    ssl = nullptr ;
    SSLver = 3 ;
    m_basicAuthUser = nullptr ;
    redirect_url = nullptr ;
    redirect = 0 ;
    header = nullptr ;
    dumpReqH = false ;
    dumpResH = false ;
}

HTTPClient::~HTTPClient()
{

}

HTTPResult HTTPClient::basicAuth(const char* user, const char* password) //Basic Authentification
{
#define AUTHB_SIZE 128
    if((strlen(user) + strlen(password)) >= AUTHB_SIZE)
        return HTTP_ERROR ;
    m_basicAuthUser = user;
    m_basicAuthPassword = password;
    return HTTP_OK ;
}

HTTPResult HTTPClient::get(const char* url, IHTTPDataIn* pDataIn, int timeout /*= HTTP_CLIENT_DEFAULT_TIMEOUT*/) //Blocking
{
    HTTPResult ret ;
    ret = connect(url, HTTP_GET, nullptr, pDataIn, timeout);
    return ret;
}

HTTPResult HTTPClient::get(const char* url, char* result, size_t maxResultLen, int timeout /*= HTTP_CLIENT_DEFAULT_TIMEOUT*/) //Blocking
{
    HTTPText str(result, maxResultLen);   
    return get(url, &str, timeout);
}

HTTPResult HTTPClient::post(const char* url, const IHTTPDataOut& dataOut, IHTTPDataIn* pDataIn, int timeout /*= HTTP_CLIENT_DEFAULT_TIMEOUT*/) //Blocking
{
    return connect(url, HTTP_POST, (IHTTPDataOut*)&dataOut, pDataIn, timeout);
}

HTTPResult HTTPClient::put(const char* url, const IHTTPDataOut& dataOut, IHTTPDataIn* pDataIn, int timeout /*= HTTP_CLIENT_DEFAULT_TIMEOUT*/) //Blocking
{
    return connect(url, HTTP_PUT, (IHTTPDataOut*)&dataOut, pDataIn, timeout);
}

HTTPResult HTTPClient::del(const char* url, IHTTPDataIn* pDataIn, int timeout /*= HTTP_CLIENT_DEFAULT_TIMEOUT*/) //Blocking
{
    return connect(url, HTTP_DELETE, nullptr, pDataIn, timeout);
}


int HTTPClient::getHTTPResponseCode()
{
    return m_httpResponseCode;
}

void HTTPClient::setHeader(const char * h)
{
    header = h ;
}

void HTTPClient::dumpReqHeader(bool sw)
{
    dumpReqH = sw ;
}

void HTTPClient::dumpResHeader(bool sw)
{
    dumpResH = sw ;
}

void HTTPClient::setLocationBuf(char * url, int size)
{
    redirect_url = url ;
    redirect_url_size = size ;
}

HTTPResult HTTPClient::setSSLversion(int minorV)
{
     switch(minorV) {
     #if defined(WOLFSSL_ALLOW_SSLV3) && !defined(NO_OLD_TLS)
         case 0 : break ;
     #endif
     #if !defined(NO_OLD_TLS)
         case 1 : break ;
         case 2 : break ;
     #endif
         case 3 : break ;
         default: 
             ERR("Invalid SSL version");
                 return HTTP_CONN;
    }
    SSLver = minorV ;
    return HTTP_OK ;
}

#define CHECK_CONN_ERR(ret) \
  do{ \
    if(ret) { \
      wolfssl_free() ;\
      m_sock.close(); \
      ERR("Connection error (%d)", ret); \
      return HTTP_CONN; \
    } \
  } while(0)

#define PRTCL_ERR() \
  do{ \
    wolfssl_free() ;\
    m_sock.close(); \
    ERR("Protocol error"); \
    return HTTP_PRTCL; \
  } while(0)

#define DUMP_REQ_HEADER(buff) \
    if(dumpReqH)printf("%s", buff) ;
#define DUMP_RES_HEADER(buff) \
    if(dumpResH)printf("%s\n", buff) ;

void HTTPClient::wolfssl_free()
{
    if(ssl) {
        wolfSSL_free(ssl) ;
        ssl = nullptr ;
    }
    if(ctx) {
        wolfSSL_CTX_free(ctx) ;
        ctx = nullptr ;
    }
    (void)wolfSSL_Cleanup() ;
}

HTTPResult HTTPClient::connect(const char* url, HTTP_METH method, IHTTPDataOut* pDataOut, IHTTPDataIn* pDataIn, int timeout) //Execute request
{
    WOLFSSL_METHOD * SSLmethod ;
    m_httpResponseCode = 0; //Invalidate code
    m_timeout = timeout;
    redirect = 0 ;

    pDataIn->writeReset();
    if( pDataOut ) {
        pDataOut->readReset();
    }

    char scheme[8];
    char host[32];
    char path[160];

    int ret ;

    //First we need to parse the url (http[s]://host[:port][/[path]])
    HTTPResult res = parseURL(url, scheme, sizeof(scheme), host, sizeof(host), &port, path, sizeof(path));
    if(res != HTTP_OK) {
        ERR("parseURL returned %d", res);
        return res;
    }

    if(port == 0) {
        if(strcmp(scheme, "http") == 0)
            port = HTTP_PORT ;
        else if(strcmp(scheme, "https") == 0)
            port = HTTPS_PORT ;
        else ;
    }

    DBG("Scheme: %s", scheme);
    DBG("Host: %s", host);
    DBG("Port: %d", port);
    DBG("Path: %s", path);
    if(dumpReqH) (void)printf("\nHTTP Request: %s://%s:%d\n", scheme, host, port) ;
    //Connect
    DBG("Connecting socket to server");

#define MAX_RETRY 5
    int retry ;

    for(retry=0; retry<MAX_RETRY; retry++) {
        int ret = m_sock.connect(host, port);
        if(ret == 0)break ;
    }
    if(retry == MAX_RETRY) {
        m_sock.close();
        ERR("Could not connect");
        return HTTP_CONN;
    }

    if(port == HTTPS_PORT) {
        /* Start SSL connect */
        DBG("SSLver=%d", SSLver) ;
        if(ctx == nullptr) {
            switch(SSLver) {
                #if defined(WOLFSSL_ALLOW_SSLV3) && !defined(NO_OLD_TLS)
                case 0 :
                    SSLmethod = wolfSSLv3_client_method() ;
                    break ;
                #endif
                #if !defined(NO_OLD_TLS)
                case 1 :
                    SSLmethod = wolfTLSv1_client_method() ;
                    break ;
                case 2 :
                    SSLmethod = wolfTLSv1_1_client_method() ;
                    break ;
                #endif
                case 3 :
                    SSLmethod = wolfTLSv1_2_client_method() ;
                    break ;
                default: 
                    ERR("Invalid SSL version");
                    return HTTP_CONN;
            }
            ctx = wolfSSL_CTX_new((WOLFSSL_METHOD *)SSLmethod);
            if (ctx == nullptr) {
                ERR("unable to get ctx");
                return HTTP_CONN;
            }
            wolfSSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr);
            wolfSSL_SetIORecv(ctx, SocketReceive) ;
            wolfSSL_SetIOSend(ctx, SocketSend) ;
        }
        if (ssl == nullptr) {
            ssl = wolfSSL_new(ctx);
            if (ssl == nullptr) {
                ERR("unable to get SSL object");
                wolfssl_free() ;
                return HTTP_CONN;
            }
        }
        wolfSSL_SetIOReadCtx (ssl, (void *)&m_sock) ;
        wolfSSL_SetIOWriteCtx(ssl, (void *)&m_sock) ;
        DBG("ctx=%x, ssl=%x, ssl->ctx->CBIORecv, CBIOSend=%x, %x\n",
            ctx, ssl, SocketReceive, SocketSend ) ;
        if (wolfSSL_connect(ssl) != SSL_SUCCESS) {
            ERR("SSL_connect failed");
            wolfssl_free() ;
            return HTTP_CONN;
        }
    } /* SSL connect complete */

    //Send request
    DBG("Sending request");
    char buf[CHUNK_SIZE];
    send_buf_p = send_buf ; // Reset send buffer ;

    const char* meth = (method==HTTP_GET)?"GET":(method==HTTP_POST)?"POST":(method==HTTP_PUT)?"PUT":(method==HTTP_DELETE)?"DELETE":"";
    (void)snprintf(buf, sizeof(buf), "%s %s HTTP/1.1\r\nHost: %s\r\n", meth, path, host); //Write request
    DUMP_REQ_HEADER(buf) ;
    ret = send(buf);
    if(ret) {
        m_sock.close();
        ERR("Could not write request");
        return HTTP_CONN;
    }

    wait(0.1) ;

    //Send all headers

    //Send default headers
    DBG("Sending headers");
    if(m_basicAuthUser) {
        (void)bAuth() ; /* send out Basic Auth header */
    }
    if( pDataOut != nullptr ) {
        if( pDataOut->getIsChunked() ) {
            ret = send("Transfer-Encoding: chunked\r\n");
            DUMP_REQ_HEADER("Transfer-Encoding: chunked\r\n") ;
            CHECK_CONN_ERR(ret);
        } else {
            (void)snprintf(buf, sizeof(buf), "Content-Length: %zu\r\n", pDataOut->getDataLen());
            DUMP_REQ_HEADER(buf) ;           
            DBG("Content buf:%s", buf) ;
            ret = send(buf);
            CHECK_CONN_ERR(ret);
        }
        char type[48];
        if( pDataOut->getDataType(type, 48) == HTTP_OK ) {
            (void)snprintf(buf, sizeof(buf), "Content-Type: %s\r\n", type);
            DUMP_REQ_HEADER(buf) ;
            ret = send(buf);
            CHECK_CONN_ERR(ret);
        }
    }

    //Add user headers
    if(header) {
        ret = send((const char *)header);
        DUMP_REQ_HEADER(header) ;
        CHECK_CONN_ERR(ret);
    }

    //Close headers
    DBG("Headers sent");
    ret = send("\r\n");
    CHECK_CONN_ERR(ret);

    size_t trfLen;

    //Send data (if available)
    DUMP_REQ_HEADER("\n") ;
    if( pDataOut != nullptr ) {
        DBG("Sending data");
        while(true) {
            size_t writtenLen = 0;
            (void)pDataOut->read(buf, CHUNK_SIZE, &trfLen);
            buf[trfLen] = 0x0 ;
            DBG("buf:%s", buf) ;
            if( pDataOut->getIsChunked() ) {
                //Write chunk header
                char chunkHeader[64];
                (void)snprintf(chunkHeader, sizeof(chunkHeader), "%zX\r\n", trfLen); //In hex encoding
                ret = send(chunkHeader);
                CHECK_CONN_ERR(ret);
            } else if( trfLen == 0 ) {
                DBG("trfLen==0") ;
                break;
            }
            DBG("trfLen 1=%d", trfLen) ;
            if( trfLen != 0 ) {
                DBG("Sending 1") ;
                DUMP_REQ_HEADER(buf) ;
                ret = send(buf, trfLen);
                DBG("Sent 1") ;
                CHECK_CONN_ERR(ret);
            }

            if( pDataOut->getIsChunked()  ) {
                ret = send("\r\n"); //Chunk-terminating CRLF
                CHECK_CONN_ERR(ret);
            } else {
                writtenLen += trfLen;
                if( writtenLen >= pDataOut->getDataLen() ) {
                    DBG("writtenLen=%zu", writtenLen) ;
                    break;
                }
                DBG("writtenLen+=trfLen = %zu", writtenLen) ;
            }
            DBG("trfLen 2=%zu", trfLen) ;
            if( trfLen == 0 ) {
                DBG("trfLen == 0") ;
                break;
            }
        }

    }
    DUMP_REQ_HEADER("\n") ;
    ret = flush() ; // flush the send buffer ;
    CHECK_CONN_ERR(ret);

    //Receive response
    DBG("Receiving response:");

    ret = recv(buf, CHUNK_SIZE - 1, CHUNK_SIZE - 1, &trfLen); //Read n bytes
    CHECK_CONN_ERR(ret);

    buf[trfLen] = '\0';

    char* crlfPtr = strstr(buf, "\r\n");
    if(crlfPtr == nullptr) {
        PRTCL_ERR();
    }

    int crlfPos = crlfPtr - buf;
    buf[crlfPos] = '\0';
    DUMP_RES_HEADER("\nHTTP Response:") ;
    DUMP_RES_HEADER(buf) ;

    //Parse HTTP response
    if( sscanf(buf, "HTTP/%*d.%*d %d %*[^\r\n]", &m_httpResponseCode) != 1 ) {
        //Cannot match string, error
        ERR("Not a correct HTTP answer : %s\n", buf);
        PRTCL_ERR();
    }

    if( (m_httpResponseCode < 200) || (m_httpResponseCode >= 400) ) {
        //Did not return a 2xx code; TODO fetch headers/(&data?) anyway and implement a mean of writing/reading headers
        WARN("Response code %d", m_httpResponseCode);
        PRTCL_ERR();
    }

    DBG("Reading headers");

    (void)memmove(buf, &buf[crlfPos+2], trfLen - (crlfPos + 2) + 1); //Be sure to move NULL-terminating char as well
    trfLen -= (crlfPos + 2);

    size_t recvContentLength = 0;
    bool recvChunked = false;
    //Now get headers
    while( true ) {
        crlfPtr = strstr(buf, "\r\n");
        if(crlfPtr == nullptr) {
            if( trfLen < CHUNK_SIZE - 1 ) {
                size_t newTrfLen;
                ret = recv(buf + trfLen, 1, CHUNK_SIZE - trfLen - 1, &newTrfLen);
                trfLen += newTrfLen;
                buf[trfLen] = '\0';
                DBG("Read %d chars; In buf: [%s]", newTrfLen, buf);
                CHECK_CONN_ERR(ret);
                continue;
            } else {  // Too large header. Skip to the next.
                WARN("Header too large [%20s]. Skip to the next.\n", buf) ;
                while(true) {
                    ret = recv(buf, 1, CHUNK_SIZE-1, &trfLen);
                    buf[trfLen] = '\0' ;
                    crlfPtr = strstr(buf, "\r\n");
                    if(crlfPtr != nullptr) {
                        crlfPos = crlfPtr - buf;
                        (void)memmove(buf, &buf[crlfPos+2], trfLen - (crlfPos + 2) + 1); //Be sure to move NULL-terminating char as well
                        trfLen -= (crlfPos + 2);
                        DBG("Got next header(%d)[%s]", trfLen, buf) ;
                        break ;
                    } else {
                        DBG("Skipped[%s]\n", buf) ;
                        continue ;
                    }
                }
                continue ; // to fill out rest of buff
            }
        }

        crlfPos = crlfPtr - buf;
        DBG("crlfPos=%d", crlfPos) ;
        if(crlfPos == 0) { //End of headers
            DBG("Headers read");
            (void)memmove(buf, &buf[2], trfLen - 2 + 1); //Be sure to move NULL-terminating char as well
            trfLen -= 2;
            break;
        }

        buf[crlfPos] = '\0';
        DUMP_RES_HEADER(buf) ;
        
        char key[32];
        char value[32];

        key[31] = '\0';
        value[31] = '\0';

        int n = sscanf(buf, "%31[^:]: %31[^\r\n]", key, value);
        DBG("Read header(%d) : %s: %s\n", n, key, value);
        if ( n == 2 ) {
            char *k, *v ;
            for(k=key ;   *k != '\0'; k++)*k = toupper(*k) ;
            for(v=value ; *v != '\0'; v++)*v = toupper(*v) ;
            if( !strcmp(key, "CONTENT-LENGTH") ) {
                (void)sscanf(value, "%zd", &recvContentLength);
                pDataIn->setDataLen(recvContentLength);
            } else if( !strcmp(key, "TRANSFER-ENCODING") ) {
                if( !strcmp(value, "CHUNKED") ) {
                    recvChunked = true;
                    pDataIn->setIsChunked(true);
                }
            } else if( !strcmp(key, "CONTENT-TYPE") ) {
                pDataIn->setDataType(value);
            } else if( !strcmp(key, "LOCATION") && redirect_url) {
                (void)sscanf(buf, "%31[^:]: %128[^\r\n]", key, redirect_url);
                DBG("Redirect %s: %s", key, redirect_url) ;
                redirect = 1 ;
            }
            else ;
            (void)memmove(buf, &buf[crlfPos+2], trfLen - (crlfPos + 2) + 1); //Be sure to move NULL-terminating char as well
            trfLen -= (crlfPos + 2);
            DBG("next header(trfLen:%d)[%s]", trfLen, buf) ;
        } else {
            ERR("Could not parse header");
            PRTCL_ERR();
        }

    }

    //Receive data
    DBG("Receiving data");

    while(true) {
        size_t readLen = 0;

        if( recvChunked ) {
            //Read chunk header
            bool foundCrlf;
            do {
                foundCrlf = false;
                crlfPos=0;
                buf[trfLen]=0;
                if(trfLen >= 2) {
                    crlfPtr = strstr(buf, "\r\n") ;
                    if(crlfPtr != nullptr){
                        foundCrlf = true;
                        crlfPos = crlfPtr - buf;
                        break ; 
                    }
                    /*for(; crlfPos < trfLen - 2; crlfPos++) {
                        if( buf[crlfPos] == '\r' && buf[crlfPos + 1] == '\n' ) {
                            foundCrlf = true;
                            break;
                        }
                    }*/
                }
                if(!foundCrlf) { //Try to read more
                    if( trfLen < CHUNK_SIZE ) {
                        size_t newTrfLen;
                        ret = recv(buf + trfLen, 0, CHUNK_SIZE - trfLen - 1, &newTrfLen);
                        trfLen += newTrfLen;
                        CHECK_CONN_ERR(ret);
                        continue;
                    } else {
                        PRTCL_ERR();
                    }
                }
            } while(!foundCrlf);
            buf[crlfPos] = '\0';
            if(((buf[crlfPos-2] == 0x0a) && (buf[crlfPos-1] == 0x0a))){
                WARN("null chunck\n") ;
                readLen = 0 ;
            } else {
                int n = sscanf(buf, "%zx", &readLen);
                if(n!=1) {
                    ERR("Could not read chunk length:%02x,%02x,%02x,%02x,\"%s\"", 
                    buf[crlfPos-4],buf[crlfPos-3],buf[crlfPos-2],buf[crlfPos-1],buf);
                    PRTCL_ERR();
                }
            }
            (void)memmove(buf, &buf[crlfPos+2], trfLen - (crlfPos + 2)); //Not need to move NULL-terminating char any more
            trfLen -= (crlfPos + 2);

            if( readLen == 0 ) {
                //Last chunk
                break;
            }
        } else {
            readLen = recvContentLength;
        }

        DBG("Retrieving %zu bytes", readLen);

        do {
            (void)pDataIn->write(buf, MIN(trfLen, readLen));
            if( trfLen > readLen ) {
                (void)memmove(buf, &buf[readLen], trfLen - readLen);
                trfLen -= readLen;
                readLen = 0;
            } else {
                readLen -= trfLen;
            }

            if(readLen) {
                ret = recv(buf, 1, CHUNK_SIZE - trfLen - 1, &trfLen);
                CHECK_CONN_ERR(ret);
            }
        } while(readLen);

        if( recvChunked ) {
            if(trfLen < 2) {
                size_t newTrfLen;
                //Read missing chars to find end of chunk
                ret = recv(buf + trfLen, 2 - trfLen, CHUNK_SIZE - trfLen - 1, &newTrfLen);
                CHECK_CONN_ERR(ret);
                trfLen += newTrfLen;
            }
            if(strcmp(buf, "\r\n") == 0) {
                WARN("Null Chunck 2\n") ;
                break ;
            }
            (void)memmove(buf, &buf[2], trfLen - 2);
            trfLen -= 2;
        } else {
            break;
        }

    }
    wolfssl_free() ;
    m_sock.close();
    DBG("Completed HTTP transaction");
    if(redirect)return HTTP_REDIRECT ;
    else        return HTTP_OK;
}

HTTPResult HTTPClient::recv(char* buf, size_t minLen, size_t maxLen, size_t* pReadLen)   //0 on success, err code on failure
{
    DBG("Trying to read between %d and %d bytes", minLen, maxLen);
    size_t readLen = 0;
    maxLen = maxLen == 0 ? 1 : maxLen ;
    if(!m_sock.is_connected()) {
        WARN("Connection was closed by server");
        return HTTP_CLOSED; //Connection was closed by server
    }

    int ret;

    if(port == HTTPS_PORT) {
        DBG("Enter wolfSSL_read") ;

        m_sock.set_blocking(false, m_timeout);
        readLen = wolfSSL_read(ssl, buf, maxLen);
        if (readLen > 0) {
            buf[readLen] = 0;
            DBG("wolfSSL_read:%s\n", buf);
        } else {
            ERR("wolfSSL_read, ret = %zu", readLen) ;
            return HTTP_ERROR ;
        }
        DBG("Read %d bytes", readLen);
        *pReadLen = readLen;
        return HTTP_OK;
    }

    while(readLen < maxLen) {
        if(readLen < minLen) {
            DBG("Trying to read at most %d bytes [Blocking]", minLen - readLen);
            m_sock.set_blocking(false, m_timeout);
            ret = m_sock.receive_all(buf + readLen, minLen - readLen);
        } else {
            DBG("Trying to read at most %d bytes [Not blocking]", maxLen - readLen);
            m_sock.set_blocking(false, 0);
            ret = m_sock.receive(buf + readLen, maxLen - readLen);
        }

        if( ret > 0) {
            readLen += ret;
        } else if( ret == 0 ) {
            break;
        } else {
            if(!m_sock.is_connected()) {
                ERR("Connection error (recv returned %d)", ret);
                *pReadLen = readLen;
                return HTTP_CONN;
            } else {
                break;
            }
        }

        if(!m_sock.is_connected()) {
            break;
        }
    }
    DBG("Read %d bytes", readLen);
    *pReadLen = readLen;
    return HTTP_OK;
}

HTTPResult HTTPClient::send(const char* buf, size_t len)   //0 on success, err code on failure
{
    HTTPResult ret ;
    int cp_len ;

    if(len == 0) {
        len = strlen(buf);
    }

    do {

        if(static_cast<unsigned int>(SEND_BUF_SIZE - (send_buf_p - send_buf)) >= len) {
            cp_len = len ;
        } else {
            cp_len = SEND_BUF_SIZE - (send_buf_p - send_buf) ;
        }
        DBG("send_buf_p:%s. send_buf+SIZE:%s, len=%d, cp_len=%d", send_buf_p, send_buf+SEND_BUF_SIZE, len, cp_len) ;
        (void)memcpy(send_buf_p, buf, cp_len) ;
        send_buf_p += cp_len ;
        len -= cp_len ;

        if(send_buf_p == send_buf + SEND_BUF_SIZE) {
            if(port == HTTPS_PORT) {
                ERR("HTTPClient::send buffer overflow");
                return HTTP_ERROR ;
            }
            ret = flush() ;
            if(ret)return(ret) ;
        }
    } while(len) ;
    return HTTP_OK ;
}

HTTPResult HTTPClient::flush()   //0 on success, err code on failure
{
    int len ;
    char * buf ;

    buf = send_buf ;
    len = send_buf_p - send_buf ;
    send_buf_p = send_buf ; // reset send buffer

    DBG("Trying to write %d bytes:%s\n", len, buf);
    size_t writtenLen = 0;

    if(!m_sock.is_connected()) {
        WARN("Connection was closed by server");
        return HTTP_CLOSED; //Connection was closed by server
    }

    if(port == HTTPS_PORT) {
        DBG("Enter wolfSSL_write") ;
        if (wolfSSL_write(ssl, buf, len) != len) {
            ERR("SSL_write failed");
            return HTTP_ERROR ;
        }
        DBG("Written %d bytes", writtenLen);
        return HTTP_OK;
    }
    m_sock.set_blocking(false, m_timeout);
    int ret = m_sock.send_all(buf, len);
    if(ret > 0) {
        writtenLen += ret;
    } else if( ret == 0 ) {
        WARN("Connection was closed by server");
        return HTTP_CLOSED; //Connection was closed by server
    } else {
        ERR("Connection error (send returned %d)", ret);
        return HTTP_CONN;
    }

    DBG("Written %d bytes", writtenLen);
    return HTTP_OK;
}

HTTPResult HTTPClient::parseURL(const char* url, char* scheme, size_t maxSchemeLen, char* host, size_t maxHostLen, uint16_t* port, char* path, size_t maxPathLen)   //Parse URL
{
    char* schemePtr = (char*) url;
    char* hostPtr = (char*) strstr(url, "://");
    if(hostPtr == nullptr) {
        WARN("Could not find host");
        return HTTP_PARSE; //URL is invalid
    }

    if( maxSchemeLen < static_cast<unsigned int>(hostPtr - schemePtr + 1)) { //including NULL-terminating char
        WARN("Scheme str is too small (%zu >= %d)", maxSchemeLen, hostPtr - schemePtr + 1);
        return HTTP_PARSE;
    }
    (void)memcpy(scheme, schemePtr, hostPtr - schemePtr);
    scheme[hostPtr - schemePtr] = '\0';

    hostPtr+=3;

    size_t hostLen = 0;

    char* portPtr = strchr(hostPtr, ':');
    if( portPtr != nullptr ) {
        hostLen = portPtr - hostPtr;
        portPtr++;
        if( sscanf(portPtr, "%hu", port) != 1) {
            WARN("Could not find port");
            return HTTP_PARSE;
        }
    } else {
        *port=0;
    }
    char* pathPtr = strchr(hostPtr, '/');
    if( hostLen == 0 ) {
        hostLen = pathPtr - hostPtr;
    }

    if( maxHostLen < hostLen + 1 ) { //including NULL-terminating char
        WARN("Host str is too small (%zu >= %u)", maxHostLen, hostLen + 1);
        return HTTP_PARSE;
    }
    (void)memcpy(host, hostPtr, hostLen);
    host[hostLen] = '\0';

    size_t pathLen;
    char* fragmentPtr = strchr(hostPtr, '#');
    if(fragmentPtr != nullptr) {
        pathLen = fragmentPtr - pathPtr;
    } else {
        pathLen = strlen(pathPtr);
    }

    if( maxPathLen < pathLen + 1 ) { //including NULL-terminating char
        WARN("Path str is too small (%zu >= %u)", maxPathLen, pathLen + 1);
        return HTTP_PARSE;
    }
    (void)memcpy(path, pathPtr, pathLen);
    path[pathLen] = '\0';

    return HTTP_OK;
}

HTTPResult HTTPClient::bAuth()
{
    HTTPResult ret ;
    char b_auth[(int)((AUTHB_SIZE+3)*4/3+1)] ;
    char base64buff[AUTHB_SIZE+3] ;

    ret = send("Authorization: Basic ") ;
    DUMP_REQ_HEADER("Authorization: Basic ") ;
    CHECK_CONN_ERR(ret);
    (void)sprintf(base64buff, "%s:%s", m_basicAuthUser, m_basicAuthPassword) ;
    DUMP_REQ_HEADER(base64buff) ;
    DBG("bAuth: %s", base64buff) ;
    base64enc(b_auth, base64buff) ;
    b_auth[strlen(b_auth)+1] = '\0' ;
    b_auth[strlen(b_auth)] = '\n' ;
    DBG("b_auth:%s", b_auth) ;
    ret = send(b_auth) ;
    CHECK_CONN_ERR(ret);
    return HTTP_OK ;
}
