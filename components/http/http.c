#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "errno.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "http.h"


#define TAG "http_client"


esp_err_t http_client_get(char *host, char *port, char *path, stream_reader_cb callback)
{
    const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res;
    struct in_addr *addr;

    int err = getaddrinfo(host, port, &hints, &res);
    if(err != ESP_OK || res == NULL) {
        ESP_LOGE(TAG, "DNS lookup failed err=%d res=%p", err, res);
        return err;
    }

    // print resolved IP
    addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
    ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", inet_ntoa(*addr));

    // allocate socket
    int sock = socket(res->ai_family, res->ai_socktype, 0);
    if(sock < 0) {
        ESP_LOGE(TAG, "... Failed to allocate socket.");
        freeaddrinfo(res);
    }
    ESP_LOGI(TAG, "... allocated socket\r\n");


    // connect
    if(connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
        ESP_LOGE(TAG, "... socket connect failed errno=%d", errno);
        close(sock);
        freeaddrinfo(res);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "... connected");
    freeaddrinfo(res);

    // write http request
    char *request;
    if(asprintf(&request, "%s%s%s%s%s", "GET ", path, " HTTP/1.0\r\nHost: ", host, "\r\n\r\n") < 0)
    {
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "requesting %s", request);

    if (write(sock, request, strlen(request)) < 0) {
        ESP_LOGE(TAG, "... socket send failed");
        close(sock);
    }

    free(request);
    ESP_LOGI(TAG, "... socket send success");


    /* Read HTTP response */
    char recv_buf[64];
    bzero(recv_buf, sizeof(recv_buf));
    ssize_t numBytes;

    esp_err_t cont = ESP_OK;
    do {
        numBytes = read(sock, recv_buf, sizeof(recv_buf)-1);
        // ESP_LOGI(TAG, "received %d bytes", sizeof(recv_buf));
        cont = (*callback)(recv_buf, numBytes);
    } while(numBytes > 0 && cont == ESP_OK);

    ESP_LOGI(TAG, "... done reading from socket. Last read return=%d errno=%d\r\n", numBytes, errno);
    close(sock);
    ESP_LOGI(TAG, "socket closed");

    return ESP_OK;
}
