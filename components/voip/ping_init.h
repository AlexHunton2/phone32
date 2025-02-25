/* ping_init.h */

#ifndef PING_INIT_H
#define PING_INIT_H

void initialize_ping(
    char *,
    void (*)(esp_ping_handle_t, void *),
    void (*)(esp_ping_handle_t, void *),
    void (*)(esp_ping_handle_t, void *)
    );

#endif
