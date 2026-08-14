/*
 * bint.c
 * Copyright (c) 2023-2026 Ray Den
 * SPDX-License-Identifier: MIT
 *
 * Big integer native module for Raden.
 *
 * Big integers are stored as arrays of uint64_t digits (limbs) in
 * little-endian order using base 10^9 (BIGINT_BASE).  This base is chosen
 * so that any two limbs can be multiplied without overflowing uint64_t,
 * making the implementation fully portable (no __uint128_t needed).
 *
 * Each limb holds a value in [0, BIGINT_BASE-1].
 * The representation is sign-magnitude (neg flag).
 * Zero always has neg == false and count == 0.
 *
 * Raden-side values are decimal strings on the stack.
 * Native functions parse the string, perform the operation, and push the
 * result string back onto the stack.
 *
 * Exported Raden functions:
 *   bint_add, bint_sub, bint_mul, bint_div, bint_mod, bint_pow,
 *   bint_neg, bint_abs,
 *   bint_cmp, bint_eq, bint_lt, bint_gt, bint_le, bint_ge,
 *   bint_and, bint_or, bint_xor, bint_not,
 *   bint_shl, bint_shr
 *
 * Example Raden usage:
 *   "./libs/bint.rdn" load
 *   "12345678901234567890" "9876543210" bint_add call print
 */

/*
 *  AI documentation generated
 * */

#include "../include/rdn_native.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/*  Internal BigInt representation                                     */
/* ------------------------------------------------------------------ */

/*
 * Each limb holds a value in [0, BIGINT_BASE-1].
 * 10^9 fits comfortably in uint64_t and guarantees that
 * (BIGINT_BASE-1) * (BIGINT_BASE-1) < UINT64_MAX.
 */
#define BIGINT_BASE 1000000000ULL

/* Maximum decimal digits per limb (used for string conversion sizing) */
#define DIGITS_PER_LIMB 9

typedef struct {
    uint64_t *limbs;
    size_t    count;
    size_t    capacity;
    bool      neg;
} BigInt;

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                   */
/* ------------------------------------------------------------------ */

static void bigint_grow(BigInt *a, size_t needed) {
    if (needed <= a->capacity) return;
    if (a->capacity == 0) a->capacity = 64;
    while (needed > a->capacity) a->capacity *= 2;
    uint64_t* save = a->limbs;
    a->limbs = (uint64_t *)realloc(a->limbs, a->capacity * sizeof(uint64_t));
    if (a->limbs == NULL) {
        if (save) {
            free(save);
        }
    }
}

static void bigint_free(BigInt *a) {
    free(a->limbs);
    a->limbs    = NULL;
    a->count    = 0;
    a->capacity = 0;
    a->neg      = false;
}

static void bigint_zero(BigInt *a) {
    a->count = 0;
    a->neg   = false;
}

static void bigint_trim(BigInt *a) {
    while (a->count > 0 && a->limbs[a->count - 1] == 0) {
        a->count--;
    }
    if (a->count == 0) {
        a->neg = false;
    }
}

static void bigint_append(BigInt *a, uint64_t v) {
    bigint_grow(a, a->count + 1);
    a->limbs[a->count++] = v;
}

static BigInt bigint_clone(const BigInt *src) {
    BigInt r = {0};
    bigint_grow(&r, src->count);
    for (size_t i = 0; i < src->count; i++) {
        bigint_append(&r, src->limbs[i]);
    }
    r.neg = src->neg;
    return r;
}

/*
 * Normalise a BigInt after bitwise or shift operations.
 * Some limbs may exceed BIGINT_BASE; carry the excess forward.
 */
static void bigint_normalise(BigInt *a) {
    uint64_t carry = 0;
    for (size_t i = 0; i < a->count; i++) {
        uint64_t v = a->limbs[i] + carry;
        a->limbs[i] = v % BIGINT_BASE;
        carry       = v / BIGINT_BASE;
    }
    while (carry > 0) {
        bigint_append(a, carry % BIGINT_BASE);
        carry /= BIGINT_BASE;
    }
    bigint_trim(a);
}

/* ------------------------------------------------------------------ */
/*  Comparison (all return -1 / 0 / 1)                                 */
/* ------------------------------------------------------------------ */

