#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    PET_COMMAND_NONE = 0,
    PET_COMMAND_GREETING = 1,
    PET_COMMAND_HAPPY = 2,
    PET_COMMAND_SLEEP = 3,
} pet_command_id_t;

typedef enum {
    PET_EXPRESSION_IDLE = 0,
    PET_EXPRESSION_GREETING,
    PET_EXPRESSION_HAPPY,
    PET_EXPRESSION_SLEEPY,
} pet_expression_t;

pet_expression_t pet_command_expression(uint16_t command_id, bool *known);
