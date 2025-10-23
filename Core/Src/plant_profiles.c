#include "plant_profiles.h"
#include <stddef.h>

// Private database - ADD MORE PROFILES HERE TO SCALE
static PlantProfile_t profiles[] = {
    {"TOMATO", 450, 280, 650, 30, 5},
    {"PEPPER", 420, 260, 600, 28, 4},
    {"LETTUCE", 500, 200, 400, 20, 3},
    {"HERBS", 480, 240, 550, 25, 4},
    {"BASIL", 470, 250, 580, 22, 4},      // ADD THESE
    {"CUCUMBER", 490, 270, 620, 35, 6},   // TO TEST
    {"STRAWBERRY", 460, 240, 600, 28, 5}, // SCROLLING
};

void plant_profiles_init(void) {
    // Reserved for future use
}

uint8_t get_num_profiles(void) {
    return sizeof(profiles) / sizeof(profiles[0]);
}

PlantProfile_t* get_profile(uint8_t index) {
    if (index >= get_num_profiles()) return NULL;
    return &profiles[index];
}

const char* get_profile_name(uint8_t index) {
    PlantProfile_t* p = get_profile(index);
    return (p != NULL) ? p->name : "INVALID";
}