/* Compare absolute values only (ignores sign). */
static int bigint_cmp_mag(const BigInt *a, const BigInt *b) {
    if (a->count != b->count) {
        return a->count < b->count ? -1 : 1;
    }
    for (size_t i = a->count; i > 0; i--) {
        size_t idx = i - 1;
        if (a->limbs[idx] != b->limbs[idx]) {
            return a->limbs[idx] < b->limbs[idx] ? -1 : 1;
        }
    }
    return 0;
}

/* Full signed comparison. */
static int bigint_cmp(const BigInt *a, const BigInt *b) {
    if (a->neg != b->neg) {
        return a->neg ? -1 : 1;
    }
    int mag = bigint_cmp_mag(a, b);
    return a->neg ? -mag : mag;
}

/* ------------------------------------------------------------------ */
/*  Magnitude-only arithmetic (|result| = |a| OP |b|)                  */
/* ------------------------------------------------------------------ */

/* result = |a| + |b| */
static void bigint_add_mag(BigInt *result, const BigInt *a, const BigInt *b) {
    bigint_zero(result);
    size_t max = a->count > b->count ? a->count : b->count;
    bigint_grow(result, max + 1);

    uint64_t carry = 0;
    for (size_t i = 0; i < max; i++) {
        uint64_t av = i < a->count ? a->limbs[i] : 0;
        uint64_t bv = i < b->count ? b->limbs[i] : 0;
        uint64_t s  = av + bv + carry;
        bigint_append(result, s % BIGINT_BASE);
        carry = s / BIGINT_BASE;
    }
    if (carry > 0) {
        bigint_append(result, carry);
    }
}

/*
 * result = |a| - |b|
 * Caller MUST ensure |a| >= |b|.
 */
static void bigint_sub_mag(BigInt *result, const BigInt *a, const BigInt *b) {
    bigint_zero(result);
    bigint_grow(result, a->count);

    uint64_t borrow = 0;
    for (size_t i = 0; i < a->count; i++) {
        uint64_t av = a->limbs[i];
        uint64_t bv = i < b->count ? b->limbs[i] : 0;
        uint64_t diff;
        if (av < bv + borrow) {
            diff   = av + BIGINT_BASE - bv - borrow;
            borrow = 1;
        } else {
            diff   = av - bv - borrow;
            borrow = 0;
        }
        bigint_append(result, diff);
    }
    bigint_trim(result);
}

/* ------------------------------------------------------------------ */
/*  Full signed arithmetic                                              */
/* ------------------------------------------------------------------ */

/* result = a + b */
static void bigint_add(BigInt *result, const BigInt *a, const BigInt *b) {
    if (a->neg == b->neg) {
        bigint_add_mag(result, a, b);
        result->neg = a->neg;
    } else {
        int cmp = bigint_cmp_mag(a, b);
        if (cmp >= 0) {
            bigint_sub_mag(result, a, b);
            result->neg = a->neg;   /* |a| >= |b| so sign follows a */
        } else {
            bigint_sub_mag(result, b, a);
            result->neg = b->neg;
        }
    }
}

/* result = a - b */
static void bigint_sub(BigInt *result, const BigInt *a, const BigInt *b) {
    BigInt neg_b = bigint_clone(b);
    neg_b.neg = !neg_b.neg;
    bigint_add(result, a, &neg_b);
    bigint_free(&neg_b);
}

/* result = a * b  (schoolbook O(n*m)) */
static void bigint_mul(BigInt *result, const BigInt *a, const BigInt *b) {
    bigint_zero(result);
    bigint_grow(result, a->count + b->count + 1);
    /* initialise limbs to zero */
    for (size_t i = 0; i < a->count + b->count + 1; i++) {
        bigint_append(result, 0);
    }

    for (size_t i = 0; i < a->count; i++) {
        uint64_t carry = 0;
        size_t base = i;  /* result index offset */

        for (size_t j = 0; j < b->count; j++) {
            uint64_t prod   = a->limbs[i] * b->limbs[j];
            uint64_t sum    = result->limbs[base + j] + (prod % BIGINT_BASE) + carry;
            result->limbs[base + j] = sum % BIGINT_BASE;
            carry = (prod / BIGINT_BASE) + (sum / BIGINT_BASE);
        }

        /* propagate final carry */
        size_t k = base + b->count;
        while (carry > 0) {
            uint64_t sum = result->limbs[k] + carry;
            result->limbs[k] = sum % BIGINT_BASE;
            carry = sum / BIGINT_BASE;
            k++;
        }
    }

    result->neg = a->neg != b->neg;
    bigint_trim(result);
}

