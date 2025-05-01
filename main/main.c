/* main.c */
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "esp_wifi.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include <string.h>

#include "graphics.h"
#include "voip.h"
// #include "md5_digest.h"

/* The examples use WiFi configuration that you can set via project
   configuration menu

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/

#define ESP_METHOD ESP_METHOD_WPA3_PERSONAL

#define ESP_WIFI_SSID CONFIG_ESP_WIFI_SSID
#define WPA2_EAP_ID CONFIG_ESP_WIFI_EAP_ID
#define WPA2_EAP_USERNAME CONFIG_ESP_WIFI_USERNAME

#define WIFI_PASSWORD CONFIG_ESP_WIFI_PASSWORD

#define PING_TARGET_ADDR CONFIG_PING_TARGET_ADDR

#define EXAMPLE_ESP_MAXIMUM_RETRY CONFIG_ESP_MAXIMUM_RETRY

#if CONFIG_ESP_WPA3_SAE_PWE_HUNT_AND_PECK
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HUNT_AND_PECK
#define EXAMPLE_H2E_IDENTIFIER ""
#elif CONFIG_ESP_WPA3_SAE_PWE_HASH_TO_ELEMENT
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HASH_TO_ELEMENT
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#elif CONFIG_ESP_WPA3_SAE_PWE_BOTH
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_BOTH
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#endif
#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about
 * two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

/* esp netif object representing the WIFI station */
static esp_netif_t *sta_netif = NULL;

static const char *TAG = "wifi station";

static int s_retry_num = 0;

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED) {
    if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
      esp_wifi_connect();
      s_retry_num++;
      ESP_LOGI(TAG, "retry to connect to the AP");
    } else {
      xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
    }
    ESP_LOGI(TAG, "connect to the AP fail");
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    s_retry_num = 0;
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  }
}

static esp_err_t wifi_init_sta(void) {
  s_wifi_event_group = xEventGroupCreate();

  ESP_ERROR_CHECK(esp_netif_init());

  ESP_ERROR_CHECK(esp_event_loop_create_default());
  sta_netif = esp_netif_create_default_wifi_sta();
  assert(sta_netif);

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

#if CONFIG_ESP_AUTH_WPA2_ENTERPRISE
  wifi_config_t wifi_config = {
      .sta =
          {
              .ssid = ESP_WIFI_SSID,
          },
  };
#else
  wifi_config_t wifi_config = {
      .sta =
          {
              .ssid = ESP_WIFI_SSID,
              .password = WIFI_PASSWORD,
              .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
              .sae_pwe_h2e = ESP_WIFI_SAE_MODE,
              .sae_h2e_identifier = EXAMPLE_H2E_IDENTIFIER,
          },
  };
#endif

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

#if CONFIG_ESP_AUTH_WPA2_ENTERPRISE
  ESP_ERROR_CHECK(
      esp_eap_client_set_identity((uint8_t *)WPA2_EAP_ID, strlen(WPA2_EAP_ID)));

  // skipping server certificate validation, setting PEAP user/pass
  ESP_ERROR_CHECK(esp_eap_client_set_username((uint8_t *)WPA2_EAP_USERNAME,
                                              strlen(WPA2_EAP_USERNAME)));
  ESP_ERROR_CHECK(esp_eap_client_set_password((uint8_t *)WIFI_PASSWORD,
                                              strlen(WIFI_PASSWORD)));

  ESP_ERROR_CHECK(esp_wifi_sta_enterprise_enable());
#endif

  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_LOGI(TAG, "wifi_init_sta finished.");

  /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or
   * connection failed for the maximum number of re-tries (WIFI_FAIL_BIT). The
   * bits are set by event_handler() (see above) */
  EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                         WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                         pdFALSE, pdFALSE, portMAX_DELAY);

  /* xEventGroupWaitBits() returns the bits before the call returned, hence we
   * can test which event actually happened. */
  if (bits & WIFI_CONNECTED_BIT) {
    ESP_LOGI(TAG, "connected to AP");
    return 0;
  } else if (bits & WIFI_FAIL_BIT) {
    ESP_LOGI(TAG, "Failed to connect to AP");
    return 1;
  } else {
    ESP_LOGE(TAG, "UNEXPECTED EVENT");
    return 2;
  }
}

/*
static void run_voip(void *) {
  char *call_number = "2245711812";
  if (register_sip()) {
    ESP_LOGE(TAG, "Sip register failed");
    return;
  }
  ESP_LOGI(TAG, "Sip Registered");

  //make_call(call_number);
  vTaskDelete(NULL);
}
*/

void app_main(void) {
  // ===  Graphics  ===
  graphics_init();

  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  // Connect to wifi (needed for the entire program)
  ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
  esp_err_t wifi_ret = wifi_init_sta();
  if (wifi_ret) {
    ESP_LOGI(TAG, "Connection failed, exiting program");
    return;
  }

  // xTaskCreate(&wifi_task, "wifi_task", 4096, NULL, 5, NULL);
  esp_netif_ip_info_t ip;
  memset(&ip, 0, sizeof(esp_netif_ip_info_t));
  ESP_LOGI(TAG, "~~~~~~~~~~~");
  if (esp_netif_is_netif_up(sta_netif)) {
    ESP_LOGI(TAG, "INTERFACE IS UP");
  } else {
    ESP_LOGI(TAG, "INTERFACE IS DOWN");
  }
  if (esp_netif_get_ip_info(sta_netif, &ip) == 0) {
    ESP_LOGI(TAG, "IP:" IPSTR, IP2STR(&ip.ip));
    ESP_LOGI(TAG, "MASK:" IPSTR, IP2STR(&ip.netmask));
    ESP_LOGI(TAG, "GW:" IPSTR, IP2STR(&ip.gw));
    ESP_LOGI(TAG, "~~~~~~~~~~~");

    // Update internal clock to be on correct time by pooling NTP
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
  }

  // run_voip();
  //  char final[MD5_DIGEST_LENGTH * 2 + 1];
  //  final[MD5_DIGEST_LENGTH * 2] = '\0';
  //  char * input = "Hello World!";
  //  int val = get_digest(final, (uint8_t *)input, strlen(input));
  //  if (val) {
  //    ESP_LOGE("main", "error getting digest");
  //  }
  //  else {
  //    ESP_LOGI("main", "%s", final);
  //  }
  //
  if (register_sip()) {
    ESP_LOGE(TAG, "Sip register failed");
    return;
  }
  // xTaskCreate(run_voip, "RunVoip", 8000, NULL, 1, NULL);
}
