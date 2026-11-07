/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 KL5NE, Alaska HamRadio, and AHR-Decoder contributors */
/* Compiles the public header as plain C (C99) and exercises the API's
 * argument checking. The on-air behaviour is tested in test_capi.cpp. */
#include "ahr_wwv.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

int ahr_wwv_c_header_checks(void)
{
    int fails = 0;
    ahr_wwv_config c;
    ahr_wwv_status s;
    ahr_wwv_handle *h;

    if (ahr_wwv_abi_version() != AHR_WWV_ABI_VERSION) { printf("  FAIL abi version\n"); fails++; }
    ahr_wwv_config_default(&c);
    if (c.struct_size != sizeof c || c.sample_rate_hz != 48000.0 || c.carrier_hz != 1500.0) { printf("  FAIL defaults\n"); fails++; }

    c.sample_rate_hz = 4000.0; /* too low */
    if (ahr_wwv_create(&c) != NULL) { printf("  FAIL accepted 4 kHz\n"); fails++; }
    ahr_wwv_config_default(&c);
    c.struct_size = 8; /* caller built against a different header */
    if (ahr_wwv_create(&c) != NULL) { printf("  FAIL accepted short config\n"); fails++; }

    h = ahr_wwv_create(NULL);
    if (!h) { printf("  FAIL create with defaults\n"); return fails + 1; }
    memset(&s, 0, sizeof s);
    s.struct_size = 4;
    if (ahr_wwv_poll(h, &s) != AHR_WWV_ERR_STRUCT_SIZE) { printf("  FAIL short status accepted\n"); fails++; }
    s.struct_size = sizeof s;
    if (ahr_wwv_poll(h, &s) != AHR_WWV_OK || s.state != AHR_WWV_SEARCHING || s.utc_valid || s.clock_offset_valid) {
        printf("  FAIL fresh status\n"); fails++;
    }
    if (ahr_wwv_push_audio(h, NULL, 10, NAN) != AHR_WWV_ERR_NULL) { printf("  FAIL NULL samples\n"); fails++; }
    if (ahr_wwv_push_audio(NULL, NULL, 0, NAN) != AHR_WWV_ERR_NULL) { printf("  FAIL NULL handle\n"); fails++; }
    if (strcmp(ahr_wwv_state_name(AHR_WWV_VERIFIED), "Verified") != 0) { printf("  FAIL state name\n"); fails++; }
    if (ahr_wwv_reset(h) != AHR_WWV_OK) { printf("  FAIL reset\n"); fails++; }
    ahr_wwv_destroy(h);
    ahr_wwv_destroy(NULL);
    return fails;
}
