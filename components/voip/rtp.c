/* rtp.c */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "esp_log.h"

#include "lwip/sockets.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"

#include "rtp.h"

#define RTP_BUF_SIZE 300
#define RTP_BUF_COUNT 3

//static SemaphoreHandle_t xMutexLock;
static SemaphoreHandle_t xProduce;
static SemaphoreHandle_t xConsume;

static int sock;

struct rtp_data {
  ssize_t len; // length of the message
  uint8_t data[RTP_BUF_SIZE];
};

static struct rtp_data rtp_ring[RTP_BUF_COUNT];

static int produce_idx = 0;
static int consume_idx = 0;

int rtp_init(uint16_t src_port) {
  const char * TAG = "rtpinit";
  //xMutexLock = xSemaphoreCreateMutex();
  xProduce = xSemaphoreCreateCounting(RTP_BUF_COUNT, RTP_BUF_COUNT);
  xConsume = xSemaphoreCreateCounting(RTP_BUF_COUNT, 0);

  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    ESP_LOGE(TAG, "Error: unable to create socket\n");
    return 1;
  }

  struct sockaddr_in src_addr;
  memset(&src_addr, 0, sizeof(src_addr));
  src_addr.sin_family = AF_INET;
  src_addr.sin_addr.s_addr = INADDR_ANY;
  src_addr.sin_port = htons(src_port);
  int err = bind(sock, (struct sockaddr *)&src_addr, sizeof(src_addr));

  if (err < 0) {
    ESP_LOGE(TAG, "Error: unable to bind socket\n");
    ESP_LOGE(TAG, "errno = %d\n", errno);
    return 1;
  }
  return 0;
}

void taskReceive(void * null) {

  const char * TAG = "taskReceive";

  uint32_t ssrc = 0xdeadbeef;

  // create initial two RTP packets to send (with all ff payload)
  struct rtp_hdr * init_hdr = (struct rtp_hdr *)rtp_ring[0].data;
  init_hdr->vers_pad_x_csrc = 0x80; // version 2, all else is 0
  init_hdr->m_pt = 0; // no marker, payload is default G.711 (0)
  //init_hdr->seq_num = htons(1); // why not
  //init_hdr->timestamp = 0; // why not -- increment by 160 each packet for G.711
                          // -- 20ms packets, which is 1/50 sec, so 160 increase
                          // given 8000khz clock
  //init_hdr->ssrc = ssrc;
  memset(init_hdr->payload, 0, 160);
  rtp_ring[0].len = 172;
  xSemaphoreTake(xProduce, portMAX_DELAY);
  xSemaphoreGive(xConsume);

  init_hdr = (struct rtp_hdr *)rtp_ring[1].data;
  init_hdr->vers_pad_x_csrc = 0x80; // version 2, all else is 0
  init_hdr->m_pt = 0; // no marker, payload is default G.711 (0)
  //init_hdr->seq_num = htons(1); // why not
  //init_hdr->timestamp = 0; // why not -- increment by 160 each packet for G.711
                          // -- 20ms packets, which is 1/50 sec, so 160 increase
                          // given 8000khz clock
  //init_hdr->ssrc = ssrc;
  memset(init_hdr->payload, 0, 160);
  rtp_ring[1].len = 172;
  xSemaphoreTake(xProduce, portMAX_DELAY);
  xSemaphoreGive(xConsume);

  int i = 2;
  ssize_t rec_len;

  ESP_LOGI(TAG, "entering receive loop");
  while (1) {
    xSemaphoreTake(xProduce, portMAX_DELAY);

    rec_len = recvfrom(sock, rtp_ring[i].data, RTP_BUF_SIZE, 0, NULL, NULL);
    if (rec_len > RTP_BUF_SIZE) {
      rec_len = RTP_BUF_SIZE;
    }
    rtp_ring[i].len = rec_len;

    i = (i + 1) % RTP_BUF_COUNT;

    xSemaphoreGive(xConsume);
    ESP_LOGI(TAG, "received %d bytes", rtp_ring[i].len);
  }

}

void taskSend(void * sendArg) {

  const char * TAG = "taskSend";

  struct taskSendArgs * sendArgs = (struct taskSendArgs *)sendArg;

  // server address should stay the same, just change the port
  socklen_t socklength = sizeof(sendArgs->sip_sockaddr);
  sendArgs->sip_sockaddr.sin_port = htons(sendArgs->dest_port);

  int i = 0;
  uint16_t count = 0;
  uint32_t ssrc = 0xdeadbeef;
  uint32_t timestamp = 0;
  ESP_LOGI(TAG, "entering send loop");
  while (1) {
    xSemaphoreTake(xConsume, portMAX_DELAY);
    // TODO: change the RTP header information
    struct rtp_hdr * hdr = (struct rtp_hdr *)rtp_ring[i].data;
    hdr->seq_num = htons(count);
    hdr->ssrc = htons(ssrc);
    hdr->timestamp = htonl(timestamp);

    int err = sendto(sock, rtp_ring[i].data, rtp_ring[i].len, 0, (struct sockaddr *)&sendArgs->sip_sockaddr, socklength);
    count++;
    i = (i + 1) % RTP_BUF_COUNT;
    timestamp += 160;
    xSemaphoreGive(xProduce);
    ESP_LOGI(TAG, "consumed buffer");
  }

}
