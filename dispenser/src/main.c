/*
 * Copyright (c) 2026 Sam Kwort
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/shell/shell.h>
#include <zephyr/smf.h>
#include <zephyr/logging/log.h>
#include "main.h"
#include "mqtt_handler.h"

LOG_MODULE_REGISTER(dispenser, LOG_LEVEL_INF);

/* ========================================================================== */
/* State durations (ms) */
#define TICK_MS            100
#define STATE_COUNT         6

/* ========================================================================== */
/* State Machine Context                                                      */
/* ========================================================================== */

static const char *const state_names[] = {
    [STATE_IDLE] = "IDLE",
    [STATE_LOCKED] = "LOCKED",
    [STATE_WAITING_PIN] = "WAITING_PIN",
    [STATE_VALIDATING] = "VALIDATING",
    [STATE_UNLOCKED] = "UNLOCKED",
    [STATE_DISPENSING] = "DISPENSING",
};


struct dispenser_ctx dispenser_ctx;

/* ========================================================================== */
/* Forward Declarations                                                       */
/* ========================================================================== */

static enum smf_state_result idle_run(void *obj);

static void locked_entry(void *obj);
static enum smf_state_result locked_run(void *obj);

static void waiting_pin_entry(void *obj);
static enum smf_state_result waiting_pin_run(void *obj);

static void validating_entry(void *obj);
static enum smf_state_result validating_run(void *obj);

static void unlocked_entry(void *obj);
static enum smf_state_result unlocked_run(void *obj);

static void dispensing_entry(void *obj);
static enum smf_state_result dispensing_run(void *obj);


/* ========================================================================== */
/* State Table                                                                */
/* ========================================================================== */

static const struct smf_state states[] = {
    [STATE_IDLE] = SMF_CREATE_STATE(NULL, idle_run, NULL, NULL, NULL),

    [STATE_LOCKED] = SMF_CREATE_STATE(locked_entry, locked_run, NULL, NULL, NULL),

    [STATE_WAITING_PIN] = SMF_CREATE_STATE(waiting_pin_entry, waiting_pin_run, NULL, NULL, NULL),

    [STATE_VALIDATING] = SMF_CREATE_STATE(validating_entry, validating_run, NULL, NULL, NULL),

    [STATE_UNLOCKED] = SMF_CREATE_STATE(unlocked_entry, unlocked_run, NULL, NULL, NULL),
    
    [STATE_DISPENSING] = SMF_CREATE_STATE(dispensing_entry, dispensing_run, NULL, NULL, NULL),
};

/* ========================================================================== */
/* State Handlers                                                             */
/* ========================================================================== */

// Wait for scheduled unlock command from base, then transition to WAITING_PIN
static enum smf_state_result idle_run(void *obj)
{
    struct dispenser_ctx *ctx = obj;

    if (ctx->unlock_received) {
        // Reset for new cycle
        ctx->unlock_received = false;
        smf_set_state(SMF_CTX(ctx), &states[STATE_WAITING_PIN]);
        return SMF_EVENT_HANDLED;
    }

    return SMF_EVENT_HANDLED;
}

// Display "ENTER PIN" message to user
static void waiting_pin_entry(void *obj)
{
    struct dispenser_ctx *ctx = obj;
    // Clear entered pin for next cycle
    memset(ctx->entered_pin, 0, sizeof(ctx->entered_pin));  
    LOG_INF("ENTER PIN");
}

// Wait for pin entry from keypad, then transition to VALIDATING
static enum smf_state_result waiting_pin_run(void *obj)
{
    struct dispenser_ctx *ctx = obj;

    if (ctx->entered_pin[0] != '\0') { // simple check for pin entry
        smf_set_state(SMF_CTX(ctx), &states[STATE_VALIDATING]);
        return SMF_EVENT_HANDLED;
    }

    return SMF_EVENT_HANDLED;
}

// Display "VALIDATING" message to user
static void validating_entry(void *obj)
{
    struct dispenser_ctx *ctx = obj;
    LOG_INF("VALIDATING");
}

// Check entered pin against correct pin, transition to UNLOCKED or LOCKED
static enum smf_state_result validating_run(void *obj)
{
    struct dispenser_ctx *ctx = obj;
    if (strcmp(ctx->entered_pin, ctx->correct_pin) == 0) {
        smf_set_state(SMF_CTX(ctx), &states[STATE_UNLOCKED]);
        LOG_INF("Correct PIN");
    } 
    else {
        char payload[512];
        build_dispense_failure_event(ctx, "invalid_pin", payload, sizeof(payload));
        // Placeholder for MQTT publish function, replace with real implementation later
        // mqtt_publish_event(payload);
        smf_set_state(SMF_CTX(ctx), &states[STATE_LOCKED]);
        LOG_INF("Incorrect PIN");
    }

