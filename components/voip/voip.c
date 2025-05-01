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
#include "freertos/semphr.h"

//#include "ping/ping_sock.h"
#include "lwip/sockets.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"

//#include "ping_init.h"
#include "voip.h"
#include "md5_digest.h"
#include "rtp.h"

static const char *TAG = "voip";

#define SRC_PORT 65300
#define SIP_PORT 5060
#define BUF_LEN 750

#define STUN_PORT 3478 // default STUN server port
#define STUN_ADDR "74.125.250.129" // stun.l.google.com
#define STUN_BINDING_REQUEST 0x0001
#define STUN_MAGIC_COOKIE 0x2112A442
#define STUN_SUCCESS 0x0101

#define RTP_PORT 4000
#define RTP_BUF_COUNT 3


struct stun_msg_hdr {
  uint16_t msg_type;
  uint16_t msg_length;
  uint32_t magic_cookie;
  uint32_t transaction_id[3];
};

struct sip_msg {
  char * branch;
  char * tag_from;
  char * tag_to;
  char * call_id;
  char * cseq;
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

const char * ack_str =
  "ACK %s SIP/2.0\n"
  "Via: SIP/2.0/UDP %s:%s;branch=%s\n"
  "From: <sip:%s@chicago1.voip.ms>;tag=%s\n"
  "To: <sip:%s@chicago1.voip.ms>;tag=%s\n"
  "CSeq: %s ACK\n"
  "Call-ID: %s\n"
  "Content-Length: 0\n\n";

const char * sdp_str =
  "v=%s\n"
  "o=- 3949402524 3949402524 IN IP4 %s\n"
  "s=%s\n"
  "t=0 0\n"
  "c=IN IP4 %s\n"
  "m=audio 4000 RTP/AVP 0\n"
  "a=rtpmap:0 PCMU/8000\n";

const char * ok_str =
  "SIP/2.0 200 OK\n"
  "Via: %s\n"
  "From: %s\n"
  "To: %s\n"
  "Call-ID: %s\n"
  "CSeq: %s\n\n";

const char * hostname = CONFIG_VOIP_HOSTNAME;
//const char * call_number = CONFIG_VOIP_CALL_NUMBER;
const char * username = CONFIG_VOIP_USERNAME;
const char * password = CONFIG_VOIP_PASSWORD;

/* parameters to send to taskSend and taskReceive */

uint16_t dest_port;
struct taskSendArgs sendArgs;

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
    ESP_LOGI(TAG, "Message:\n%s\n", buf);
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

int send_invite(int sock, struct sockaddr_in * sip_sockaddr, char * buf, int buf_len, char * external_ip_str, char * call_number) {

  int sdp_len = snprintf(buf, buf_len, sdp_str,
      "0",
      external_ip_str,
      "my_session",
      external_ip_str);
  int send_len = sdp_len;
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
      "my_session",
      external_ip_str);

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
    ESP_LOGI(TAG, "Message:\n%s\n", buf);
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

  send_len += snprintf(buf + send_len, buf_len - send_len, sdp_str,
      "0",
      external_ip_str,
      "my_session",
      external_ip_str);

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

static char external_ip_str[INET_ADDRSTRLEN];
static int sock;
static struct sockaddr_in sip_sockaddr;

int register_sip() {

  // get external ip
  int err = get_external_ip(external_ip_str);
  switch (err) {
    case 1:
      ESP_LOGE(TAG, "external_ip socket error");
      return 1;
    case 2:
      ESP_LOGE(TAG, "external_ip sendto error");
      return 1;
    case 3:
      ESP_LOGE(TAG, "external_ip packet format error");
      strcpy(external_ip_str, "127.0.0.1");
      break;
    case 4:
      ESP_LOGE(TAG, "external_ip inet_ntop error");
      return 1;
  }
  ESP_LOGI(TAG, "External IP: %s", external_ip_str);

  struct hostent * sip_host = gethostbyname(hostname);
  if (sip_host->h_addrtype != AF_INET) {
    ESP_LOGE(TAG, "Error: address type not IPv4\n");
    return 1;
  }

  char * host_addr_str = inet_ntoa(*(struct in_addr *)sip_host->h_addr);
  ESP_LOGI(TAG, "IP address: %s\n", host_addr_str);

  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    ESP_LOGE(TAG, "Error: unable to create socket\n");
    return 1;
  }

  //struct timeval timeout;
  //timeout.tv_sec = 10;
  //timeout.tv_usec = 0;
  //setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

  struct sockaddr_in src_addr;
  memset(&src_addr, 0, sizeof(src_addr));
  src_addr.sin_family = AF_INET;
  src_addr.sin_addr.s_addr = INADDR_ANY;
  src_addr.sin_port = htons(SRC_PORT);
  err = bind(sock, (struct sockaddr *)&src_addr, sizeof(src_addr));
  if (err < 0) {
    ESP_LOGE(TAG, "Error: unable to bind socket\n");
    return 1;
  }

