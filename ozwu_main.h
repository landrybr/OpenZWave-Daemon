#ifndef OZWU_MAIN_H
#define OZWU_MAIN_H
#include <stdint.h>

typedef enum {
    OZWU_NO_COMMAND,
    OZWU_HELP,
    OZWU_KILL,
    OZWU_ADD_NODE,
    OZWU_REMOVE_NODE,
    OZWU_SET_DIMMER,
    OZWU_QUERY,
    OZWU_CHECK
    //OZWU_VERSION,
    //DAEMON_RELOAD,
    //DAEMON_CHECK
} Command;

typedef struct {
    Command command;
    //int has_command;
    int daemonize;
    int has_log;
    int has_node_name;
    //int has_set_node_name;
    int has_node_number;
    int set_node_name;
    uint8_t level;
    uint8_t node_number;
    char *device;
    char *node_name;
    char *new_node_name;
    char *log_name;
    FILE *log;
} Config;


#endif
