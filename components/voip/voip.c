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

//#include "ping/ping_sock.h"
#include "lwip/sockets.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"

//#include "ping_init.h"
#include "voip.h"
#include "md5_digest.h"

static const char *TAG = "voip";

#define SRC_PORT 65300
#define SIP_PORT 5060
#define BUF_LEN 750

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

const char * reg =
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
  "Content-Length: %s\n\n";

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

/*
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
*/

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

int send_reg(int sock, struct sockaddr_in * sip_sockaddr, char * buf, int buf_len, char * external_ip_str) {

  // Send the initial REGISTER request
  int send_len = snprintf(buf, buf_len, reg,
      "sip:chicago1.voip.ms",
      external_ip_str,
      "65300",
      "z9hG4bKPjeac04633294241c78541dba3d8c5461b",
      "sip:444138@chicago1.voip.ms",
      "263df7334c424c518c978cf068c661f5",
      "sip:444138@chicago1.voip.ms",
      "263df7334c424c518c978cf068c661f5",
      "sip:444138",
      external_ip_str,
      "65300");

  if (send_len >= buf_len) {
    ESP_LOGW(TAG, "Warning: message was truncated to %d-byte length\n", buf_len);
    buf[buf_len - 1] = '\0';
  }
  else {
    buf[send_len] = '\0';
  }

  size_t socklength = sizeof(*sip_sockaddr);
  int err = sendto(sock, buf, send_len, 0, (struct sockaddr *)sip_sockaddr, socklength);
  if (err < 0) {
    ESP_LOGE(TAG, "sendto error");
    close(sock);
    return 1;
  }

  int rec_len = recvfrom(sock, buf, buf_len - 1, 0, (struct sockaddr *)sip_sockaddr, (socklen_t *)&socklength);
  ESP_LOGI(TAG, "Received %d bytes\n", rec_len);
  if (rec_len > buf_len) {
    buf[buf_len - 1] = '\0';
  }
  else if (rec_len > 0) {
    buf[rec_len] = '\0';
  }
  ESP_LOGI(TAG, "response: %s\n", buf);

  // Check the type of the response (if 401 UNAUTHORIZED, re-authenticate)
  char * code_str = strchr(buf, ' ') + 1;
  char msg_code[4] = {'\0'};
  strncpy(msg_code, code_str, 3);
  if (!strcmp(msg_code, "200")) {
    // OK, no need to authenticate
    return 0;
  }
  else if (strcmp(msg_code, "401")) {
    // message code is something other than 401 UNAUTHORIZED, return error
    ESP_LOGE(TAG, "Error: message code = %s\n", msg_code);
    return 2;
  }

  // Message code was 401 UNAUTHORIZED, authenticate with digest

  char * auth_ptr = strstr(buf, "WWW-Authenticate");
  if (auth_ptr == NULL) {
    ESP_LOGE(TAG, "Error: auth field not found\n");
    close(sock);
    return 1;
  }

  char * da_str = "algorithm=";
  auth_ptr = strstr(auth_ptr, da_str);
  if (auth_ptr == NULL) {
    return 3;
  }

  auth_ptr += strlen(da_str);
  if (strncmp(auth_ptr, "MD5", 3)) {
    return 4;
  }

  char * match_str = "realm=\"";
  char * realm = strstr(auth_ptr, match_str) + strlen(match_str);

  match_str = "nonce=\"";
  char * nonce = strstr(realm, match_str) + strlen(match_str);

  char * digest_uri = "sip:chicago1.voip.ms";
  char * method = "REGISTER";

  char digest_response[2 * MD5_DIGEST_LENGTH + 1];
  digest_response[2 * MD5_DIGEST_LENGTH] = '\0';

  err = get_digest(digest_response, auth_ptr, username, password, digest_uri, realm, nonce, method);
  if (err == 1) {
    return 1;
  }
  if (err == 2) {
    return 1;
  }

  send_len = snprintf(buf, buf_len, auth_reg,
      "sip:chicago1.voip.ms",
      external_ip_str,
      "65300",
      "z9hG4bKPjeac04633294241c78541dba3d8c5461b",
      "sip:444138@chicago1.voip.ms",
      "263df7334c424c518c978cf068c661f5",
      "sip:444138@chicago1.voip.ms",
      "263df7334c424c518c978cf068c661f5",
      "sip:444138",
      external_ip_str,
      "65300",
      username,
      realm,
      nonce,
      digest_uri,
      digest_response,
      "MD5");

  if (send_len >= buf_len) {
    ESP_LOGW(TAG, "Warning: message was truncated to %d-byte length\n", buf_len);
    buf[buf_len - 1] = '\0';
  }
  else {
    buf[send_len] = '\0';
  }

  err = sendto(sock, buf, send_len, 0, (struct sockaddr *)sip_sockaddr, socklength);
  if (err < 0) {
    ESP_LOGE(TAG, "sendto error");
    return 1;
  }

  rec_len = recvfrom(sock, buf, buf_len - 1, 0, (struct sockaddr *)sip_sockaddr, (socklen_t *)&socklength);
  ESP_LOGI(TAG, "Received %d bytes\n", rec_len);

  return 0;
}

