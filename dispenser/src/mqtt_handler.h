#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <stddef.h>
#include "main.h"

/* ========================================================================== */
/* Unlock permission command from base node                                  */
/* ========================================================================== */

// Message published from base node to dispenser to grant unlock permission for a specific medication schedule
struct unlock_permission_command {
    char type[16];
    char schedule_id[16];
    char medication_id[16];
    int allowed_doses;
    int unlock_window_sec;
    int timestamp;
};


// Incoming - Parse unlock permission command from base node                 
int parse_unlock_permission_command(const char *payload, struct dispenser_ctx *ctx);


// Outgoing - Build event JSON payloads                                      
int build_dispense_success_event(struct dispenser_ctx *ctx, char *buf, size_t buf_len);
int build_dispense_failure_event(struct dispenser_ctx *ctx, const char *reason, char *buf, size_t buf_len);


// Publish - Stub for now, replace with real MQTT publish later              
int mqtt_publish_event(const char *payload);

#endif /* MQTT_HANDLER_H */