#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

#define DEFAULT_CFG_NAME "borznes_cfg.txt"

#define CFG_P1_A      "p1_A"
#define CFG_P1_B      "p1_B"
#define CFG_P1_START  "p1_START"
#define CFG_P1_SELECT "p1_SELECT"
#define CFG_P1_UP     "p1_UP"
#define CFG_P1_DOWN   "p1_DOWN"
#define CFG_P1_LEFT   "p1_LEFT"
#define CFG_P1_RIGHT  "p1_RIGHT"
#define CFG_P2_A      "p2_A"
#define CFG_P2_B      "p2_B"
#define CFG_P2_START  "p2_START"
#define CFG_P2_SELECT "p2_SELECT"
#define CFG_P2_UP     "p2_UP"
#define CFG_P2_DOWN   "p2_DOWN"
#define CFG_P2_LEFT   "p2_LEFT"
#define CFG_P2_RIGHT  "p2_RIGHT"

typedef struct ConfigNode {
    char*              key;
    uint64_t           value;
    struct ConfigNode* next;
} ConfigNode;

extern ConfigNode* g_cfg;

void config_load(const char* filename);
void config_save(const char* filename);
void config_unload();

int  config_get_value(const char* key, uint64_t* value);
void config_set_value(const char* key, uint64_t value);

#endif