/*
 * Compute quotient and remainder:
 *   q = |a| / |b|   r = |a| % |b|
 * Returns false on division by zero.
 */
static bool bigint_div_mod(BigInt *q, BigInt *r,
                           const BigInt *a, const BigInt *b) {
    bigint_zero(q);
    bigint_zero(r);

    if (b->count == 0) return false;            /* division by zero */
    if (bigint_cmp_mag(a, b) < 0) {
        /* |a| < |b| : q=0, r=|a| */
        BigInt ca = bigint_clone(a);
        *r = ca;
        return true;
    }

    if (b->count == 1) {
        /* Single-limb divisor: fast path */
        uint64_t divisor = b->limbs[0];
        uint64_t rem     = 0;
        bigint_grow(q, a->count);
        for (size_t i = a->count; i > 0; i--) {
            uint64_t v = rem * BIGINT_BASE + a->limbs[i - 1];
            uint64_t d = v / divisor;
            rem        = v % divisor;
            /* Insert at front (most-significant side) */
            bigint_append(q, d);
        }
        /* The limbs are in MSB-first order; reverse them */
        size_t lo = 0, hi = q->count - 1;
        while (lo < hi) {
            uint64_t t = q->limbs[lo];
            q->limbs[lo] = q->limbs[hi];
            q->limbs[hi] = t;
            lo++; hi--;
        }
        bigint_trim(q);

        if (rem > 0) {
            bigint_append(r, rem);
        }
        return true;
    }

    /* Multi-limb divisor: long division (Knuth-style) */
    /*
     * We use a simpler approach: trial subtraction.
     * Algorithm:
     *   1. Normalise: multiply both a and b by d so that b's top limb >= BASE/2.
     *   2. For each quotient digit q_hat:
     *        q_hat = (u[j+n]*BASE + u[j+n-1]) / v[n-1]
     *        If q_hat >= BASE, q_hat = BASE-1
     *        Reduce q_hat until q_hat * v <= appropriate slice of u
     *        Subtract q_hat * v (shifted) from u
     *        Store q_hat as quotient digit
     *   3. Un-normalise remainder
     */

    /* Normalisation factor: shift left so v's top limb >= BASE/2 */
    uint64_t d = BIGINT_BASE / (b->limbs[b->count - 1] + 1);

    /* u = a * d, v = b * d */
    BigInt u = {0}, v = {0};
    {
        BigInt d_big = {0};
        bigint_append(&d_big, d);
        bigint_mul(&u, a, &d_big);
        bigint_mul(&v, b, &d_big);
        bigint_free(&d_big);
    }

    /* Ensure u has at least v.count + 1 limbs (pad with zero) */
    while (u.count < v.count + 1) {
        bigint_append(&u, 0);
    }

    size_t n = v.count;
    size_t m = u.count - n;

    /* quotient will have m limbs */
    BigInt quot = {0};
    bigint_grow(&quot, m);
    for (size_t i = 0; i < m; i++) bigint_append(&quot, 0);

    BigInt base_big = {0};
    bigint_append(&base_big, BIGINT_BASE);

    for (size_t j = m; j > 0; j--) {
        size_t idx = j - 1;  /* 0-based */

        /* Estimate q_hat */
        uint64_t u_top   = u.limbs[idx + n];
        uint64_t u_next  = u.limbs[idx + n - 1];
        uint64_t v_top   = v.limbs[n - 1];

        uint64_t q_hat = (u_top * BIGINT_BASE + u_next) / v_top;
        uint64_t r_hat = (u_top * BIGINT_BASE + u_next) % v_top;

        /* Adjust if q_hat >= BASE or q_hat * v_next > r_hat*BASE + u_next-1 */
        while (q_hat >= BIGINT_BASE ||
               (n > 1 && q_hat * v.limbs[n - 2] > r_hat * BIGINT_BASE + u.limbs[idx + n - 2])) {
            q_hat--;
            r_hat += v_top;
            if (r_hat >= BIGINT_BASE) break;
        }

        /* Subtract q_hat * (v shifted by idx limbs) from u */
        uint64_t sub_carry = 0;
        bool    negative   = false;
        for (size_t i = 0; i < n; i++) {
            uint64_t prod = q_hat * v.limbs[i];
            uint64_t sub  = (prod % BIGINT_BASE) + sub_carry;
            if (u.limbs[idx + i] < sub) {
                u.limbs[idx + i] += BIGINT_BASE - sub;
                sub_carry = (prod / BIGINT_BASE) + 1;
            } else {
                u.limbs[idx + i] -= sub;
                sub_carry = prod / BIGINT_BASE;
            }
        }
        /* Handle the extra limb */
        if (u.limbs[idx + n] < sub_carry) {
            u.limbs[idx + n] += BIGINT_BASE - sub_carry;
            negative = true;
        } else {
            u.limbs[idx + n] -= sub_carry;
        }

        /* If the subtraction went negative, add back and decrement q_hat */
        if (negative) {
            q_hat--;
            uint64_t add_carry = 0;
            for (size_t i = 0; i < n; i++) {
                uint64_t sum = u.limbs[idx + i] + v.limbs[i] + add_carry;
                u.limbs[idx + i] = sum % BIGINT_BASE;
                add_carry = sum / BIGINT_BASE;
            }
            u.limbs[idx + n] += add_carry;
        }

        quot.limbs[idx] = q_hat;
    }

    bigint_trim(&quot);
    bigint_trim(&u);

    /* Un-normalise remainder */
    if (d > 1) {
        BigInt d_big = {0};
        bigint_append(&d_big, d);
        /* r = u / d */
        uint64_t rem = 0;
        bigint_grow(r, u.count);
        for (size_t i = u.count; i > 0; i--) {
            uint64_t vv = rem * BIGINT_BASE + u.limbs[i - 1];
            /* We only need the remainder, not the quotient */
            rem = vv % d;
        }
        if (rem > 0) {
            bigint_append(r, rem);
        }
    } else {
        bigint_free(r);
        *r = u;
        u.limbs = NULL; /* ownership transferred */
    }

    *q = quot;
    bigint_free(&v);

    /* q and r are magnitudes; sign handled by caller */
    return true;
}