    return SMF_EVENT_HANDLED;
}

// Display "UNLOCKED" message and open servo
static void unlocked_entry(void *obj)
{
    struct dispenser_ctx *ctx = obj;
    ctx->servo_open = true;
    LOG_INF("UNLOCKED");
    // Placeholder for servo to open flap
    // servo_open(); 
}

// Transition straight to DISPENSING
static enum smf_state_result unlocked_run(void *obj)
{
    struct dispenser_ctx *ctx = obj;

    smf_set_state(SMF_CTX(ctx), &states[STATE_DISPENSING]);
    return SMF_EVENT_HANDLED;
}

// Close servo if open, display "LOCKED" message to user
static void locked_entry(void *obj)
{
    struct dispenser_ctx *ctx = obj;

    if (ctx->servo_open) {
        // Placeholder for servo to close flap
        // servo_close(); 
        ctx->servo_open = false;
    }
    LOG_INF("LOCKED");
}

// Wait for scheduled unlock command from base, then transition to WAITING_PIN
static enum smf_state_result locked_run(void *obj)
{
    struct dispenser_ctx *ctx = obj;

    if (ctx->unlock_received) {
        // Reset for new cycle
        ctx->unlock_received = false;
        smf_set_state(SMF_CTX(ctx), &states[STATE_WAITING_PIN]);
        return SMF_EVENT_HANDLED;
    }

    return SMF_EVENT_HANDLED;
}

// Display "DISPENSING" message, decrement pill count and send dispense success event to base
static void dispensing_entry(void *obj)
{
    struct dispenser_ctx *ctx = obj;

    ctx->pill_count--;
    LOG_INF("DISPENSING - pills remaining: %d", ctx->pill_count);

    // Send dispense success event to base node for logging and inventory tracking
    char payload[512];
    build_dispense_success_event(ctx, payload, sizeof(payload));
    // Placeholder for MQTT publish function, replace with real implementation later
    // mqtt_publish_event(payload);
}

// Transitions straight to LOCKED
static enum smf_state_result dispensing_run(void *obj)
{
    struct dispenser_ctx *ctx = obj;

    smf_set_state(SMF_CTX(ctx), &states[STATE_LOCKED]);
    return SMF_EVENT_HANDLED;
}


/* ========================================================================== */
/* Shell Commands                                                             */
/* ========================================================================== */

static dispenser_state_t get_current_state(void)
{
    const struct smf_state *current = dispenser_ctx.ctx.current;

    for (int i = 0; i < STATE_COUNT; i++) {
        if (current == &states[i]) {
            return (dispenser_state_t)i;
        }
    }

    return STATE_IDLE;
}

static int cmd_permission(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    dispenser_ctx.unlock_received = true;
    shell_print(sh, "Unlock permission sent");

    return 0;
}

static int cmd_pin(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);

    strncpy(dispenser_ctx.entered_pin, argv[1], sizeof(dispenser_ctx.entered_pin) - 1);
    shell_print(sh, "PIN entered: %s", argv[1]);

    return 0;
}

static int cmd_status(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    dispenser_state_t current = get_current_state();

    shell_print(sh, "State: %s", state_names[current]);
    shell_print(sh, "Pill count: %d", dispenser_ctx.pill_count);

    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(smf_cmds,
                               SHELL_CMD_ARG(permission, NULL,
                                             "Simulate unlock permission from base node\n"
                                             "Usage: smf permission",
                                             cmd_permission, 1, 0),
                               SHELL_CMD_ARG(pin, NULL,
                                             "Simulate PIN entry\n"
                                             "Usage: smf pin <pin>",
                                             cmd_pin, 2, 0),
                               SHELL_CMD(status, NULL,
                                         "Show current state and pill count",
                                         cmd_status),
                               SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(smf, &smf_cmds, "Dispenser state machine", NULL);

/* ========================================================================== */
/* Initialisation                                                             */
/* ========================================================================== */

int main(void)
{
    int ret;

    dispenser_ctx.pill_count = 10;
    dispenser_ctx.correct_pin = "1234";

    smf_set_initial(SMF_CTX(&dispenser_ctx), &states[STATE_IDLE]);

    while (1) {
        ret = smf_run_state(SMF_CTX(&dispenser_ctx));
        if (ret != 0) {
            LOG_ERR("State machine terminated: %d", ret);
            break;
        }
        k_msleep(TICK_MS);
    }

    return 0;
}