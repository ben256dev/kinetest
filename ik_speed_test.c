#include <math.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct vec2 {
    float x;
    float y;
} vec2;

static void usage(const char *prog) {
    fprintf(stderr, "usage: %s iterations base.x base.y end_effector.x end_effector.y r R\n", prog);
}

typedef struct planar_two_link_limb {
    vec2  base;
    vec2  end_effector;
    vec2  midjoint;
    float r;
    float R;
} planar_two_link_limb;

/* RETURN VALUES:
 *     0 success
 *     1 non positive limb lengths
 *     2 zero distance
 *     3 circles do not intersect
 */
int planar_two_link_solve_circles(planar_two_link_limb *limb) {
    if (limb->r <= 0.0f || limb->R <= 0.0f)
        return 1;

    const vec2 u = { limb->end_effector.x - limb->base.x, limb->end_effector.y - limb->base.y };
    const float d = hypotf(u.x, u.y);

    if (d == 0.0f)
        return 2;

    if (d > limb->r + limb->R || d < fabsf(limb->r - limb->R))
        return 3;

    const float x = (d * d + limb->r - limb->R) / (2.0f * d);
    const float y = sqrtf(limb->r - x * x);

    const float u_inv_sqrt = 1.0f / d;
    const vec2 u_normalized = { u.x * u_inv_sqrt, u.y * u_inv_sqrt };
    const vec2 u_normalized_perp = { -u_normalized.y, u_normalized.x };

    limb->midjoint.x = limb->base.x + x * u_normalized.x + y * u_normalized_perp.x;
    limb->midjoint.y = limb->base.y + x * u_normalized.y + y * u_normalized_perp.y;

    return 0;
}

int main(int argc, char **argv) {
    if (argc != 8) {
        usage(argv[0]);
        return 1;
    }

    planar_two_link_limb limb = { 0 };
    limb.base.x         = strtof(argv[2], NULL);
    limb.base.y         = strtof(argv[3], NULL);
    limb.end_effector.x = strtof(argv[4], NULL);
    limb.end_effector.y = strtof(argv[5], NULL);
    limb.r              = strtof(argv[6], NULL);
    limb.R              = strtof(argv[7], NULL);

    size_t iterations = (size_t)strtoul(argv[1], NULL, 10);
    for (size_t i = 0; i < iterations; ++i)
        planar_two_link_solve_circles(&limb);

    printf("%f %f\n", limb.midjoint.x, limb.midjoint.y);

    return 0;
}
