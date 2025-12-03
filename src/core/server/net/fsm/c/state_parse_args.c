#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "xoe.h"

/* TLS includes for configuration */
#include "tls_config.h"

/* Forward declaration of print_usage (defined in xoe.c) */
void print_usage(const char *prog_name);

/**
 * @brief Parse command-line arguments
 *
 * Processes all command-line arguments and populates the configuration
 * structure. Handles both short options (-p, -i, etc.) and long options
 * (--cert, --key) in a unified manner.
 *
 * @param config Pointer to configuration structure to populate
 * @param argc Argument count from main()
 * @param argv Argument vector from main()
 * @return STATE_VALIDATE_CONFIG on success, STATE_CLEANUP on error or -h flag
 */
xoe_state_t state_parse_args(xoe_config_t *config, int argc, char *argv[]) {
    int opt = 0;
    char *colon = NULL;

    /* Save program name for usage messages */
    config->program_name = argv[0];

    /* Parse short options using getopt() */
    while ((opt = getopt(argc, argv, "i:p:c:e:h")) != -1) {
        switch (opt) {
            case 'i':
                config->listen_address = optarg;
                break;

            case 'p':
                config->listen_port = atoi(optarg);
                if (config->listen_port <= 0 || config->listen_port > 65535) {
                    fprintf(stderr, "Invalid port number: %s\n", optarg);
                    print_usage(argv[0]);
                    config->exit_code = EXIT_FAILURE;
                    return STATE_CLEANUP;
                }
                break;

            case 'c':
                colon = strchr(optarg, ':');
                if (colon == NULL) {
                    fprintf(stderr, "Invalid server address format. Expected <ip>:<port>.\n");
                    print_usage(argv[0]);
                    config->exit_code = EXIT_FAILURE;
                    return STATE_CLEANUP;
                }
                *colon = '\0';
                config->connect_server_ip = optarg;
                config->connect_server_port = atoi(colon + 1);
                if (config->connect_server_port <= 0 ||
                    config->connect_server_port > 65535) {
                    fprintf(stderr, "Invalid server port number: %s\n", colon + 1);
                    print_usage(argv[0]);
                    config->exit_code = EXIT_FAILURE;
                    return STATE_CLEANUP;
                }
                break;

            case 'e':
#if TLS_ENABLED
                if (strcmp(optarg, "none") == 0) {
                    config->encryption_mode = ENCRYPT_NONE;
                } else if (strcmp(optarg, "tls12") == 0) {
                    config->encryption_mode = ENCRYPT_TLS12;
                } else if (strcmp(optarg, "tls13") == 0) {
                    config->encryption_mode = ENCRYPT_TLS13;
                } else {
                    fprintf(stderr, "Invalid encryption mode: %s\n", optarg);
                    fprintf(stderr, "Valid modes: none, tls12, tls13\n");
                    print_usage(argv[0]);
                    config->exit_code = EXIT_FAILURE;
                    return STATE_CLEANUP;
                }
#else
                fprintf(stderr, "TLS support not compiled in. Rebuild with TLS_ENABLED=1\n");
                config->exit_code = EXIT_FAILURE;
                return STATE_CLEANUP;
#endif
                break;

            case 'h':
                config->mode = MODE_HELP;
                print_usage(argv[0]);
                config->exit_code = EXIT_SUCCESS;
                return STATE_CLEANUP;

            default:
                print_usage(argv[0]);
                config->exit_code = EXIT_FAILURE;
                return STATE_CLEANUP;
        }
    }

#if TLS_ENABLED
    /* Parse remaining arguments for long options (--cert and --key) */
    while (optind < argc) {
        if (strcmp(argv[optind], "-cert") == 0 ||
            strcmp(argv[optind], "--cert") == 0) {
            if (optind + 1 >= argc) {
                fprintf(stderr, "Option %s requires an argument\n", argv[optind]);
                print_usage(argv[0]);
                config->exit_code = EXIT_FAILURE;
                return STATE_CLEANUP;
            }
            strncpy(config->cert_path, argv[optind + 1], TLS_CERT_PATH_MAX - 1);
            config->cert_path[TLS_CERT_PATH_MAX - 1] = '\0';
            optind += 2;
        } else if (strcmp(argv[optind], "-key") == 0 ||
                   strcmp(argv[optind], "--key") == 0) {
            if (optind + 1 >= argc) {
                fprintf(stderr, "Option %s requires an argument\n", argv[optind]);
                print_usage(argv[0]);
                config->exit_code = EXIT_FAILURE;
                return STATE_CLEANUP;
            }
            strncpy(config->key_path, argv[optind + 1], TLS_CERT_PATH_MAX - 1);
            config->key_path[TLS_CERT_PATH_MAX - 1] = '\0';
            optind += 2;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[optind]);
            print_usage(argv[0]);
            config->exit_code = EXIT_FAILURE;
            return STATE_CLEANUP;
        }
    }
#endif

    return STATE_VALIDATE_CONFIG;
}
