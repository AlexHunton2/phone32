/* WiFi station Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_eap_client.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_random.h"

#include "ping/ping_sock.h"
#include "lwip/sockets.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"

#include "ping_init.h"
#include "voip.h"
#include "md5_digest.h"

static const char *TAG = "voip";

#define SRC_PORT 65300
#define SIP_PORT 5060
#define BUF_LEN 1024

#define STUN_PORT 3478 // default STUN server port
#define STUN_ADDR "74.125.250.129" // stun.l.google.com
#define STUN_BINDING_REQUEST 0x0001
#define STUN_MAGIC_COOKIE 0x2112A442
#define STUN_SUCCESS 0x0101

struct stun_msg_hdr {
  uint16_t msg_type;
  uint16_t msg_length;
  uint32_t magic_cookie;
  uint32_t transaction_id[3];
};

char * reg =
  "REGISTER %s SIP/2.0\n"
  "Via: SIP/2.0/UDP %s:%s;branch=%s\n"
  "From: <%s>;tag=%s\n"
  "To: <%s>\n"
  "Call-ID: %s\n"
  "Cseq: 1 REGISTER\n"
  "Contact: %s@%s:%s\n"
  "Max-Forwards: 70\n"
  "Expires: 3600\n"
  "Content-Length: 0\n\n";

const char * auth_reg =
  "REGISTER %s SIP/2.0\n"
  "Via: SIP/2.0/UDP %s:%s;branch=%s\n"
  "From: <%s>;tag=%s\n"
  "To: <%s>\n"
  "Call-ID: %s\n"
  "Cseq: 1 REGISTER\n"
  "Contact: %s@%s:%s\n"
  "Max-Forwards: 70\n"
  "Expires: 3600\n"
  "Authorization: Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", response=\"%s\", algorithm=%s\n"
  "Content-Length: 0\n\n";

const char * invite =
  "INVITE sip:%s@chicago.voip.ms SIP/2.0\n"
  "Via: SIP/2.0/UDP %s:%s;branch=%s\n"
  "From: <sip:%s@chicago1.voip.ms>;tag=%s\n"
  "To: <sip:%s@chicago1.voip.ms>\n"
  "Call-ID: %s\n"
  "Contact: <sip:%s@%s:%s;ob>\n"
  "CSeq: %s INVITE\n"
  "Content-Type: application/sdp\n"
  "Content-Length: %s\n\n"
  "%s";

const char * auth_invite =
  "INVITE sip:%s@chicago.voip.ms SIP/2.0\n"
  "Via: SIP/2.0/UDP %s:%s;branch=%s\n"
  "From: <sip:%s@chicago1.voip.ms>;tag=%s\n"
  "To: <sip:%s@chicago1.voip.ms>\n"
  "Call-ID: %s\n"
  "Contact: <sip:%s@%s:%s;ob>\n"
  "CSeq: %s INVITE\n"
  "Content-Type: application/sdp\n"
  "Authorization: Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", response=\"%s\", algorithm=%s\n"
  "Content-Length: %s\n\n";

const char * sdp_str =
  "v=%s\n"
  "o=- 3949402524 3949402524 IN IP4 %s\n"
  "s=%s\n"
  "t=0 0\n"
  "m=audio 4006 RTP/AVP 8 0 101\n";

const char * hostname = CONFIG_VOIP_HOSTNAME;
const char * call_number = CONFIG_VOIP_CALL_NUMBER;
const char * username = CONFIG_VOIP_USERNAME;
const char * password = CONFIG_VOIP_PASSWORD;

static void on_ping_success(esp_ping_handle_t hdl, void *args) {
  ESP_LOGI(TAG, "ping success");
}

static void on_ping_timeout(esp_ping_handle_t hdl, void *args) {
  ESP_LOGI(TAG, "ping timeout");
}

static void on_ping_end(esp_ping_handle_t hdl, void *args) {
  ESP_LOGI(TAG, "ping end");
  esp_ping_delete_session(hdl);
}

int get_external_ip(char * ip_str) {

  int sock;
  struct sockaddr_in server_addr;
  struct sockaddr_in in_addr;
  struct stun_msg_hdr stun_request;

  sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
  if (sock < 0) {
    return 1;
  }

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(STUN_PORT);
  //inet_pton(AF_INET, STUN_ADDR, &server_addr.sin_addr);
  server_addr.sin_addr.s_addr = inet_addr(STUN_ADDR);

  memset(&in_addr, 0, sizeof(in_addr));
  in_addr.sin_family = AF_INET;
  in_addr.sin_port = htons(STUN_PORT);
  in_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  bind(sock, (struct sockaddr *)&in_addr, sizeof(in_addr));

  struct timeval timeout;
  timeout.tv_sec = 10;
  timeout.tv_usec = 0;
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

  int rand_96[3];
  int i;
  for (i = 0; i < 3; i++) {
    rand_96[i] = (uint32_t)esp_random();
  }

  memset(&stun_request, 0, sizeof(stun_request));
  stun_request.msg_type = htons(STUN_BINDING_REQUEST);
  stun_request.magic_cookie = htonl(STUN_MAGIC_COOKIE);
  memcpy(&stun_request.transaction_id, rand_96, sizeof(rand_96));

  char * ip_tmp = inet_ntop(AF_INET, &server_addr.sin_addr, ip_str, INET_ADDRSTRLEN);
  int port_tmp = ntohs(server_addr.sin_port);
  ESP_LOGI(TAG, "IP: %s", ip_tmp);
  ESP_LOGI(TAG, "Port: %d", port_tmp);

  int err = sendto(sock, &stun_request, sizeof(stun_request), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));

  if (err < 0) {
    perror("sendto");
    close(sock);
    return 2;
  }

  char buf[128];
  socklen_t socklen = sizeof(server_addr);
  ssize_t rec_len = recvfrom(sock, buf, 128, 0, (struct sockaddr *)&server_addr, &socklen);

  if (rec_len < sizeof(struct stun_msg_hdr)) {
    return 3;
  }

  struct stun_msg_hdr * tmp = (struct stun_msg_hdr *)buf;
  if (tmp->msg_type != htons(STUN_SUCCESS)) {
    return 3;
  }

  uint32_t * ip_response = (uint32_t *)(buf + rec_len - 4);
  *ip_response = tmp->magic_cookie ^ *ip_response;

  const char * ret = inet_ntop(AF_INET, ip_response, ip_str, INET_ADDRSTRLEN);
  if (ret == NULL) {
    return 4;
  }

  close(sock);
  return 0;
}

void run_voip(void) {

  //initialize_ping("10.0.17.158", on_ping_success, on_ping_timeout, on_ping_end);
  //return;

  // get external ip
  char external_ip_str[INET_ADDRSTRLEN];
  int err = get_external_ip(external_ip_str);
  switch (err) {
    case 1:
      ESP_LOGE(TAG, "external_ip socket error");
      return;
    case 2:
      ESP_LOGE(TAG, "external_ip sendto error");
      return;
    case 3:
      ESP_LOGE(TAG, "external_ip packet format error");
      return;
    case 4:
      ESP_LOGE(TAG, "external_ip inet_ntop error");
      return;
  }
  ESP_LOGI(TAG, "External IP: %s", external_ip_str);
  return;

    // create socket to use
    struct sockaddr_in dest_addr_ip4;
    dest_addr_ip4.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr_ip4.sin_family = AF_INET;
    dest_addr_ip4.sin_port = htons(65300);

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
      ESP_LOGE(TAG, "Error: unable to create socket");
      return;
    }

    ESP_LOGI(TAG, "Socket created");

    struct timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 10;
    setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

    err = bind(sock, (struct sockaddr *)&dest_addr_ip4, sizeof(dest_addr_ip4));
    if (err < 0) {
      ESP_LOGE(TAG, "Error: unable to bind socket");
      return;
    }

    ESP_LOGI(TAG, "Socket bound, port %d", ntohs(dest_addr_ip4.sin_port));

    // register with SIP server (twice, handle nonce)
    // -- should also initialize task to send NAT keepalive messages at an interval

    // invite phone number specified in Kconfig (handle 401 unauthorized, respond with ACK and another INVITE

    // handle state from here (trying, ringing, ...)

}
