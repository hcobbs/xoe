#include "mgmt_commands.h"
#include "mgmt_config.h"
#include "core/config.h"
#include "core/server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

/**
 * Management Command Implementation
 *
 * All commands use session's pre-allocated buffers.
 * No dynamic allocation - memory-safe by design.
 */

/* Command handler function pointer */
typedef int (*cmd_handler_t)(mgmt_session_t *session, int argc, char **argv);

/* Command dispatch entry */
typedef struct {
    const char *name;
    cmd_handler_t handler;
    const char *help;
} cmd_entry_t;

/* Forward declarations */
static int cmd_help(mgmt_session_t *session, int argc, char **argv);
static int cmd_show(mgmt_session_t *session, int argc, char **argv);
static int cmd_set(mgmt_session_t *session, int argc, char **argv);
static int cmd_get(mgmt_session_t *session, int argc, char **argv);
static int cmd_pending(mgmt_session_t *session, int argc, char **argv);
static int cmd_clear(mgmt_session_t *session, int argc, char **argv);
static int cmd_validate(mgmt_session_t *session, int argc, char **argv);
static int cmd_restart(mgmt_session_t *session, int argc, char **argv);
static int cmd_quit(mgmt_session_t *session, int argc, char **argv);
static int cmd_shutdown(mgmt_session_t *session, int argc, char **argv);

/* Command dispatch table */
static const cmd_entry_t commands[] = {
    {"help",     cmd_help,     "Display available commands"},
    {"show",     cmd_show,     "Display status/config/clients"},
    {"set",      cmd_set,      "Set configuration parameter"},
    {"get",      cmd_get,      "Get configuration parameter"},
    {"pending",  cmd_pending,  "Show pending changes"},
    {"clear",    cmd_clear,    "Clear pending changes"},
    {"validate", cmd_validate, "Validate pending config"},
    {"restart",  cmd_restart,  "Apply changes and restart"},
    {"quit",     cmd_quit,     "Close session"},
    {"shutdown", cmd_shutdown, "Shutdown application"},
    {NULL, NULL, NULL}
};

/* Helper: Send string to session */
static void send_str(mgmt_session_t *session, const char *str) {
    write(session->socket_fd, str, strlen(str));
}

/* Helper: Send formatted string */
static void send_fmt(mgmt_session_t *session, const char *fmt, ...) {
    va_list args;
    int len;

    va_start(args, fmt);
    len = vsnprintf(session->write_buffer, MGMT_BUFFER_SIZE, fmt, args);
    va_end(args);

    if (len > 0 && len < MGMT_BUFFER_SIZE) {
        write(session->socket_fd, session->write_buffer, len);
    }
}

/* Helper: Parse command line into argc/argv */
static int parse_command(char *line, int *argc, char **argv, int max_args) {
    int count = 0;
    char *p = line;

    while (*p && count < max_args) {
        while (*p && isspace((unsigned char)*p)) {
            p++;
        }

        if (*p == '\0') {
            break;
        }

        argv[count++] = p;

        while (*p && !isspace((unsigned char)*p)) {
            p++;
        }

        if (*p) {
            *p++ = '\0';
        }
    }

    *argc = count;
    return count;
}

/**
 * mgmt_command_loop - Main command processing loop
 */
void mgmt_command_loop(mgmt_session_t *session) {
    ssize_t bytes_read;
    char *newline;
    int argc;
    char *argv[16];
    int i;
    int found;
    const char *prompt = "xoe> ";

    while (1) {
        send_str(session, prompt);

        bytes_read = read(session->socket_fd, session->read_buffer,
                         MGMT_BUFFER_SIZE - 1);
        if (bytes_read <= 0) {
            break;
        }

        session->read_buffer[bytes_read] = '\0';

        newline = strchr(session->read_buffer, '\n');
        if (newline) *newline = '\0';
        newline = strchr(session->read_buffer, '\r');
        if (newline) *newline = '\0';

        if (session->read_buffer[0] == '\0') {
            continue;
        }

        parse_command(session->read_buffer, &argc, argv, 16);
        if (argc == 0) {
            continue;
        }

        found = 0;
        for (i = 0; commands[i].name != NULL; i++) {
            if (strcmp(argv[0], commands[i].name) == 0) {
                if (commands[i].handler(session, argc, argv) != 0) {
                    return;
                }
                found = 1;
                break;
            }
        }

        if (!found) {
            send_fmt(session, "Unknown command: %s (type 'help' for list)\n",
                    argv[0]);
        }
    }
}

