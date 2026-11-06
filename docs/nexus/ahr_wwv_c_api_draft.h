/* Proposed C ABI for AHR WWV/WWVH in Nexus; DRAFT ONLY, NOT IMPLEMENTED.
 * Review with Claude before any build or merger. GPL-3.0-or-later.
 * Samples: mono float PCM; carrier_offset_hz is observed audio tone frequency
 * (nominal 1500 Hz in USB with dial 1.5 kHz below WWV).
 * Time units: Unix UTC seconds; positive clock offset = PC fast.
 * This API is single-thread-owned; no exceptions or Rust types cross the ABI.
 */
#ifndef AHR_WWV_C_API_DRAFT_H
#define AHR_WWV_C_API_DRAFT_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define AHR_WWV_ABI_VERSION 0u
#define AHR_WWV_FRAME_CELLS 60u
typedef struct ahr_wwv_handle ahr_wwv_handle;
enum ahr_wwv_state { AHR_WWV_SEARCHING=0, AHR_WWV_SECOND_LOCK=1,
 AHR_WWV_FRAME_LOCK=2, AHR_WWV_DECODED=3, AHR_WWV_VERIFIED=4,
 AHR_WWV_HOLDOVER=5 };
enum ahr_wwv_station { AHR_WWV_STATION_UNKNOWN=0, AHR_WWV_STATION_WWV=1,
 AHR_WWV_STATION_WWVH=2, AHR_WWV_STATION_BOTH=3 };
enum ahr_wwv_symbol { AHR_WWV_NONE=0, AHR_WWV_ZERO=1,
 AHR_WWV_ONE=2, AHR_WWV_MARKER=3, AHR_WWV_UNKNOWN=4 };
typedef struct ahr_wwv_config {
 uint32_t struct_size;
 double sample_rate_hz;
 double carrier_offset_hz;
 double propagation_delay_seconds;
 double capture_latency_seconds;
} ahr_wwv_config;
typedef struct ahr_wwv_cell {
 uint8_t symbol;
 uint8_t received;
 uint16_t reserved;
 float confidence;
} ahr_wwv_cell;
typedef struct ahr_wwv_status {
 uint32_t struct_size;
 uint32_t abi_version;
 int32_t state;
 int32_t station;
 uint8_t utc_valid;
 uint8_t clock_offset_valid;
 uint8_t leap_second_warning;
 uint8_t dst1;
 uint8_t dst2;
 uint8_t reserved_flags[3];
 double utc_unix_seconds;
 double clock_offset_seconds;
 double time_uncertainty_seconds;
 double second_lock_quality;
 double subcarrier_snr_db;
 int32_t frame_position;
 int32_t frames_verified;
 int32_t year;
 int32_t day_of_year;
 int32_t hour;
 int32_t minute;
 int32_t dut1_tenths;
 ahr_wwv_cell cells[AHR_WWV_FRAME_CELLS];
} ahr_wwv_status;
/* create returns NULL on invalid config/allocation failure */
ahr_wwv_handle *ahr_wwv_create(const ahr_wwv_config *config);
void ahr_wwv_destroy(ahr_wwv_handle *handle);
/* Use NaN for unknown first_sample_pc_unix_seconds.
 * Return 0 on success, nonzero error. Rate/offset specified by config.
 */
int32_t ahr_wwv_push_audio(ahr_wwv_handle *handle, const float *samples,
                           size_t count, double first_sample_pc_unix_seconds);
int32_t ahr_wwv_poll(ahr_wwv_handle *handle, ahr_wwv_status *status);
void ahr_wwv_reset(ahr_wwv_handle *handle);
#ifdef __cplusplus
}
#endif
#endif