/* ------------------------------------------------------------------ */
/*  Power                                                              */
/* ------------------------------------------------------------------ */

/*
 * result = base ^ exp
 * Exponent is a uint64_t (not a BigInt) to keep results tractable.
 * Uses exponentiation by squaring.
 */
static void bigint_pow(BigInt *result, const BigInt *base, uint64_t exp) {
    if (exp == 0) {
        bigint_zero(result);
        bigint_append(result, 1);
        return;
    }

    bool odd = (exp & 1);

    BigInt acc = {0};
    bigint_append(&acc, 1);

    BigInt cur = bigint_clone(base);

    while (exp > 0) {
        if (exp & 1) {
            BigInt tmp = {0};
            bigint_mul(&tmp, &acc, &cur);
            bigint_free(&acc);
            acc = tmp;
        }
        exp >>= 1;
        if (exp > 0) {
            BigInt tmp = {0};
            bigint_mul(&tmp, &cur, &cur);
            bigint_free(&cur);
            cur = tmp;
        }
    }

    bigint_free(&cur);
    *result = acc;
    result->neg = base->neg && odd;
}

/* ------------------------------------------------------------------ */
/*  Bitwise operations (non-negative only)                             */
/* ------------------------------------------------------------------ */

static void bigint_and(BigInt *result, const BigInt *a, const BigInt *b) {
    bigint_zero(result);
    size_t n = a->count < b->count ? a->count : b->count;
    bigint_grow(result, n);
    for (size_t i = 0; i < n; i++) {
        bigint_append(result, a->limbs[i] & b->limbs[i]);
    }
    bigint_normalise(result);
    result->neg = false;
}

static void bigint_or(BigInt *result, const BigInt *a, const BigInt *b) {
    bigint_zero(result);
    size_t n = a->count > b->count ? a->count : b->count;
    bigint_grow(result, n);
    for (size_t i = 0; i < n; i++) {
        uint64_t av = i < a->count ? a->limbs[i] : 0;
        uint64_t bv = i < b->count ? b->limbs[i] : 0;
        bigint_append(result, av | bv);
    }
    bigint_normalise(result);
    result->neg = false;
}

static void bigint_xor(BigInt *result, const BigInt *a, const BigInt *b) {
    bigint_zero(result);
    size_t n = a->count > b->count ? a->count : b->count;
    bigint_grow(result, n);
    for (size_t i = 0; i < n; i++) {
        uint64_t av = i < a->count ? a->limbs[i] : 0;
        uint64_t bv = i < b->count ? b->limbs[i] : 0;
        bigint_append(result, av ^ bv);
    }
    bigint_normalise(result);
    result->neg = false;
}