static int cmd_help(mgmt_session_t *session, int argc, char **argv) {
    int i;
    (void)argc;
    (void)argv;

    send_str(session, "\nAvailable Commands:\n");
    for (i = 0; commands[i].name != NULL; i++) {
        send_fmt(session, "  %-12s - %s\n", commands[i].name, commands[i].help);
    }
    send_str(session, "\n");

    return 0;
}

static int cmd_show(mgmt_session_t *session, int argc, char **argv) {
    if (argc < 2) {
        send_str(session, "Usage: show {config|status|clients}\n");
        return 0;
    }

    if (strcmp(argv[1], "config") == 0) {
        send_str(session, "\n=== Active Configuration ===\n");
        send_str(session, "(Full config display coming in next iteration)\n");
        if (g_config_manager != NULL) {
            int mode = mgmt_config_get_mode(g_config_manager);
            send_fmt(session, "Mode: %s\n",
                    mode == MODE_SERVER ? "server" : "client");
            send_fmt(session, "Listen Port: %d\n",
                    mgmt_config_get_listen_port(g_config_manager));
        }
        send_str(session, "\n");

    } else if (strcmp(argv[1], "status") == 0) {
        int clients = get_active_client_count();
        int has_pending = 0;

        if (g_config_manager != NULL) {
            has_pending = mgmt_config_has_pending(g_config_manager);
        }

        send_str(session, "\n=== Server Status ===\n");
        send_fmt(session, "Active Clients: %d\n", clients);
        send_fmt(session, "Pending Changes: %s\n",
                has_pending ? "yes" : "no");
        send_fmt(session, "Restart Flag: %s\n",
                mgmt_restart_is_requested() ? "SET" : "clear");
        send_str(session, "\n");

    } else if (strcmp(argv[1], "clients") == 0) {
        int clients = get_active_client_count();
        send_str(session, "\n=== Connected Clients ===\n");
        send_fmt(session, "Active Connections: %d\n", clients);
        send_str(session, "\n");

    } else {
        send_fmt(session, "Unknown show target: %s\n", argv[1]);
    }

    return 0;
}

static int cmd_set(mgmt_session_t *session, int argc, char **argv) {
    const char *param;
    const char *value;

    if (argc < 3) {
        send_str(session, "Usage: set <parameter> <value>\n");
        send_str(session, "Parameters: mode, port, listen_addr\n");
        return 0;
    }

    if (g_config_manager == NULL) {
        send_str(session, "Error: Config manager not initialized\n");
        return 0;
    }

    param = argv[1];
    value = argv[2];

    if (strcmp(param, "mode") == 0) {
        xoe_mode_t mode;
        if (strcmp(value, "server") == 0) {
            mode = MODE_SERVER;
        } else if (strcmp(value, "client") == 0) {
            mode = MODE_CLIENT_STANDARD;
        } else {
            send_fmt(session, "Invalid mode: %s\n", value);
            return 0;
        }
        mgmt_config_set_mode(g_config_manager, mode);
        send_fmt(session, "Pending: mode = %s\n", value);

    } else if (strcmp(param, "port") == 0) {
        char *endptr;
        long port;
        errno = 0;
        port = strtol(value, &endptr, 10);
        if (errno == ERANGE || *endptr != '\0' || port <= 0 || port > 65535) {
            send_str(session, "Invalid port\n");
            return 0;
        }
        mgmt_config_set_listen_port(g_config_manager, (int)port);
        send_fmt(session, "Pending: port = %ld\n", port);

    } else {
        send_fmt(session, "Unknown parameter: %s\n", param);
    }

    return 0;
}

