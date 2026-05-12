#include <zephyr/kernel.h>
#include <zephyr/data/json.h>
#include <zephyr/logging/log.h>
#include <stdio.h>
#include <string.h>
#include "mqtt_handler.h"

LOG_MODULE_REGISTER(mqtt_handler, LOG_LEVEL_INF);

// Current unlock permission command                                         
static struct unlock_permission_command current_unlock;


// Helper: Extract fields of incoming unlock permission command from base node and populate struct
static const struct json_obj_descr unlock_permission_descr[] = {
    JSON_OBJ_DESCR_PRIM(struct unlock_permission_command, schedule_id, JSON_TOK_STRING),
    JSON_OBJ_DESCR_PRIM(struct unlock_permission_command, medication_id, JSON_TOK_STRING),
    JSON_OBJ_DESCR_PRIM(struct unlock_permission_command, allowed_doses, JSON_TOK_NUMBER),
    JSON_OBJ_DESCR_PRIM(struct unlock_permission_command, unlock_window_sec, JSON_TOK_NUMBER),
    JSON_OBJ_DESCR_PRIM(struct unlock_permission_command, timestamp, JSON_TOK_NUMBER),
};

// Parse incoming unlock permission command by calling helper above 
// Will be called from main.c when MQTT message received from base node with unlock permission details                              
int parse_unlock_permission_command(const char *payload, struct dispenser_ctx *ctx)
{
    int ret = json_obj_parse((char *)payload, strlen(payload),
                             unlock_permission_descr,
                             ARRAY_SIZE(unlock_permission_descr),
                             &current_unlock);
    if (ret < 0) {
        LOG_ERR("Failed to parse unlock permission command: %d", ret);
        return ret;
    }

    // Reject if command type is not "unlock"
    // Could change later
    if (strcmp(current_unlock.type, "unlock") != 0) {
        LOG_ERR("Unexpected command type: %s", current_unlock.type);
        return -EINVAL;
    }

    ctx->unlock_received = true;

    LOG_INF("Unlock permission parsed: schedule=%s med=%s doses=%d",
            current_unlock.schedule_id,
            current_unlock.medication_id,
            current_unlock.allowed_doses);

    return 0;
}

// Build outgoing dispense success event                                      
int build_dispense_success_event(struct dispenser_ctx *ctx, char *buf, size_t buf_len)
{
    // Increment event ID for unique identification of this event in logs, etc. on base node
    ctx->event_id++;

    // Build JSON payload with event details, including current schedule/medication, amount dispensed, remaining estimate, etc.
    int ret = snprintf(buf, buf_len,
        "{"
        "\"event\": \"dispense_success\","
        "\"event_id\": \"evt_%04d\","
        "\"schedule_id\": \"%s\","
        "\"medication_id\": \"%s\","
        "\"dispensed_amount\": %d,"
        "\"remaining_estimate\": %d,"
        "\"auth_method\": \"pin\","
        "\"timestamp\": %lld"
        "}",
        ctx->event_id,
        current_unlock.schedule_id,
        current_unlock.medication_id,
        current_unlock.allowed_doses,
        ctx->pill_count,
        k_uptime_get());

    if (ret >= buf_len) {
        LOG_ERR("Dispense event buffer too small");
        return -ENOMEM;
    }

    LOG_INF("Built dispense event: %s", buf);
    return 0;
}

// Build outgoing dispense failure event                                         
int build_dispense_failure_event(struct dispenser_ctx *ctx, const char *reason, char *buf, size_t buf_len)
{
    // Increment event ID for unique identification of this event in logs, etc. on base node
    ctx->event_id++;

    // Build JSON payload with event details, including reason for failure
    int ret = snprintf(buf, buf_len,
        "{"
        "\"event\": \"dispense_failure\","
        "\"event_id\": \"evt_%04d\","
        "\"reason\": \"%s\","
        "\"timestamp\": %lld"
        "}",
        ctx->event_id,
        reason,
        k_uptime_get());

    if (ret >= buf_len) {
        LOG_ERR("Dispense failure event buffer too small");
        return -ENOMEM;
    }

    LOG_INF("Built dispense failure event: %s", buf);
    return 0;
}

/* ========================================================================== */
/* Publish event - stub for now                                              */
/* ========================================================================== */

int mqtt_publish_event(const char *payload)
{
    // TODO: replace with real MQTT publish call when Argon is available
    LOG_INF("MQTT publish (stub): %s", payload);
    return 0;
}