#include "aloha.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

int aloha_s(int slotted, double g, char text[16], int *at_max) {
    double s;

    if (text == NULL || at_max == NULL || (slotted != 0 && slotted != 1)) {
        return 0;
    }
    if (!isfinite(g) || g < 0) {
        return 0;
    }
    s = slotted ? g * exp(-g) : g * exp(-2.0 * g);
    if (!isfinite(s)) {
        return 0;
    }
    snprintf(text, 16, "%.4f", s);
    *at_max = slotted ? fabs(g - 1.0) < 1e-9 : fabs(g - 0.5) < 1e-9;
    return 1;
}

void aloha_num(double x, char *buf, int cap) {
    char raw[32];
    int n;

    if (buf == NULL || cap < 2) {
        return;
    }
    if (!isfinite(x)) {
        snprintf(buf, (size_t)cap, "?");
        return;
    }
    if (fabs(x - round(x)) < 1e-9 && fabs(x) < 1000000.0) {
        snprintf(buf, (size_t)cap, "%.0f", x);
        return;
    }
    snprintf(raw, sizeof raw, "%.4f", x);
    n = (int)strlen(raw);
    while (n > 1 && raw[n - 1] == '0') {
        raw[--n] = '\0';
    }
    if (n > 1 && raw[n - 1] == '.') {
        raw[--n] = '\0';
    }
    snprintf(buf, (size_t)cap, "%s", raw);
}

void aloha_peak(char pure_pct[8], char slot_pct[8], char pure_s[16], char slot_s[16]) {
    int at_max = 0;

    aloha_s(0, 0.5, pure_s, &at_max);
    aloha_s(1, 1.0, slot_s, &at_max);
    snprintf(pure_pct, 8, "%.1f%%", 100.0 / (2.0 * exp(1.0)));
    snprintf(slot_pct, 8, "%.1f%%", 100.0 / exp(1.0));
}
