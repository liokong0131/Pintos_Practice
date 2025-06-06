#include <stdint.h>

#define F (1 << 14)

int int_to_fp(int n) {
    return n * F;
}
int fp_to_int(int x) {
    return x / F;
}
int fp_to_int_round(int x) {
    if (x >= 0) return (x + F / 2) / F;
    else        return (x - F / 2) / F;
}

// add
int add_fp(int x, int y) {
    return x + y;
}
int add_fp_int(int x, int n) {
    return x + n * F;
}

// sub
int sub_fp(int x, int y) {
    return x - y;
}
int sub_fp_int(int x, int n) {
    return x - n * F;
}

//mul
int mul_fp(int x, int y) {
    return ((int64_t) x) * y / F;
}
int mul_fp_int(int x, int n) {
    return x * n;
}

//div
int div_fp(int x, int y) {
    return ((int64_t) x) * F / y;
}
int div_fp_int(int x, int n) {
    return x / n;
}