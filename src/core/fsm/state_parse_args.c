/**
 * state_parse_args.c
 *
 * Parses command-line arguments and populates the configuration structure.
 * Handles both short options (via getopt) and long options (manual parsing).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include "../../../../../../lib/getopt/getopt.h"
#else
#include <unistd.h>
#endif

#include "core/config.h"
#include "lib/common/definitions.h"
#include "connectors/serial/serial_config.h"
#include "connectors/usb/usb_config.h"
#include "connectors/usb/usb_device.h"

#if TLS_ENABLED
#include "lib/security/tls_context.h"
#endif

/**
 * state_parse_args - Parse command-line arguments
 * @config: Pointer to configuration structure
 * @argc: Argument count from main()
 * @argv: Argument vector from main()
 *
 * Returns: STATE_VALIDATE_CONFIG on success, STATE_CLEANUP on help/error
 *
 * Parses command-line options in two phases:
 * 1. Short options via getopt: -i, -p, -c, -e, -s, -b, -h
 * 2. Long options via manual parsing: --cert, --key, --parity, --databits,
 *    --stopbits, --flow
 *
 * Updates config structure with parsed values and validates input ranges.
 */
xoe_state_t state_parse_args(xoe_config_t *config, int argc, char *argv[]) {
    int opt = 0;
    char *colon = NULL;
    serial_config_t *serial_cfg = (serial_config_t*)config->serial_config;

    /* Store program name for usage output */
    config->program_name = argv[0];

    /* Phase 1: Parse short options with getopt */
    while ((opt = getopt(argc, argv, "i:p:c:e:s:b:u:h")) != -1) {
        switch (opt) {
            case 'i':
                config->listen_address = optarg;
                break;

            case 'p':
                config->listen_port = atoi(optarg);
                if (config->listen_port <= 0 || config->listen_port > 65535) {
                    fprintf(stderr, "Invalid port number: %s\n", optarg);
                    print_usage(config->program_name);
                    config->exit_code = EXIT_FAILURE;
                    return STATE_CLEANUP;
                }
                break;

            case 'c':
                colon = strchr(optarg, ':');
                if (colon == NULL) {
                    fprintf(stderr, "Invalid server address format. Expected <ip>:<port>.\n");
                    print_usage(config->program_name);
                    config->exit_code = EXIT_FAILURE;
                    return STATE_CLEANUP;
                }
                *colon = '\0';
                config->connect_server_ip = optarg;
                config->connect_server_port = atoi(colon + 1);
                if (config->connect_server_port <= 0 || config->connect_server_port > 65535) {
                    fprintf(stderr, "Invalid server port number: %s\n", colon + 1);
                    print_usage(config->program_name);
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
                    print_usage(config->program_name);
                    config->exit_code = EXIT_FAILURE;
                    return STATE_CLEANUP;
                }
#else
                fprintf(stderr, "TLS support not compiled in. Rebuild with TLS_ENABLED=1\n");
                config->exit_code = EXIT_FAILURE;
                return STATE_CLEANUP;
#endif
                break;

            case 's':
                config->serial_device = optarg;
                config->use_serial = TRUE;
                break;

            case 'b':
                if (serial_cfg != NULL) {
                    serial_cfg->baud_rate = atoi(optarg);
                    if (serial_cfg->baud_rate <= 0) {
                        fprintf(stderr, "Invalid baud rate: %s\n", optarg);
                        print_usage(config->program_name);
                        config->exit_code = EXIT_FAILURE;
                        return STATE_CLEANUP;
                    }
                }
                break;

            case 'u': {
                /* Parse VID:PID format (hex values) */
                usb_multi_config_t *usb_multi = (usb_multi_config_t*)config->usb_config;
                unsigned int vid = 0, pid = 0;

                if (usb_multi == NULL) {
                    fprintf(stderr, "Error: USB configuration not initialized\n");
                    config->exit_code = EXIT_FAILURE;
                    return STATE_CLEANUP;
                }

                /* Check if we have room for another device */
                if (usb_multi->device_count >= usb_multi->max_devices) {
                    fprintf(stderr, "Error: Maximum number of USB devices (%d) exceeded\n",
                            usb_multi->max_devices);
                    config->exit_code = EXIT_FAILURE;
                    return STATE_CLEANUP;
                }

                /* Parse VID:PID (format: 0xVVVV:0xPPPP or VVVV:PPPP) */
                if (sscanf(optarg, "%x:%x", &vid, &pid) != 2) {
                    fprintf(stderr, "Invalid USB device format: %s\n", optarg);
                    fprintf(stderr, "Expected format: VID:PID (hex, e.g., 046d:c52b)\n");
                    print_usage(config->program_name);
                    config->exit_code = EXIT_FAILURE;
                    return STATE_CLEANUP;
                }

                /* Validate VID/PID ranges */
                if (vid == 0 || vid > 0xFFFF || pid == 0 || pid > 0xFFFF) {
                    fprintf(stderr, "Invalid VID:PID values: %04x:%04x\n", vid, pid);
                    config->exit_code = EXIT_FAILURE;
                    return STATE_CLEANUP;
                }

                /* Initialize new device slot with defaults */
                usb_config_init_defaults(&usb_multi->devices[usb_multi->device_count]);
                usb_multi->devices[usb_multi->device_count].vendor_id = (uint16_t)vid;
                usb_multi->devices[usb_multi->device_count].product_id = (uint16_t)pid;

                /* Increment device count */
                usb_multi->device_count++;

                config->use_usb = TRUE;
                break;
            }

            case 'h':
                print_usage(config->program_name);
                config->mode = MODE_HELP;
                config->exit_code = EXIT_SUCCESS;
                return STATE_CLEANUP;

            default:
                print_usage(config->program_name);
                config->exit_code = EXIT_FAILURE;
                return STATE_CLEANUP;
        }
    }

    /* Phase 2: Parse long options manually */
    while (optind < argc) {
#if TLS_ENABLED
        if (strcmp(argv[optind], "-cert") == 0 || strcmp(argv[optind], "--cert") == 0) {
            if (optind + 1 >= argc) {
                fprintf(stderr, "Option %s requires an argument\n", argv[optind]);
                print_usage(config->program_name);
                config->exit_code = EXIT_FAILURE;
                return STATE_CLEANUP;
            }
            strncpy(config->cert_path, argv[optind + 1], TLS_CERT_PATH_MAX - 1);
            config->cert_path[TLS_CERT_PATH_MAX - 1] = '\0';
            optind += 2;
        } else if (strcmp(argv[optind], "-key") == 0 || strcmp(argv[optind], "--key") == 0) {
            if (optind + 1 >= argc) {
                fprintf(stderr, "Option %s requires an argument\n", argv[optind]);
                print_usage(config->program_name);
                config->exit_code = EXIT_FAILURE;
                return STATE_CLEANUP;
            }
            strncpy(config->key_path, argv[optind + 1], TLS_CERT_PATH_MAX - 1);
            config->key_path[TLS_CERT_PATH_MAX - 1] = '\0';
            optind += 2;
        } else
#endif
        if (strcmp(argv[optind], "--parity") == 0) {
            if (optind + 1 >= argc) {
                fprintf(stderr, "Option --parity requires an argument\n");
                print_usage(config->program_name);
                config->exit_code = EXIT_FAILURE;
                return STATE_CLEANUP;
            }
            if (serial_cfg != NULL) {
                if (strcmp(argv[optind + 1], "none") == 0) {
                    serial_cfg->parity = SERIAL_PARITY_NONE;
                } else if (strcmp(argv[optind + 1], "even") == 0) {
                    serial_cfg->parity = SERIAL_PARITY_EVEN;
                } else if (strcmp(argv[optind + 1], "odd") == 0) {
                    serial_cfg->parity = SERIAL_PARITY_ODD;
                } else {
                    fprintf(stderr, "Invalid parity: %s (use none, even, or odd)\n",
                            argv[optind + 1]);
                    config->exit_code = EXIT_FAILURE;
                    return STATE_CLEANUP;
                }
            }
            optind += 2;
        } else if (strcmp(argv[optind], "--databits") == 0) {
            if (optind + 1 >= argc) {
                fprintf(stderr, "Option --databits requires an argument\n");
                print_usage(config->program_name);
                config->exit_code = EXIT_FAILURE;
                return STATE_CLEANUP;
            }
            if (serial_cfg != NULL) {
                serial_cfg->data_bits = atoi(argv[optind + 1]);
                if (serial_cfg->data_bits != 7 && serial_cfg->data_bits != 8) {
                    fprintf(stderr, "Invalid data bits: %s (use 7 or 8)\n", argv[optind + 1]);
                    config->exit_code = EXIT_FAILURE;
                    return STATE_CLEANUP;
                }
            }
            optind += 2;
        } else if (strcmp(argv[optind], "--stopbits") == 0) {
            if (optind + 1 >= argc) {
                fprintf(stderr, "Option --stopbits requires an argument\n");
                print_usage(config->program_name);
                config->exit_code = EXIT_FAILURE;
                return STATE_CLEANUP;
            }
            if (serial_cfg != NULL) {
                serial_cfg->stop_bits = atoi(argv[optind + 1]);
                if (serial_cfg->stop_bits != 1 && serial_cfg->stop_bits != 2) {
                    fprintf(stderr, "Invalid stop bits: %s (use 1 or 2)\n", argv[optind + 1]);
                    config->exit_code = EXIT_FAILURE;
                    return STATE_CLEANUP;
                }
            }
            optind += 2;
        } else if (strcmp(argv[optind], "--flow") == 0) {
            if (optind + 1 >= argc) {
                fprintf(stderr, "Option --flow requires an argument\n");
                print_usage(config->program_name);
                config->exit_code = EXIT_FAILURE;
                return STATE_CLEANUP;
            }
            if (serial_cfg != NULL) {
                if (strcmp(argv[optind + 1], "none") == 0) {
                    serial_cfg->flow_control = SERIAL_FLOW_NONE;
                } else if (strcmp(argv[optind + 1], "xonxoff") == 0) {
                    serial_cfg->flow_control = SERIAL_FLOW_XONXOFF;
                } else if (strcmp(argv[optind + 1], "rtscts") == 0) {
                    serial_cfg->flow_control = SERIAL_FLOW_RTSCTS;
                } else {
                    fprintf(stderr, "Invalid flow control: %s (use none, xonxoff, or rtscts)\n",
                            argv[optind + 1]);
                    config->exit_code = EXIT_FAILURE;
                    return STATE_CLEANUP;
                }
            }
            optind += 2;
        } else if (strcmp(argv[optind], "--interface") == 0) {
            if (optind + 1 >= argc) {
                fprintf(stderr, "Option --interface requires an argument\n");
                print_usage(config->program_name);
                config->exit_code = EXIT_FAILURE;
                return STATE_CLEANUP;
            }
            /* Apply to most recently added USB device */
            {
                usb_multi_config_t *usb_multi = (usb_multi_config_t*)config->usb_config;
                int interface_num = atoi(argv[optind + 1]);
                if (usb_multi != NULL && usb_multi->device_count > 0) {
                    usb_multi->devices[usb_multi->device_count - 1].interface_number = interface_num;
                } else {
                    fprintf(stderr, "Error: --interface must follow -u option\n");
                    config->exit_code = EXIT_FAILURE;
                    return STATE_CLEANUP;
                }
            }
            optind += 2;
        } else if (strcmp(argv[optind], "--ep-in") == 0) {
            if (optind + 1 >= argc) {
                fprintf(stderr, "Option --ep-in requires an argument\n");
                print_usage(config->program_name);
                config->exit_code = EXIT_FAILURE;
                return STATE_CLEANUP;
            }
            /* Apply to most recently added USB device */
            {
                usb_multi_config_t *usb_multi = (usb_multi_config_t*)config->usb_config;
                unsigned int ep_addr = 0;
                if (sscanf(argv[optind + 1], "%x", &ep_addr) != 1 || ep_addr > 0xFF) {
                    fprintf(stderr, "Invalid endpoint address: %s (use hex, e.g., 81)\n",
                            argv[optind + 1]);
                    config->exit_code = EXIT_FAILURE;
                    return STATE_CLEANUP;
                }
                if (usb_multi != NULL && usb_multi->device_count > 0) {
                    usb_multi->devices[usb_multi->device_count - 1].bulk_in_endpoint =
                        (uint8_t)ep_addr;
                } else {
                    fprintf(stderr, "Error: --ep-in must follow -u option\n");
                    config->exit_code = EXIT_FAILURE;
                    return STATE_CLEANUP;
                }
            }
            optind += 2;
        } else if (strcmp(argv[optind], "--ep-out") == 0) {
            if (optind + 1 >= argc) {
                fprintf(stderr, "Option --ep-out requires an argument\n");
                print_usage(config->program_name);
                config->exit_code = EXIT_FAILURE;
                return STATE_CLEANUP;
            }
            /* Apply to most recently added USB device */
            {
                usb_multi_config_t *usb_multi = (usb_multi_config_t*)config->usb_config;
                unsigned int ep_addr = 0;
                if (sscanf(argv[optind + 1], "%x", &ep_addr) != 1 || ep_addr > 0xFF) {
                    fprintf(stderr, "Invalid endpoint address: %s (use hex, e.g., 01)\n",
                            argv[optind + 1]);
                    config->exit_code = EXIT_FAILURE;
                    return STATE_CLEANUP;
                }
                if (usb_multi != NULL && usb_multi->device_count > 0) {
                    usb_multi->devices[usb_multi->device_count - 1].bulk_out_endpoint =
                        (uint8_t)ep_addr;
                } else {
                    fprintf(stderr, "Error: --ep-out must follow -u option\n");
                    config->exit_code = EXIT_FAILURE;
                    return STATE_CLEANUP;
                }
            }
            optind += 2;
        } else if (strcmp(argv[optind], "--ep-int") == 0) {
            if (optind + 1 >= argc) {
                fprintf(stderr, "Option --ep-int requires an argument\n");
                print_usage(config->program_name);
                config->exit_code = EXIT_FAILURE;
                return STATE_CLEANUP;
            }
            /* Apply to most recently added USB device */
            {
                usb_multi_config_t *usb_multi = (usb_multi_config_t*)config->usb_config;
                unsigned int ep_addr = 0;
                if (sscanf(argv[optind + 1], "%x", &ep_addr) != 1 || ep_addr > 0xFF) {
                    fprintf(stderr, "Invalid endpoint address: %s (use hex, e.g., 83)\n",
                            argv[optind + 1]);
                    config->exit_code = EXIT_FAILURE;
                    return STATE_CLEANUP;
                }
                if (usb_multi != NULL && usb_multi->device_count > 0) {
                    usb_multi->devices[usb_multi->device_count - 1].interrupt_in_endpoint =
                        (uint8_t)ep_addr;
                } else {
                    fprintf(stderr, "Error: --ep-int must follow -u option\n");
                    config->exit_code = EXIT_FAILURE;
                    return STATE_CLEANUP;
                }
            }
            optind += 2;
        } else if (strcmp(argv[optind], "--list-usb") == 0) {
            /* List USB devices and exit */
            {
                struct libusb_context* ctx = NULL;
                int result = usb_device_init_library(&ctx);
                if (result != 0) {
                    fprintf(stderr, "Error: Failed to initialize libusb (error %d)\n", result);
                    config->exit_code = EXIT_FAILURE;
                    return STATE_CLEANUP;
                }
                printf("\nConnected USB Devices:\n");
                printf("======================\n");
                usb_device_enumerate(ctx);
                usb_device_cleanup_library(ctx);
            }
            config->exit_code = EXIT_SUCCESS;
            return STATE_CLEANUP;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[optind]);
            print_usage(config->program_name);
            config->exit_code = EXIT_FAILURE;
            return STATE_CLEANUP;
        }
    }

    return STATE_VALIDATE_CONFIG;
}
