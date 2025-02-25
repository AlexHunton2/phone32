/* ping.c - initialize_ping */

#ifndef PING_H
#define PING_H

#include "freertos/FreeRTOS.h"
#include "ping/ping_sock.h"
#include "esp_log.h"
#include "lwip/netdb.h"

#include "ping_init.h"

static const char *TAG = "ping init";

void initialize_ping(
    char * target_addr_str,
    void (*func_success)(esp_ping_handle_t hdl, void *args),
    void (*func_timeout)(esp_ping_handle_t hdl, void *args),
    void (*func_end)(esp_ping_handle_t hdl, void *args)
    ) {
  esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
  ping_config.count = 4;

  // get the IP address
  ip_addr_t target_addr;
  struct addrinfo hint;
  struct addrinfo *res = NULL;
  memset(&hint, 0, sizeof(hint));
  memset(&target_addr, 0, sizeof(target_addr));

  if (getaddrinfo(target_addr_str, NULL, &hint, &res) != 0) {
    ESP_LOGI(TAG, "ping: unknown host %s\n", target_addr_str);
    return;
  }

  // skipping checking if ai_family == AF_INET
  struct in_addr addr4 = ((struct sockaddr_in *) (res->ai_addr))->sin_addr;
  inet_addr_to_ip4addr(ip_2_ip4(&target_addr), &addr4);
  freeaddrinfo(res);

  ping_config.target_addr = target_addr;

  //int count = 0;
  esp_ping_callbacks_t cbs;
  cbs.on_ping_success = func_success;
  cbs.on_ping_timeout = func_timeout;
  cbs.on_ping_end = func_end;
  cbs.cb_args = NULL;

  esp_ping_handle_t ping;
  ESP_LOGI(TAG, "ping target_addr: %s\n", inet_ntoa(ping_config.target_addr.u_addr.ip4));
  esp_ping_new_session(&ping_config, &cbs, &ping);
  esp_ping_start(ping);

  return;
}

#endif