static int cmd_get(mgmt_session_t *session, int argc, char **argv) {
    const char *param;

    if (argc < 2) {
        send_str(session, "Usage: get <parameter>\n");
        return 0;
    }

    if (g_config_manager == NULL) {
        send_str(session, "Error: Config manager not initialized\n");
        return 0;
    }

    param = argv[1];

    if (strcmp(param, "mode") == 0) {
        int mode = mgmt_config_get_mode(g_config_manager);
        send_fmt(session, "mode: %s\n",
                mode == MODE_SERVER ? "server" : "client");

    } else if (strcmp(param, "port") == 0) {
        int port = mgmt_config_get_listen_port(g_config_manager);
        send_fmt(session, "port: %d\n", port);

    } else {
        send_fmt(session, "Unknown parameter: %s\n", param);
    }

    return 0;
}

static int cmd_pending(mgmt_session_t *session, int argc, char **argv) {
    (void)argc;
    (void)argv;

    if (g_config_manager == NULL) {
        send_str(session, "Error: Config manager not initialized\n");
        return 0;
    }

    if (!mgmt_config_has_pending(g_config_manager)) {
        send_str(session, "No pending changes\n");
        return 0;
    }

    send_str(session, "\n=== Pending Changes ===\n");
    send_str(session, "(Detailed diff coming in next iteration)\n");
    send_str(session, "Use 'validate' to check, 'restart' to apply, or 'clear' to discard\n\n");

    return 0;
}

static int cmd_clear(mgmt_session_t *session, int argc, char **argv) {
    (void)argc;
    (void)argv;

    if (g_config_manager == NULL) {
        send_str(session, "Error: Config manager not initialized\n");
        return 0;
    }

    if (!mgmt_config_has_pending(g_config_manager)) {
        send_str(session, "No pending changes to clear\n");
        return 0;
    }

    mgmt_config_clear_pending(g_config_manager);
    send_str(session, "Pending changes cleared\n");

    return 0;
}

static int cmd_validate(mgmt_session_t *session, int argc, char **argv) {
    char error_buf[256];
    int result;

    (void)argc;
    (void)argv;

    if (g_config_manager == NULL) {
        send_str(session, "Error: Config manager not initialized\n");
        return 0;
    }

    if (!mgmt_config_has_pending(g_config_manager)) {
        send_str(session, "No pending changes to validate\n");
        return 0;
    }

    result = mgmt_config_validate_pending(g_config_manager, error_buf, sizeof(error_buf));
    if (result != 0) {
        send_fmt(session, "Validation failed: %s\n", error_buf);
        return 0;
    }

    send_str(session, "Validation passed - configuration is valid\n");
    send_str(session, "Use 'restart' to apply changes\n");

    return 0;
}

static int cmd_restart(mgmt_session_t *session, int argc, char **argv) {
    char error_buf[256];
    int result;

    (void)argc;
    (void)argv;

    if (g_config_manager == NULL) {
        send_str(session, "Error: Config manager not initialized\n");
        return 0;
    }

    if (!mgmt_config_has_pending(g_config_manager)) {
        send_str(session, "No pending changes - nothing to apply\n");
        return 0;
    }

    result = mgmt_config_validate_pending(g_config_manager, error_buf, sizeof(error_buf));
    if (result != 0) {
        send_fmt(session, "Cannot restart - validation failed: %s\n", error_buf);
        return 0;
    }

    mgmt_restart_request();

    send_str(session, "\nRestart initiated\n");
    send_str(session, "Management session will remain active\n\n");

    return 0;
}

static int cmd_quit(mgmt_session_t *session, int argc, char **argv) {
    (void)argc;
    (void)argv;

    send_str(session, "Goodbye\n");

    return 1;
}

static int cmd_shutdown(mgmt_session_t *session, int argc, char **argv) {
    (void)argc;
    (void)argv;

    send_str(session, "TODO: Application shutdown not yet implemented\n");

    return 0;
}
