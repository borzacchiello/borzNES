#include "config.h"
#include "alloc.h"
#include "logging.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ConfigNode* g_cfg;

void config_load(const char* filename)
{
    FILE* f = fopen(filename, "r");
    if (!f) {
        warning("config_load(): unable to open config file");
        return;
    }

    static char buf[128];
    while (fgets(buf, sizeof(buf), f)) {
        char*    key;
        uint64_t value;

        int nread = sscanf(buf, "%ms : %llx", &key, (unsigned long long*)&value);
        if (nread == EOF)
            // empty line
            continue;

        if (nread != 2)
            panic("config_load(): invalid config line \"%s\"", buf);

        ConfigNode* n = malloc_or_fail(sizeof(ConfigNode));
        n->key        = key;
        n->value      = value;
        n->next       = g_cfg;
        g_cfg         = n;
    }
    fclose(f);
}

void config_unload()
{
    ConfigNode* curr = g_cfg;
    while (curr != NULL) {
        ConfigNode* prev = curr;
        curr             = curr->next;
        free(prev->key);
        free(prev);
    }
}

void config_save(const char* filename)
{
    FILE* f = fopen(filename, "w");
    if (!f)
        panic("config_save(): unable to open config file");

    ConfigNode* curr = g_cfg;
    while (curr != NULL) {
        fprintf(f, "%s : %llx\n", curr->key, (unsigned long long)curr->value);
        curr = curr->next;
    }
    fclose(f);
}

int config_get_value(const char* key, uint64_t* value)
{
    ConfigNode* curr = g_cfg;
    while (curr != NULL) {
        if (strcmp(curr->key, key) == 0) {
            *value = curr->value;
            return 1;
        }
        curr = curr->next;
    }
    return 0;
}

void config_set_value(const char* key, uint64_t value)
{
    ConfigNode* curr = g_cfg;
    while (curr != NULL) {
        if (strcmp(curr->key, key) == 0) {
            curr->value = value;
            return;
        }
        curr = curr->next;
    }

    ConfigNode* n = malloc_or_fail(sizeof(ConfigNode));
    n->key        = strdup(key);
    n->value      = value;
    n->next       = g_cfg;
    g_cfg         = n;
}
