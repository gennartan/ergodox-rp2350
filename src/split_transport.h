#ifndef SPLIT_TRANSPORT_H
#define SPLIT_TRANSPORT_H

#include <stdbool.h>
#include <stdint.h>

typedef void (*split_report_cb_t)(const uint8_t *report, uint8_t len,
    void *user_data);

int split_transport_init(split_report_cb_t cb, void *user_data);
int split_transport_send_report(const uint8_t *report, uint8_t len);

#endif /* SPLIT_TRANSPORT_H */
