/* rtp.h */

#ifndef RTP_H
#define RTP_H


//extern SemaphoreHandle_t xMutexLock;
//extern SemaphoreHandle_t xCountingLock;

//extern uint8_t rtp_ring[RTP_BUF_COUNT][RTP_BUF_SIZE]

struct rtp_hdr {
  // below: 2 bit version, 1 bit padding, 1 bit extension, 4 bit CSRC count
  uint8_t vers_pad_x_csrc;
  uint8_t m_pt; // 1 bit marker, 7 bit payload type
  uint16_t seq_num; // 16 bit sequence number, incremented for each sent packet
  uint32_t timestamp;
  uint32_t ssrc; // 32 bit synchronization source identifier
  uint8_t payload[1]; // variable-length CSRC and header extensions
};

struct taskSendArgs {
  struct sockaddr_in sip_sockaddr;
  uint16_t src_port;
  uint16_t dest_port;
};

int rtp_init(uint16_t);
void taskReceive(void *);
void taskSend(void *);

#endif