  //struct sockaddr_in sip_sockaddr; // -- this is now a global variable
  socklen_t socklength = sizeof(sip_sockaddr);
  memset(&sip_sockaddr, 0, sizeof(struct sockaddr_in));
  sip_sockaddr.sin_family = AF_INET;
  sip_sockaddr.sin_port = htons(SIP_PORT);
  memcpy(&sip_sockaddr.sin_addr, sip_host->h_addr, sizeof(struct in_addr));

  char buf[BUF_LEN];

  err = send_reg(sock, &sip_sockaddr, buf, BUF_LEN, external_ip_str);
  if (err) {
    ESP_LOGE(TAG, "Error: send_reg failed with return value %d\n", err);
    return 1;
  }

  return 0;
}

void call_task(void * task_args) {
  char * call_number = (char *)task_args;
  make_call(call_number);
  xTaskNotify(*(TaskHandle_t *)&call_number[12], pdTRUE, eSetValueWithOverwrite);
  vTaskDelete(NULL);
}

void make_call(char * call_number) {
  // client is already registered with the server

  char buf[BUF_LEN];
  socklen_t socklength = sizeof(sip_sockaddr);

  // Send initial invite message (unauthorized)
  int err = send_invite(sock, &sip_sockaddr, buf, BUF_LEN, external_ip_str, call_number);
  if (err) {
    ESP_LOGE(TAG, "Error: send_invite failed with return value %d\n", err);
    return;
  }

  // buffer contains response to invite, check to make sure it is 100 TRYING
  char * sip_ver = "SIP/2.0 ";
  if (strncmp(sip_ver, buf, strlen(sip_ver))) {
    // incorrect response format
    ESP_LOGE(TAG, "Error: response formatted incorrectly");
    return;
  }

  char * response_type = buf + strlen(sip_ver);
  if (strncmp(response_type, "100", 3)) {
    // unknown
    ESP_LOGE(TAG, "Error: response has unknown status code: %s", response_type);
    return;
  }

  // server is TRYING, wait for RINGING or SESSION PROGRESS

  ssize_t rec_len = recvfrom(sock, buf, BUF_LEN - 1, 0, (struct sockaddr *)&sip_sockaddr, &socklength);
  ESP_LOGI(TAG, "Received %d bytes\n", rec_len);
  if (rec_len > BUF_LEN) {
    buf[BUF_LEN - 1] = '\0';
  }
  else if (rec_len > 0) {
    buf[BUF_LEN] = '\0';
  }

  // HANDLE RTP STREAMS AND TASK CREATION

  int running_rtp = 0;
  if (strncmp(sip_ver, buf, strlen(sip_ver))) {
    // incorrect response format
    ESP_LOGE(TAG, "Error: response formatted incorrectly");
    return;
  }

  response_type = buf + strlen(sip_ver);
  if (!strncmp(response_type, "180", 3)) {
    // RINGING -- wait for OK
  }
  else if (!strncmp(response_type, "183", 3)) {
    // SESSION PROGRESS -- get dest RTP port and initiate RTP streams
    char * sdp_pre = "m=audio ";
    char * sdp_m = strstr(buf, sdp_pre);
    if (!sdp_m) {
      // Substring not found (no sdp m specifier?)
      ESP_LOGE(TAG, "Error: session progress doesn't contain destination port");
      return;
    }
    sdp_m += strlen(sdp_pre);
    char * port_end = strchr(sdp_m, ' ');
    *port_end = '\0';
    int dport = atoi(sdp_m);
    dest_port = (uint16_t)dport;
    ESP_LOGI(TAG, "dest_port: %d\n", dest_port);

    sendArgs.dest_port = dest_port;
    sendArgs.src_port = RTP_PORT;
    sendArgs.sip_sockaddr = sip_sockaddr;

    ESP_LOGI(TAG, "SESSION PROGRESS, initializing rtp semaphores");
    uint16_t rtp_port = RTP_PORT;
    rtp_init(rtp_port);
    BaseType_t tret = xTaskCreate(taskReceive, "TaskReceive", 4096, NULL, 1, NULL);
    if (tret != pdPASS) {
      ESP_LOGE(TAG, "Error: couldn't create receive task");
      return;
    }
    ESP_LOGI(TAG, "Created taskReceive");
    tret = xTaskCreate(taskSend, "TaskSend", 4096, &sendArgs, 1, NULL);
    if (tret != pdPASS) {
      ESP_LOGE(TAG, "Error: couldn't create send task");
      return;
    }
    ESP_LOGI(TAG, "Created taskSend");
  }
  else {
    // unknown
    ESP_LOGE(TAG, "Error: response has unknown status code: %s", response_type);
    return;
  }

  // listen for more SIP messages (OK, BYE)
  int send_len;
  while (1) {

    ESP_LOGI(TAG, "Listening for SIP messages");
    rec_len = recvfrom(sock, buf, BUF_LEN - 1, 0, (struct sockaddr *)&sip_sockaddr, &socklength);
    ESP_LOGI(TAG, "Received %d bytes\n", rec_len);
    if (rec_len > BUF_LEN) {
      buf[BUF_LEN - 1] = '\0';
    }
    else if (rec_len > 0) {
      buf[BUF_LEN] = '\0';
    }
    if (rec_len < 0) {
      // no bytes received, continue
      continue;
    }
    ESP_LOGI(TAG, "Message:\n%s", buf);

    if (!strncmp(buf, sip_ver, strlen(sip_ver))) {
      // SIP RESPONSE
      response_type = buf + strlen(sip_ver);
      if (!strncmp(response_type, "200", 3)) { // OK
        // send ACK in response
        char to_tag[50];
        memset(to_tag, 0, 50);
        char * tag_start = strstr(buf, "tag=");
        tag_start += 4;
        tag_start = strstr(tag_start, "tag=");
        tag_start += 4;
        char * tag_end = strstr(tag_start, "\n");
        strncpy(to_tag, tag_start, tag_end - tag_start);
        send_len = snprintf(buf, BUF_LEN, ack_str,
            "sip:2245711812@208.100.60.8:5060",
            external_ip_str,
            "65300",
            "z9hG4bKPjfc7170ddca184cc38e1edaac48207ce7",
            username,
            "f2e78bb18e0f460e8724e9983319e1f3",
            call_number,
            to_tag,
            "3012",
            "6948ac89686744c8b37f0ba3ba175e1b");

        if (send_len >= BUF_LEN) {
          ESP_LOGW(TAG, "Warning: message was truncated to %d-byte length\n", BUF_LEN);
          buf[BUF_LEN - 1] = '\0';
        }
        else {
          buf[send_len] = '\0';
        }
        ESP_LOGI(TAG, "Sending Response:\n%s", buf);
        err = sendto(sock, buf, send_len, 0, (struct sockaddr *)&sip_sockaddr, socklength);
        if (err < 0) {
          ESP_LOGE(TAG, "sendto error");
          return;
        }

      }
    }
    else {
      // SIP REQUEST
      if (!strncmp(buf, "BYE", 3)) {
        // BYE request, send OK
        char str_buf[400];
        char * str_ptr = str_buf;
        char * ptr = strstr(buf, "Via: ");
        if (!ptr) {
          ESP_LOGE(TAG, "Error: via not found");
          return;
        }
        ptr += strlen("Via: ");
        char * ptr_end = strchr(ptr, '\n');
        char * vialine = strncpy(str_ptr, ptr, ptr_end - ptr);
        str_ptr += ptr_end - ptr;
        *str_ptr = '\0';
        str_ptr++;
        ESP_LOGI(TAG, "vialine: %s\n", vialine);
        ptr = strstr(buf, "From: ");
        if (!ptr) {
          ESP_LOGE(TAG, "Error: from not found");
          return;
        }
        ptr += strlen("From: ");
        ptr_end = strchr(ptr, '\n');
        char * fromline = strncpy(str_ptr, ptr, ptr_end - ptr);
        str_ptr += ptr_end - ptr;
        *str_ptr = '\0';
        str_ptr++;
        ESP_LOGI(TAG, "fromline: %s\n", fromline);
        ptr = strstr(buf, "To: ");
        if (!ptr) {
          ESP_LOGE(TAG, "Error: to not found");
          return;
        }
        ptr += strlen("To: ");
        ptr_end = strchr(ptr, '\n');
        char * toline = strncpy(str_ptr, ptr, ptr_end - ptr);
        str_ptr += ptr_end - ptr;
        *str_ptr = '\0';
        str_ptr++;
        ESP_LOGI(TAG, "toline: %s\n", toline);
        ptr = strstr(buf, "Call-ID: ");
        if (!ptr) {
          ESP_LOGE(TAG, "Error: to not found");
          return;
        }
        ptr += strlen("Call-ID: ");
        ptr_end = strchr(ptr, '\n');
        char * call_id = strncpy(str_ptr, ptr, ptr_end - ptr);
        str_ptr += ptr_end - ptr;
        *str_ptr = '\0';
        str_ptr++;
        ESP_LOGI(TAG, "call_id: %s\n", call_id);
        ptr = strstr(buf, "CSeq: ");
        if (!ptr) {
          ESP_LOGE(TAG, "Error: cseq not found");
          return;
        }
        ptr += strlen("CSeq: ");
        ptr_end = strchr(ptr, '\n');
        char * cseq = strncpy(str_ptr, ptr, ptr_end - ptr);
        str_ptr += ptr_end - ptr;
        *str_ptr = '\0';
        ESP_LOGI(TAG, "cseq: %s\n", cseq);
        if (str_ptr - str_buf > 400) {
          ESP_LOGE(TAG, "buffer overflow");
          return;
        }
        //char * ptr = strstr(buf, "branch=") + strlen("branch=");
        //if (!ptr) {
        //  ESP_LOGE(TAG, "Error: branch= not found");
        //  return;
        //}
        //char * ptr_end = strchr(ptr, ';');
        //if (!ptr_end) {
        //  ESP_LOGE(TAG, "Error: branch= to ; not found");
        //  return;
        //}

        //char * branch = strncpy(str_ptr, ptr, ptr_end - ptr);
        //str_ptr += ptr_end - ptr;
        //*str_ptr = '\0';
        //str_ptr++;
        //ESP_LOGI(TAG, "branch=%s\n", branch);
        //ptr = ptr_end + 1;
        //ptr = strstr(ptr, "tag=") + strlen("tag=");
        //if (!ptr) {
        //  ESP_LOGE(TAG, "Error: tag= not found");
        //  return;
        //}
        //ptr_end = strchr(ptr, '\n');
        //if (!ptr_end) {
        //  ESP_LOGE(TAG, "Error: tag= to \\n not found");
        //  return;
        //}
        //char * tag1 = strncpy(str_ptr, ptr, ptr_end - ptr);
        //str_ptr += ptr_end - ptr;
        //*str_ptr = '\0';
        //str_ptr++;
        //ptr = ptr_end + 1;
        //ptr = strstr(ptr, "tag=") + strlen("tag=");
        //if (!ptr) {
        //  ESP_LOGE(TAG, "Error: tag= not found");
        //  return;
        //}
        //ptr_end = strchr(ptr, '\n');
        //if (!ptr_end) {
        //  ESP_LOGE(TAG, "Error: tag= to \\n not found");
        //  return;
        //}
        //char * tag2 = strncpy(str_ptr, ptr, ptr_end - ptr);
        //str_ptr += ptr_end - ptr;
        //*str_ptr = '\0';
        //str_ptr++;
        //ptr = ptr_end + 1;
        //ESP_LOGI(TAG, "BUF:\n%s\n", buf);
        //ptr = strstr(buf, "Call-ID: ") + strlen("Call-ID: ");
        //ESP_LOGI(TAG, "ptr: %p", ptr);
        //if (!ptr) {
        //  ESP_LOGE(TAG, "Error: Call-ID: not found");
        //  return;
        //}
        //ptr_end = strchr(ptr, '\n');
        //if (!ptr_end) {
        //  ESP_LOGE(TAG, "Error: Call-ID to \\n not found");
        //  return;
        //}
        //ESP_LOGI(TAG, "%s", ptr);
        //char * call_id = strncpy(str_ptr, ptr, ptr_end - ptr);
        //str_ptr += ptr_end - ptr;
        //*str_ptr = '\0';
        //str_ptr++;
        //ptr = ptr_end + 1;
        //ptr = strstr(buf, "CSeq: ") + strlen("CSeq: ");
        //if (!ptr) {
        //  ESP_LOGE(TAG, "Error: Cseq: not found");
        //  return;
        //}
        //ptr_end = strchr(ptr, ' ');
        //if (!ptr) {
        //  ESP_LOGE(TAG, "Error: Cseq: to ' ' not found");
        //  return;
        //}
        //*ptr_end = '\0';
        //ESP_LOGI(TAG, "%s", ptr);
        //char * cseq = strncpy(str_ptr, ptr, ptr_end - ptr);
        //str_ptr += ptr_end - ptr;
        //*str_ptr = '\0';

        ESP_LOGI(TAG, "toline: %s\n", toline);
        send_len = snprintf(buf, BUF_LEN, ok_str,
            vialine,
            fromline,
            toline,
            call_id,
            cseq);

        if (send_len >= BUF_LEN) {
          ESP_LOGW(TAG, "Warning: message was truncated to %d-byte length\n", BUF_LEN);
          buf[BUF_LEN - 1] = '\0';
        }
        else {
          buf[send_len] = '\0';
        }
        ESP_LOGI(TAG, "Sending Response:\n%s", buf);
        err = sendto(sock, buf, send_len, 0, (struct sockaddr *)&sip_sockaddr, socklength);
        if (err < 0) {
          ESP_LOGE(TAG, "sendto error");
          return;
        }
        return;
      }
    }

  }

  close(sock);
}
