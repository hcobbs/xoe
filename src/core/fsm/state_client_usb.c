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
 * This is a Phase 1 stub implementation. Full USB transfer implementation
 * will be added in Phase 2.
 */

#include <stdio.h>
#include <stdlib.h>
#include "core/config.h"
#include "lib/common/definitions.h"
#include "connectors/usb/usb_config.h"
#include "connectors/usb/usb_device.h"

/**
 * state_client_usb - USB client mode handler (Phase 1 stub)
 * @config: Pointer to configuration structure
 *
 * Returns: STATE_CLEANUP (stub implementation)
 *
 * Phase 1 Implementation:
 * This is a minimal stub that validates the USB configuration and
 * provides basic error reporting. Full USB transfer implementation
 * will be added in Phase 2.
 *
 * Planned Phase 2 Features:
 * - USB device initialization and opening
 * - Multi-threaded USB transfer handling
 * - Network packet encapsulation/decapsulation
 * - Device manager for multi-device support
 * - Hotplug support
 */
xoe_state_t state_client_usb(xoe_config_t *config) {
    usb_multi_config_t *usb_multi = NULL;
    int i = 0;

    /* Validate config pointer */
    if (config == NULL) {
        fprintf(stderr, "Error: NULL configuration pointer\n");
        config->exit_code = EXIT_FAILURE;
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

    /* Display configured devices */
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

    for (i = 0; i < usb_multi->device_count; i++) {
        usb_config_t *dev_cfg = &usb_multi->devices[i];
        int result = 0;

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

    /* Phase 1 stub: Print status and exit */
    printf("========================================\n");
    printf("\n");
    printf("Phase 1 Status: Configuration validated successfully\n");
    printf("\n");
    printf("NOTE: Full USB transfer implementation will be added in Phase 2\n");
    printf("      This includes:\n");
    printf("      - USB device initialization and opening\n");
    printf("      - Multi-threaded transfer handling\n");
    printf("      - Network packet encapsulation\n");
    printf("      - Device manager for multi-device support\n");
    printf("\n");

    config->exit_code = EXIT_SUCCESS;
    return STATE_CLEANUP;
}
