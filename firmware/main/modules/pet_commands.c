#include <stddef.h>
#include "pet_commands.h"

pet_expression_t pet_command_expression(uint16_t command_id, bool *known)
{
    if (known != NULL) {
        *known = true;
    }
    switch (command_id) {
    case PET_COMMAND_GREETING: return PET_EXPRESSION_GREETING;
    case PET_COMMAND_HAPPY: return PET_EXPRESSION_HAPPY;
    case PET_COMMAND_SLEEP: return PET_EXPRESSION_SLEEPY;
    default:
        if (known != NULL) {
            *known = false;
        }
        return PET_EXPRESSION_IDLE;
    }
}