/* bigint_not(a) = -(a+1)  (infinite two's complement semantics) */
static void bigint_not(BigInt *result, const BigInt *a) {
    BigInt one = {0};
    bigint_append(&one, 1);
    bigint_add(result, a, &one);
    result->neg = !result->neg;
    bigint_free(&one);
}

static void bigint_shl(BigInt *result, const BigInt *a, uint64_t n) {
    if (n == 0 || a->count == 0) {
        BigInt c = bigint_clone(a);
        *result = c;
        return;
    }

    size_t limb_shift  = n / 64;       /* full limb shifts */
    unsigned bit_shift = (unsigned)(n % 64);  /* remaining bits */

    bigint_zero(result);
    bigint_grow(result, a->count + limb_shift + 2);

    /* offset for full-limb shift */
    for (size_t i = 0; i < limb_shift; i++) {
        bigint_append(result, 0);
    }

    uint64_t carry = 0;
    for (size_t i = 0; i < a->count; i++) {
        uint64_t v = (a->limbs[i] << bit_shift) | carry;
        bigint_append(result, v);
        carry = (bit_shift == 0) ? 0 : (a->limbs[i] >> (64 - bit_shift));
    }
    if (carry > 0) {
        bigint_append(result, carry);
    }

    /* Reconstitute from raw binary back to base-BIGINT_BASE */
    bigint_normalise(result);
    result->neg = a->neg;
}

static void bigint_shr(BigInt *result, const BigInt *a, uint64_t n) {
    if (n == 0 || a->count == 0) {
        BigInt c = bigint_clone(a);
        bigint_free(result);
        *result = c;
        return;
    }

    /* divisor = 2^n  (computed via exponentiation by squaring) */
    BigInt two = {0};
    bigint_append(&two, 2);
    BigInt divisor = {0};
    bigint_pow(&divisor, &two, n);
    bigint_free(&two);

    /* quotient = |a| / 2^n  (correctly handles base-10^9 limbs) */
    BigInt q = {0}, r = {0};
    bigint_div_mod(&q, &r, a, &divisor);
    q.neg = a->neg;

    bigint_free(result);
    *result = q;
    bigint_free(&r);
    bigint_free(&divisor);
}

/* ------------------------------------------------------------------ */
/*  String conversion                                                  */
/* ------------------------------------------------------------------ */

/*
 * Parse a decimal string into a BigInt.
 * Accepts optional leading '+' or '-'.
 * Returns zero BigInt on parse error (empty count, neg=false).
 */
static BigInt bigint_from_string(const char *str) {
    BigInt result = {0};

    if (str == NULL || *str == '\0') return result;

    const char *p = str;
    bool neg = false;
    if (*p == '-') { neg = true; p++; }
    else if (*p == '+') { p++; }

    while (*p == '0') p++;   /* skip leading zeros */

    if (*p == '\0') return result;  /* "0", "-0", "+0" */

    bigint_append(&result, 0);  /* start with zero */

    while (*p) {
        char c = *p;
        if (c < '0' || c > '9') {
            /* Invalid character: return zero */
            bigint_free(&result);
            BigInt zero = {0};
            return zero;
        }
        unsigned digit = (unsigned)(c - '0');

        /* result = result * 10 + digit */
        uint64_t carry = digit;
        for (size_t i = 0; i < result.count; i++) {
            uint64_t prod = result.limbs[i] * 10 + carry;
            result.limbs[i] = prod % BIGINT_BASE;
            carry = prod / BIGINT_BASE;
        }
        if (carry > 0) {
            bigint_append(&result, carry);
        }

        p++;
    }

    result.neg = neg;
    bigint_trim(&result);
    return result;
}

/*
 * Convert a BigInt to its decimal string representation.
 * Caller owns the returned string and must free() it.
 */
