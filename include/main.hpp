// Define private macros
#define RELAY_PIN 4 // Pin for relay
#define LOCKOUT_SWITCH_PIN 17 // Pin for switch
#define LOCKOUT_SWITCH_LOCKED 0 // Switch is closed
#define LOCKOUT_SWITCH_UNLOCKED 1 // Switch is open

typedef enum {
  door_closed = 0x00,
  door_open = 0x01
} DOORSTATUS;

typedef enum {
  door_unlocked = 0x00,
  door_locked_wifi,
  door_locked_switch,
  door_locked_both
} DOORLOCKSTATE;