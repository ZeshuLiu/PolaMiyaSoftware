#ifndef VIEWFINDER_LSM6DS3TRC_H
#define VIEWFINDER_LSM6DS3TRC_H

#include <stdbool.h>
#include <stdint.h>

/* Same output data rate for gyroscope and accelerometer: 26, 52 or 104 Hz. */
#ifndef LSM6DS3TRC_ODR_HZ
#define LSM6DS3TRC_ODR_HZ 26U
#endif

#define LSM6DS3TRC_GYRO_UDPS_PER_LSB 8750
#define LSM6DS3TRC_ACCEL_UG_PER_LSB    61

typedef struct {
    int16_t gyro_raw[3];  /* XYZ in sensor coordinates, +/-250 degrees/s full scale. */
    int16_t accel_raw[3]; /* XYZ in sensor coordinates, +/-2 g full scale. */
    uint32_t sequence;
} LSM6DS3TRC_Sample;

bool LSM6DS3TRC_Init(void);
/* Dispatches INT1 and SPI2 events in the main loop. */
void LSM6DS3TRC_Service(uint32_t events, uint32_t now);
/* Main-loop reader. Returns false until both sensors have reported a sample. */
bool LSM6DS3TRC_GetLatest(LSM6DS3TRC_Sample *sample);

#endif /* VIEWFINDER_LSM6DS3TRC_H */
