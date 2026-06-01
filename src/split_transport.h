#ifndef SPLIT_TRANSPORT_H
#define SPLIT_TRANSPORT_H

#include <stdbool.h>
#include <stdint.h>

#define SPLIT_TRANSPORT_MAX_EVENTS 8

enum split_key_event_type {
    SPLIT_KEY_EVENT_RELEASE = 0,
    SPLIT_KEY_EVENT_PRESS = 1,
    SPLIT_KEY_EVENT_DOUBLE_TAP = 2,
};

struct split_key_event {
    uint8_t hand;
    uint8_t key_index;
    uint8_t type;
};

typedef void (*split_events_cb_t)(const struct split_key_event *events,
    uint8_t count,
    void *user_data);

int split_transport_init(split_events_cb_t cb, void *user_data);
int split_transport_send_events(const struct split_key_event *events,
    uint8_t count);
int split_transport_send_event(const struct split_key_event *event);

#endif /* SPLIT_TRANSPORT_H */
