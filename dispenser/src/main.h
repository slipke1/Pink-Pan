#ifndef MAIN_H
#define MAIN_H

#include <zephyr/smf.h>
#include <stdbool.h>

/* ========================================================================== */
/* State Machine States                                                       */
/* ========================================================================== */

typedef enum {
    STATE_IDLE,           // System startup, awaiting first unlock command
    STATE_LOCKED,         // Locked after a completed or failed cycle
    STATE_WAITING_PIN,    // Unlock command received, awaiting PIN entry
    STATE_VALIDATING,     // Checking entered PIN
    STATE_UNLOCKED,       // PIN correct, servo open
    STATE_DISPENSING,     // IR beam broken, pill detected
} dispenser_state_t;



// Struct for state machine context, holds current state and any relevant data
struct dispenser_ctx {
    struct smf_ctx ctx;
    bool unlock_received;    // set when unlock command comes from base node
    char entered_pin[8];     // pin entered by user on keypad
    char* correct_pin;       // correct pin for validation
    int pill_count;          // current pill inventory
    bool servo_open;         // current servo state, true if open
    int event_id;          // Incrementing event ID for outgoing events, used by base node for logging and tracking
};

// Global
extern struct dispenser_ctx dispenser_ctx;

#endif /* MAIN_H */