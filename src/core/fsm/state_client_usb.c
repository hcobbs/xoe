/**
 * state_client_usb.c
 *
 * USB client mode state handler - bridges USB devices over network.
 *
 * EXPERIMENTAL FEATURE:
 * - This feature is HIGHLY EXPERIMENTAL
 * - During experimental phase, test coverage is not guaranteed
 * - Testing or using this connector can cause unexpected results
 * - People with domain knowledge are welcome to critique
 *
 * Phase 4 Implementation: Active USB transfers with threading
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "core/config.h"
#include "lib/common/definitions.h"
#include "connectors/usb/usb_config.h"
#include "connectors/usb/usb_device.h"
#include "connectors/usb/usb_client.h"

/* Global client pointer for signal handler */
static usb_client_t* g_usb_client = NULL;

/**
 * @brief Signal handler for graceful shutdown
 */
static void signal_handler(int signum)
{
    (void)signum;  /* Unused parameter */

    printf("\nReceived shutdown signal...\n");

    if (g_usb_client != NULL) {
        usb_client_stop(g_usb_client);
    }
}

/**
 * state_client_usb - USB client mode handler
 * @config: Pointer to configuration structure
 *
 * Returns: STATE_CLEANUP
 *
 * Phase 4 Implementation:
 * - USB client initialization
 * - Device opening and management
 * - Network connection establishment
 * - Active USB transfer threads (USB→Network data flow)
 * - Network receive thread (Network→USB routing)
 * - Multi-threaded architecture for concurrent device handling
 *
 * Future Phases:
 * - Full bidirectional transfer coordination (Phase 5)
 * - Request/response matching and routing
 * - Device manager for advanced multi-device scenarios
 * - Hotplug support (Phase 6)
 */
xoe_state_t state_client_usb(xoe_config_t *config) {
    usb_multi_config_t *usb_multi = NULL;
    usb_client_t *client = NULL;
    int i = 0;
    int result = 0;

    /* Validate config pointer */
    if (config == NULL) {
        fprintf(stderr, "Error: NULL configuration pointer\n");
        return STATE_CLEANUP;
    }

    /* Check if server connection is configured */
    if (config->connect_server_ip == NULL || config->connect_server_port == 0) {
        fprintf(stderr, "Error: Server connection not configured\n");
        fprintf(stderr, "Use -c <ip>:<port> to specify server\n");
        config->exit_code = EXIT_FAILURE;
        return STATE_CLEANUP;
    }

    /* Validate USB configuration */
    usb_multi = (usb_multi_config_t*)config->usb_config;
    if (usb_multi == NULL) {
        fprintf(stderr, "Error: USB configuration not initialized\n");
        config->exit_code = EXIT_FAILURE;
        return STATE_CLEANUP;
    }

    if (usb_multi->device_count == 0) {
        fprintf(stderr, "Error: No USB devices configured\n");
        fprintf(stderr, "Use -u <vid>:<pid> to specify USB device\n");
        fprintf(stderr, "Use --list-usb to see available devices\n");
        config->exit_code = EXIT_FAILURE;
        return STATE_CLEANUP;
    }

    /* Display header */
    printf("\n");
    printf("========================================\n");
    printf(" XOE USB Client Mode (EXPERIMENTAL)\n");
    printf("========================================\n");
    printf("\n");
    printf("WARNING: This feature is HIGHLY EXPERIMENTAL\n");
    printf("         Testing may cause unexpected results\n");
    printf("\n");
    printf("Server: %s:%d\n", config->connect_server_ip, config->connect_server_port);
    printf("Configured USB Devices: %d\n", usb_multi->device_count);
    printf("\n");

    /* Display and validate device configurations */
    for (i = 0; i < usb_multi->device_count; i++) {
        usb_config_t *dev_cfg = &usb_multi->devices[i];

        printf("Device %d:\n", i + 1);
        printf("  VID:PID: %04x:%04x\n", dev_cfg->vendor_id, dev_cfg->product_id);
        printf("  Interface: %d\n", dev_cfg->interface_number);

        if (dev_cfg->bulk_in_endpoint != USB_NO_ENDPOINT) {
            printf("  Bulk IN endpoint: 0x%02x\n", dev_cfg->bulk_in_endpoint);
        }
        if (dev_cfg->bulk_out_endpoint != USB_NO_ENDPOINT) {
            printf("  Bulk OUT endpoint: 0x%02x\n", dev_cfg->bulk_out_endpoint);
        }
        if (dev_cfg->interrupt_in_endpoint != USB_NO_ENDPOINT) {
            printf("  Interrupt IN endpoint: 0x%02x\n", dev_cfg->interrupt_in_endpoint);
        }

        /* Validate configuration */
        result = usb_config_validate(dev_cfg);
        if (result != 0) {
            fprintf(stderr, "\nError: Device %d configuration is invalid (error %d)\n",
                    i + 1, result);
            fprintf(stderr, "Please check VID:PID and endpoint settings\n");
            config->exit_code = EXIT_FAILURE;
            return STATE_CLEANUP;
        }

        printf("  Configuration: VALID\n");
        printf("\n");
    }

    printf("========================================\n");
    printf("\n");

    /* Initialize USB client */
    printf("Initializing USB client...\n");
    client = usb_client_init(config->connect_server_ip,
                             config->connect_server_port,
                             usb_multi->max_devices);
    if (client == NULL) {
        fprintf(stderr, "Error: Failed to initialize USB client\n");
        config->exit_code = EXIT_FAILURE;
        return STATE_CLEANUP;
    }

    /* Add devices to client */
    printf("Adding USB devices...\n");
    for (i = 0; i < usb_multi->device_count; i++) {
        result = usb_client_add_device(client, &usb_multi->devices[i]);
        if (result != 0) {
            fprintf(stderr, "Error: Failed to add device %d (error %d)\n",
                    i + 1, result);
            usb_client_cleanup(client);
            config->exit_code = EXIT_FAILURE;
            return STATE_CLEANUP;
        }
    }

    /* Set up signal handler for graceful shutdown */
    g_usb_client = client;
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Start client operation */
    printf("\nStarting USB client...\n");
    result = usb_client_start(client);
    if (result != 0) {
        fprintf(stderr, "Error: Failed to start USB client (error %d)\n", result);
        usb_client_cleanup(client);
        g_usb_client = NULL;
        config->exit_code = EXIT_FAILURE;
        return STATE_CLEANUP;
    }

    /* Wait for shutdown */
    usb_client_wait(client);

    /* Print statistics */
    usb_client_print_stats(client);

    /* Cleanup */
    usb_client_cleanup(client);
    g_usb_client = NULL;

    printf("\n");
    printf("Phase 4 Status: USB client with active transfers completed\n");
    printf("\n");
    printf("NOTE: Advanced features will be added in future phases:\n");
    printf("      - Bidirectional request/response coordination (Phase 5)\n");
    printf("      - Advanced multi-device routing and management\n");
    printf("      - Hotplug device detection and handling (Phase 6)\n");
    printf("\n");

    config->exit_code = EXIT_SUCCESS;
    return STATE_CLEANUP;
}