int send_invite(int sock, struct sockaddr_in * sip_sockaddr, char * buf, int buf_len, char * external_ip_str) {

  int send_len = snprintf(buf, buf_len, sdp_str,
      "0",
      external_ip_str,
      "my_session");
  buf[send_len] = '\0';

  char len_as_str[10] = {'\0'};
  snprintf(len_as_str, 10, "%d", send_len);

  send_len = snprintf(buf, buf_len, invite,
      call_number,
      external_ip_str,
      "65300",
      "z9hG4bKPjfc7170ddca184cc38e1edaac48207ce7",
      username,
      "f2e78bb18e0f460e8724e9983319e1f3",
      call_number,
      "6948ac89686744c8b37f0ba3ba175e1b",
      username,
      external_ip_str,
      "65300",
      "3011",
      len_as_str);

  send_len += snprintf(buf + send_len, buf_len - send_len, sdp_str,
      "0",
      external_ip_str,
      "my_session");

  if (send_len >= buf_len) {
    ESP_LOGW(TAG, "Warning: message was truncated to %d-byte length\n", buf_len);
    buf[buf_len - 1] = '\0';
  }
  else {
    buf[send_len] = '\0';
  }

  socklen_t socklength = sizeof(*sip_sockaddr);
  int err = sendto(sock, buf, send_len, 0, (struct sockaddr *)sip_sockaddr, socklength);
  if (err < 0) {
    ESP_LOGE(TAG, "sendto error");
    return 1;
  }

  int rec_len = recvfrom(sock, buf, buf_len - 1, 0, (struct sockaddr *)sip_sockaddr, &socklength);
  ESP_LOGI(TAG, "Received %d bytes\n", rec_len);
  if (rec_len > buf_len) {
    buf[buf_len - 1] = '\0';
  }
  else if (rec_len > 0) {
    buf[rec_len] = '\0';
  }

  char * auth_ptr = strstr(buf, "WWW-Authenticate");
  if (auth_ptr == NULL) {
    ESP_LOGE(TAG, "Error: auth field not found\n");
    return 1;
  }

  char * da_str = "algorithm=";
  auth_ptr = strstr(auth_ptr, da_str);
  if (auth_ptr == NULL) {
    return 1;
  }

  auth_ptr += strlen(da_str);
  if (strncmp(auth_ptr, "MD5", 3)) {
    return 2;
  }

  char * match_str = "realm=\"";
  char * realm = strstr(auth_ptr, match_str) + strlen(match_str);

  match_str = "nonce=\"";
  char * nonce = strstr(realm, match_str) + strlen(match_str);

  char * method = "INVITE";

  char digest_response[2 * MD5_DIGEST_LENGTH + 1];
  digest_response[2 * MD5_DIGEST_LENGTH] = '\0';

  char * digest_uri = "sip:chicago1.voip.ms";

  err = get_digest(digest_response, auth_ptr, username, password, digest_uri, realm, nonce, method);
  if (err == 1) {
    ESP_LOGE(TAG, "Error: digest algorithm not found\n");
    return 1;
  }
  if (err == 2) {
    ESP_LOGE(TAG, "Error: algorithm type not supported\n");
    return 1;
  }

  ESP_LOGI(TAG, "final hash: %s\n", digest_response);

  send_len = snprintf(buf, buf_len, auth_invite,
      call_number,
      external_ip_str,
      "65300",
      "z9hG4bKPjfc7170ddca184cc38e1edaac48207ce7",
      username,
      "f2e78bb18e0f460e8724e9983319e1f3",
      call_number,
      "6948ac89686744c8b37f0ba3ba175e1b",
      username,
      external_ip_str,
      "65300",
      "3012",
      username,
      realm,
      nonce,
      digest_uri,
      digest_response,
      "MD5",
      len_as_str); // NOTE: this should be incorrect but works anyways -- auth message gets no SDP body

  //send_len += snprintf(buf + send_len, buf_len - send_len, sdp_str,
  //    "0",
  //    external_ip_str,
  //    "my_session");

  if (send_len >= buf_len) {
    ESP_LOGW(TAG, "Warning: message was truncated to %d-byte length\n", buf_len);
    buf[buf_len - 1] = '\0';
  }
  else {
    buf[send_len] = '\0';
  }

  ESP_LOGI(TAG, "response:\n%s", buf);

  err = sendto(sock, buf, send_len, 0, (struct sockaddr *)sip_sockaddr, socklength);
  if (err < 0) {
    ESP_LOGE(TAG, "sendto error");
    return 1;
  }

  rec_len = recvfrom(sock, buf, buf_len - 1, 0, (struct sockaddr *)sip_sockaddr, &socklength);
  ESP_LOGI(TAG, "Received %d bytes\n", rec_len);
  if (rec_len > buf_len) {
    buf[buf_len - 1] = '\0';
  }
  else if (rec_len > 0) {
    buf[rec_len] = '\0';
  }
  ESP_LOGI(TAG, "response:\n%s", buf);

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

  struct hostent * sip_host = gethostbyname(hostname);
  if (sip_host->h_addrtype != AF_INET) {
    ESP_LOGE(TAG, "Error: address type not IPv4\n");
    return;
  }

  char * host_addr_str = inet_ntoa(*(struct in_addr *)sip_host->h_addr);
  ESP_LOGI(TAG, "IP address: %s\n", host_addr_str);

  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    ESP_LOGE(TAG, "Error: unable to create socket\n");
    return;
  }

  struct timeval timeout;
  timeout.tv_sec = 10;
  timeout.tv_usec = 0;
  setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

  struct sockaddr_in src_addr;
  memset(&src_addr, 0, sizeof(src_addr));
  src_addr.sin_family = AF_INET;
  src_addr.sin_addr.s_addr = INADDR_ANY;
  src_addr.sin_port = htons(SRC_PORT);
  err = bind(sock, (struct sockaddr *)&src_addr, sizeof(src_addr));
  if (err < 0) {
    ESP_LOGE(TAG, "Error: unable to bind socket\n");
    return;
  }

  struct sockaddr_in sip_sockaddr;
  memset(&sip_sockaddr, 0, sizeof(struct sockaddr_in));
  sip_sockaddr.sin_family = AF_INET;
  sip_sockaddr.sin_port = htons(SIP_PORT);
  memcpy(&sip_sockaddr.sin_addr, sip_host->h_addr, sizeof(struct in_addr));

  char buf[BUF_LEN];

  err = send_reg(sock, &sip_sockaddr, buf, BUF_LEN, external_ip_str);
  if (err) {
    ESP_LOGE(TAG, "Error: send_reg failed with return value %d\n", err);
    return;
  }

  // parse response, calculate auth response and re-send
  // NOTE: assuming the first response will be of type 401 Unauthorized

  // Client is properly authenticated with the server, send invite to phone number
  // Response will be 401 Unauthorized, send ACK then calculate MD5 sum and re-send with updated digest hash

  // Send initial invite message (unauthorized)
  err = send_invite(sock, &sip_sockaddr, buf, BUF_LEN, external_ip_str);
  if (err) {
    ESP_LOGE(TAG, "Error: send_invite failed with return value %d\n", err);
    return;
  }

  close(sock);

}
