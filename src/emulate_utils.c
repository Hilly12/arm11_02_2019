#include "emulate_utils.h"

// Checks if the sum of 2 integers gives an overflow (the sum is given as an argument as well)
byte check_overflow(unsigned int const *a, unsigned int const *b, unsigned int const *result) {
    return ((*a > 0 && *b > 0 && ((int) *result) < 0) || (((int) *a) < 0 && ((int) *b) < 0 && *result > 0));
}

// Check if Cond satisfies CPSR register [Pg. 4 spec]
byte is_condition_satisfied(unsigned int const *cpsr, byte const *condition) {
    byte important_bits = *cpsr >> 28;
    switch (*condition) {
        case EQ: // (Z == 1)
            return important_bits & 0x4;
        case NE: // (Z == 0)
            return !(important_bits & 0x4);
        case GE: // (N == V)
            return ((important_bits >> 3) | 0x1) == (important_bits | 0x1);
        case LT: // (N != V)
            return ((important_bits >> 3) | 0x1) != (important_bits | 0x1);
        case GT: // (Z == 0) && (N == V)
            return (!(important_bits & 0x4)) && ((important_bits >> 3) | 0x1) == (important_bits | 0x1);
        case LE: // (Z == 1) || (N != V)
            return (important_bits & 0x4) && ((important_bits >> 3) | 0x1) != (important_bits | 0x1);
        case AL:
            return 1;
        default:
            return 0;
    }
}

void shift_with_carry(byte const *shift_by, byte const *shift_type,
                      unsigned int *shifted_value, byte *shift_carry) {
    switch (*shift_type) {
        case LSL:
            *shift_carry = (*shifted_value >> (32 - *shift_by)) & 0x1;
            *shifted_value = *shifted_value << *shift_by;
            break;
        case LSR:
            *shift_carry = (*shifted_value >> (*shift_by - 1)) & 0x1;
            *shifted_value = *shifted_value >> *shift_by;
            break;
        case ASR:
            *shift_carry = (*shifted_value >> (*shift_by - 1)) & 0x1;
            *shifted_value = (unsigned int) (((int) *shifted_value) >> *shift_by);
            break;
        case ROR:
            *shift_carry = (*shifted_value >> (*shift_by - 1)) & 0x1;
            *shifted_value = (*shifted_value >> *shift_by) | (*shifted_value << (32 - *shift_by));
            break;
        default:
            break;
    }
}

void shift(byte const *shift_by, byte const *shift_type, unsigned int *shifted_value) {
    switch (*shift_type) {
        case LSL:
            *shifted_value = *shifted_value << *shift_by;
            break;
        case LSR:
            *shifted_value = *shifted_value >> *shift_by;
            break;
        case ASR:
            *shifted_value = (unsigned int) (((int) *shifted_value) >> *shift_by);
            break;
        case ROR:
            *shifted_value = (*shifted_value >> *shift_by) | (*shifted_value << (32 - *shift_by));
            break;
        default:
            break;
    }
}

void processing_update_CPSR(unsigned int *cpsr, unsigned int const *result, byte const *carry) {
    if (*result) {
        if (*carry) {
            byte bit31 = (*result >> 31) & 0x1;
            if (bit31) {
                *cpsr = (*cpsr & 0xbfffffff) | 0xa0000000; // clears Z and sets N and C of CPSR
            } else {
                *cpsr = (*cpsr & 0x3fffffff) | 0x20000000; // clears N and Z and sets C of CPSR
            }
        } else {
            byte bit31 = (*result >> 31) & 0x1;
            if (bit31) {
                *cpsr = (*cpsr & 0x9fffffff) | 0x80000000; // clears Z and C and sets N of CPSR
            } else {
                *cpsr &= 0x9fffffff; // clears N, Z and C of CPSR
            }
        }
    } else {
        if (*carry) {
            *cpsr = (*cpsr & 0x7fffffff) | 0x60000000; // clears N and sets Z and C of CPSR
        } else {
            *cpsr = (*cpsr & 0x5fffffff) | 0x40000000; // clears N and C and sets Z of CPSR
        }
    }
}

void multiplying_update_CPSR(unsigned int *cpsr, unsigned int const *result) {
    if (result) {
        byte bit31 = (*result >> 31) & 0x1;
        if (bit31) {
            *cpsr = (*cpsr & 0x8fffffff) | 0x80000000; // clears Z and sets N of CPSR
        } else {
            *cpsr &= 0x3fffffff; // clears N and Z of CPSR
        }
    } else {
        *cpsr = (*cpsr & 0x7fffffff) | 0x40000000; // clears N and sets Z of CPSR
    }
}

void gpio_access_print(unsigned int *address) {
    if (*address <= GPIO_0_9_LAST_BYTE) {
        printf("One GPIO pin from 0 to 9 has been accessed\n");
    } else if (*address <= GPIO_10_19_LAST_BYTE) {
        printf("One GPIO pin from 10 to 19 has been accessed\n");
    } else {
        printf("One GPIO pin from 20 to 29 has been accessed\n");
    }
}

void load(unsigned int const *address, byte const *memory, unsigned int *Rd_register) {
    *Rd_register = (((((memory[*address + 3] << 8) |
                       memory[*address + 2]) << 8) |
                     memory[*address + 1]) << 8) |
                   memory[*address];
}

void store(unsigned int const *address, byte *memory, unsigned int const *Rd_register) {
    memory[*address] = *Rd_register & 0xff;
    memory[*address + 1] = (*Rd_register >> 8) & 0xff;
    memory[*address + 2] = (*Rd_register >> 16) & 0xff;
    memory[*address + 3] = (*Rd_register >> 24) & 0xff;
}

void output(unsigned int const *registers, byte const *memory) {
    // Print registers
    printf("Registers:\n");
    for (byte i = 0; i < 13; i++) {
        printf("$%-3d: %10d (0x%08x)\n", i, registers[i], registers[i]);
    }
    printf("PC  : %10d (0x%08x)\n", registers[PC_REF], registers[PC_REF]);
    printf("CPSR: %10d (0x%08x)\n", registers[CPSR_REF], registers[CPSR_REF]);

    // Print non-zero memory
    printf("Non-zero memory:\n");
    for (unsigned int i = 0; i < MEMORY_SIZE; i += 4) {
        int mem = (((((memory[i] << 8) |
                      memory[i + 1]) << 8) |
                    memory[i + 2]) << 8) |
                  memory[i + 3];
        if (mem != 0) {
            printf("0x%08x: 0x%08x\n", i, mem);
        }
    }
}
