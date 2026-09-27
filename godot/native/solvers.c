#include "solvers.h"
#include <math.h>

static int reachability(const IKInput *in, float *n) {
    if (!isfinite(in->x) || !isfinite(in->y) || !isfinite(in->upper) ||
        !isfinite(in->lower) || in->upper <= 0 || in->lower < 0)
        return 1;
    *n = in->x * in->x + in->y * in->y;
    if (*n == 0) return 2;
    float sum = in->upper + in->lower;
    if (*n > sum * sum) return 3;
    float difference = in->upper - in->lower;
    if (*n < difference * difference) return 4;
    return 0;
}

IKResult ik_circles(const IKInput *in) {
    float n;
    IKResult out = {0};
    out.status = reachability(in, &n);
    if (out.status) return out;
    float d = sqrtf(n);
    float x = (n + in->upper * in->upper - in->lower * in->lower) / (2.0f * d);
    float y = sqrtf(fmaxf(0, in->upper * in->upper - x * x)) * in->orientation;
    float ux = in->x / d, uy = in->y / d;
    out.x = x * ux - y * uy;
    out.y = x * uy + y * ux;
    return out;
}

IKResult ik_cosines(const IKInput *in) {
    float n;
    IKResult out = {0};
    out.status = reachability(in, &n);
    if (out.status) return out;
    float d = sqrtf(n);
    float ratio = (in->upper * in->upper + n - in->lower * in->lower) / (2.0f * in->upper * d);
    float theta = acosf(fminf(1, fmaxf(-1, ratio)));
    float angle = atan2f(in->y, in->x) + theta * in->orientation;
    out.x = cosf(angle) * in->upper;
    out.y = sinf(angle) * in->upper;
    return out;
}
