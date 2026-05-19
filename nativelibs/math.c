/*
 * math.c
 * Copyright (c) 2023-2026 Ray Den
 * SPDX-License-Identifier: MIT
 *
 * Purpose:
 * Native math functions for Raden.
 *
 * Exports:
 * - powerOf(base, exponent) -> double
 * - sqrtOf(value) -> double
 *
 * Example Raden usage:
 * "./libs/math_native.rdn" load
 * 2 8 powf call print
 * "\n" print
 * 25 sqrt call print
 */

#include "../include/rdn_native.h"

#include <math.h>

static bool powerOf(RDNApi* api) {
    double x = 0;
    double y = 0;
    if (api->stack_size(api) < 2) {
        return api->raise_error(api, "powerOf requires 2 operands");
    }
    if (!api->is_number(api, -1) || !api->is_number(api, -2)) {
        return api->raise_error(api, "powerOf requires number operands");
    }
    if (!api->to_number(api, -1, &y) || !api->to_number(api, -2, &x)) {
        return api->raise_error(api, "powerOf requires number operands");
    }
    if (!api->pop(api, 2)) {
        return false;
    }
    return api->push_number(api, pow(x, y));
}

static bool sqrtOf(RDNApi* api) {

    double x = 0;

    if (api->stack_size(api) < 1) {
        return api->raise_error(api, "sqrtOf requires 1 operands");
    }

    if (!api->is_number(api, -1)) {
        return api->raise_error(api, "sqrtOf requires number operands");
    }
    if (!api->to_number(api , -1 , &x)) {
        return api->raise_error(api, "sqrtOf requires number operands");
    }
    if (!api->pop(api, 1)) {
        return false;
    }
    return api->push_number(api, sqrt(x));
}


bool rdn_module_init(RDNModule *module) {
    if (!module->register_function(module, "powerOf", powerOf)) {
        return false;
    }

    if (!module->register_function(module, "sqrtOf", sqrtOf)) {
        return false;
    }
    return true;
}