static char *bigint_to_string(const BigInt *a) {
    if (a->count == 0) {
        char *z = (char *)malloc(2);
        if (z) z[0] = '0', z[1] = '\0';
        return z;
    }

    /* Worst-case: count * DIGITS_PER_LIMIT + 1 (sign) */
    size_t max_digits = a->count * DIGITS_PER_LIMB + 1 + (a->neg ? 1 : 0);
    char *buf = (char *)malloc(max_digits + 1);
    if (buf == NULL) return NULL;

    buf[max_digits] = '\0';
    size_t pos = max_digits;

    /* Work on a clone of the magnitude */
    BigInt tmp = bigint_clone(a);
    tmp.neg = false;

    while (tmp.count > 0) {
        uint64_t remainder = 0;
        for (size_t i = tmp.count; i > 0; i--) {
            uint64_t v = tmp.limbs[i - 1] + remainder * BIGINT_BASE;
            tmp.limbs[i - 1] = v / 10;
            remainder = v % 10;
        }
        bigint_trim(&tmp);

        pos--;
        buf[pos] = (char)('0' + remainder);
    }

    if (a->neg) {
        pos--;
        buf[pos] = '-';
    }

    if (pos > 0) {
        memmove(buf, buf + pos, max_digits - pos + 1);
    }

    bigint_free(&tmp);
    return buf;
}

/* ------------------------------------------------------------------ */
/*  Helpers for Raden native API wrappers                              */
/* ------------------------------------------------------------------ */

/*
 * Read a string at stack index, parse to BigInt, and pop the value.
 * Returns false on error (error message already set on api).
 *
 * NOTE: to_integer reads a long, and long is 32-bit on some platforms.
 * We use the string-based interface for all bigint operands.
 */
static bool pop_bigint(RDNApi *api, long index, BigInt *out) {
    if (api->type(api, index) != RDN_VALUE_STRING) {
        return api->raise_error(api, "bint: expected string operand");
    }
    const char *str = api->to_string(api, index);
    if (str == NULL) {
        return api->raise_error(api, "bint: failed to read string");
    }
    *out = bigint_from_string(str);
    /* If the result is zero but the string is non-numeric, it's a parse error */
    if (out->count == 0 && *str != '\0') {
        const char *p = str;
        if (*p == '-' || *p == '+') p++;
        while (*p == '0') p++;
        if (*p != '\0') {
            bigint_free(out);
            return api->raise_error(api, "bint: invalid digit in string");
        }
    }
    return api->pop(api, 1);
}

/* Pop a uint64_t from the stack (for shifts and pow exponent). */
static bool pop_uint64(RDNApi *api, long index, uint64_t *out) {
    long v = 0;
    if (!api->to_integer(api, index, &v)) {
        return api->raise_error(api, "bint: expected integer operand");
    }
    if (v < 0) {
        return api->raise_error(api, "bint: expected non-negative integer");
    }
    *out = (uint64_t)v;
    return api->pop(api, 1);
}

/* Pop two string BigInts from the stack (top is b, next is a). */
static bool pop_two_bigints(RDNApi *api, BigInt *a, BigInt *b) {
    if (api->stack_size(api) < 2) {
        return api->raise_error(api, "bint: requires 2 operands");
    }
    if (!pop_bigint(api, -1, b)) return false;
    if (!pop_bigint(api, -1, a)) {
        bigint_free(b);
        return false;
    }
    return true;
}

/*
 * Push a BigInt as a Raden string on the stack.
 * Returns false on memory failure (error set).
 */
static bool push_bigint(RDNApi *api, const BigInt *n) {
    char *str = bigint_to_string(n);
    if (str == NULL) {
        return api->raise_error(api, "bint: out of memory");
    }
    bool ok = api->push_string(api, str);
    free(str);
    return ok;
}

/* ------------------------------------------------------------------ */
/*  Raden native API wrappers                                          */
/* ------------------------------------------------------------------ */

static bool native_bint_add(RDNApi *api) {
    BigInt a, b;
    if (!pop_two_bigints(api, &a, &b)) return false;

    BigInt result = {0};
    bigint_add(&result, &a, &b);

    bool ok = push_bigint(api, &result);
    bigint_free(&a); bigint_free(&b); bigint_free(&result);
    return ok;
}

static bool native_bint_sub(RDNApi *api) {
    BigInt a, b;
    if (!pop_two_bigints(api, &a, &b)) return false;

    BigInt result = {0};
    bigint_sub(&result, &a, &b);

    bool ok = push_bigint(api, &result);
    bigint_free(&a); bigint_free(&b); bigint_free(&result);
    return ok;
}

static bool native_bint_mul(RDNApi *api) {
    BigInt a, b;
    if (!pop_two_bigints(api, &a, &b)) return false;

    BigInt result = {0};
    bigint_mul(&result, &a, &b);

    bool ok = push_bigint(api, &result);
    bigint_free(&a); bigint_free(&b); bigint_free(&result);
    return ok;
}

