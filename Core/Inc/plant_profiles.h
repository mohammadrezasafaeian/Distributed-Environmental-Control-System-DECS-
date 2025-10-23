#ifndef PLANT_PROFILES_H
#define PLANT_PROFILES_H

#include <stdint.h>

typedef struct {
    char name[17];
    uint16_t humidity_threshold;
    uint16_t temp_threshold;
    uint16_t light_threshold;
    uint8_t irrigation_interval_sec;
    uint8_t irrigation_duration_sec;
} PlantProfile_t;

// Public API
void plant_profiles_init(void);
uint8_t get_num_profiles(void);
PlantProfile_t* get_profile(uint8_t index);
const char* get_profile_name(uint8_t index);

#endif