static bool native_bint_div(RDNApi *api) {
    BigInt a, b;
    if (!pop_two_bigints(api, &a, &b)) return false;

    if (b.count == 0) {
        bigint_free(&a); bigint_free(&b);
        return api->raise_error(api, "bint_div: division by zero");
    }

    BigInt q = {0}, r = {0};
    if (!bigint_div_mod(&q, &r, &a, &b)) {
        bigint_free(&a); bigint_free(&b);
        return api->raise_error(api, "bint_div: division failed");
    }
    q.neg = a.neg != b.neg;
    bigint_trim(&q);
    bigint_free(&a); bigint_free(&b); bigint_free(&r);

    return push_bigint(api, &q);
}

static bool native_bint_mod(RDNApi *api) {
    BigInt a, b;
    if (!pop_two_bigints(api, &a, &b)) return false;

    if (b.count == 0) {
        bigint_free(&a); bigint_free(&b);
        return api->raise_error(api, "bint_mod: division by zero");
    }

    BigInt q = {0}, r = {0};
    if (!bigint_div_mod(&q, &r, &a, &b)) {
        bigint_free(&a); bigint_free(&b);
        return api->raise_error(api, "bint_mod: division failed");
    }
    r.neg = a.neg;
    bigint_trim(&r);
    bigint_free(&a); bigint_free(&b); bigint_free(&q);

    return push_bigint(api, &r);
}

static bool native_bint_pow(RDNApi *api) {
    if (api->stack_size(api) < 2) {
        return api->raise_error(api, "bint_pow requires 2 operands");
    }

    uint64_t exp = 0;
    if (!pop_uint64(api, -1, &exp)) return false;

    BigInt base;
    if (!pop_bigint(api, -1, &base)) return false;

    if (base.count == 0) {
        bigint_free(&base);
        /* 0^0 = 1, 0^positive = 0 */
        if (exp == 0) return api->push_string(api, "1");
        else          return api->push_string(api, "0");
    }

    BigInt result = {0};
    bigint_pow(&result, &base, exp);

    bool ok = push_bigint(api, &result);
    bigint_free(&base); bigint_free(&result);
    return ok;
}

static bool native_bint_neg(RDNApi *api) {
    if (api->stack_size(api) < 1) {
        return api->raise_error(api, "bint_neg requires 1 operand");
    }
    BigInt a;
    if (!pop_bigint(api, -1, &a)) return false;

    a.neg = !a.neg;
    bool ok = push_bigint(api, &a);
    bigint_free(&a);
    return ok;
}

static bool native_bint_abs(RDNApi *api) {
    if (api->stack_size(api) < 1) {
        return api->raise_error(api, "bint_abs requires 1 operand");
    }
    BigInt a;
    if (!pop_bigint(api, -1, &a)) return false;

    a.neg = false;
    bool ok = push_bigint(api, &a);
    bigint_free(&a);
    return ok;
}

/* Comparison: returns -1, 0, or 1 as a Raden integer */
static bool native_bint_cmp(RDNApi *api) {
    BigInt a, b;
    if (!pop_two_bigints(api, &a, &b)) return false;

    int c = bigint_cmp(&a, &b);
    bigint_free(&a); bigint_free(&b);
    return api->push_integer(api, (long)c);
}

#define DEFINE_BINT_CMP(name, op)                                        \
    static bool native_bint_##name(RDNApi *api) {                        \
        BigInt a, b;                                                     \
        if (!pop_two_bigints(api, &a, &b)) return false;                 \
        bool result = (bigint_cmp(&a, &b) op 0);                         \
        bigint_free(&a); bigint_free(&b);                                \
        return api->push_boolean(api, result);                           \
    }

DEFINE_BINT_CMP(eq, ==)
DEFINE_BINT_CMP(lt, <)
DEFINE_BINT_CMP(gt, >)
DEFINE_BINT_CMP(le, <=)
DEFINE_BINT_CMP(ge, >=)

/* Bitwise ops */
static bool native_bint_and(RDNApi *api) {
    BigInt a, b;
    if (!pop_two_bigints(api, &a, &b)) return false;
    if (a.neg || b.neg) {
        bigint_free(&a); bigint_free(&b);
        return api->raise_error(api, "bint_and: negative operands not supported");
    }
    BigInt result = {0};
    bigint_and(&result, &a, &b);
    bool ok = push_bigint(api, &result);
    bigint_free(&a); bigint_free(&b); bigint_free(&result);
    return ok;
}

static bool native_bint_or(RDNApi *api) {
    BigInt a, b;
    if (!pop_two_bigints(api, &a, &b)) return false;
    if (a.neg || b.neg) {
        bigint_free(&a); bigint_free(&b);
        return api->raise_error(api, "bint_or: negative operands not supported");
    }
    BigInt result = {0};
    bigint_or(&result, &a, &b);
    bool ok = push_bigint(api, &result);
    bigint_free(&a); bigint_free(&b); bigint_free(&result);
    return ok;
}

static bool native_bint_xor(RDNApi *api) {
    BigInt a, b;
    if (!pop_two_bigints(api, &a, &b)) return false;
    if (a.neg || b.neg) {
        bigint_free(&a); bigint_free(&b);
        return api->raise_error(api, "bint_xor: negative operands not supported");
    }
    BigInt result = {0};
    bigint_xor(&result, &a, &b);
    bool ok = push_bigint(api, &result);
    bigint_free(&a); bigint_free(&b); bigint_free(&result);
    return ok;
}

static bool native_bint_not(RDNApi *api) {
    if (api->stack_size(api) < 1) {
        return api->raise_error(api, "bint_not requires 1 operand");
    }
    BigInt a;
    if (!pop_bigint(api, -1, &a)) return false;
    if (a.neg) {
        bigint_free(&a);
        return api->raise_error(api, "bint_not: negative operand not supported");
    }
    BigInt result = {0};
    bigint_not(&result, &a);
    bool ok = push_bigint(api, &result);
    bigint_free(&a); bigint_free(&result);
    return ok;
}

static bool native_bint_shl(RDNApi *api) {
    if (api->stack_size(api) < 2) {
        return api->raise_error(api, "bint_shl requires 2 operands");
    }
    uint64_t n = 0;
    if (!pop_uint64(api, -1, &n)) return false;
    BigInt a;
    if (!pop_bigint(api, -1, &a)) return false;
    if (a.neg) {
        bigint_free(&a);
        return api->raise_error(api, "bint_shl: negative operand not supported");
    }
    BigInt result = {0};
    bigint_shl(&result, &a, n);
    bool ok = push_bigint(api, &result);
    bigint_free(&a); bigint_free(&result);
    return ok;
}

static bool native_bint_shr(RDNApi *api) {
    if (api->stack_size(api) < 2) {
        return api->raise_error(api, "bint_shr requires 2 operands");
    }
    uint64_t n = 0;
    if (!pop_uint64(api, -1, &n)) return false;
    BigInt a;
    if (!pop_bigint(api, -1, &a)) return false;
    if (a.neg) {
        bigint_free(&a);
        return api->raise_error(api, "bint_shr: negative operand not supported");
    }
    BigInt result = {0};
    bigint_shr(&result, &a, n);
    bool ok = push_bigint(api, &result);
    bigint_free(&a); bigint_free(&result);
    return ok;
}

/* ------------------------------------------------------------------ */
/*  Module entry point                                                 */
/* ------------------------------------------------------------------ */

bool rdn_module_init(RDNModule *module) {
    struct { const char *name; RDNNativeFunction func; } entries[] = {
        { "_bint_add", native_bint_add },
        { "_bint_sub", native_bint_sub },
        { "_bint_mul", native_bint_mul },
        { "_bint_div", native_bint_div },
        { "_bint_mod", native_bint_mod },
        { "_bint_pow", native_bint_pow },
        { "_bint_neg", native_bint_neg },
        { "_bint_abs", native_bint_abs },
        { "_bint_cmp", native_bint_cmp },
        { "_bint_eq",  native_bint_eq  },
        { "_bint_lt",  native_bint_lt  },
        { "_bint_gt",  native_bint_gt  },
        { "_bint_le",  native_bint_le  },
        { "_bint_ge",  native_bint_ge  },
        { "_bint_and", native_bint_and },
        { "_bint_or",  native_bint_or  },
        { "_bint_xor", native_bint_xor },
        { "_bint_not", native_bint_not },
        { "_bint_shl", native_bint_shl },
        { "_bint_shr", native_bint_shr },
    };

    for (size_t i = 0; i < sizeof(entries) / sizeof(entries[0]); i++) {
        if (!module->register_function(module, entries[i].name, entries[i].func)) {
            return false;
        }
    }
    return true;
}
