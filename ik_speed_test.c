#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BUTIL_IMPLEMENTATION
#include "butil.h"

typedef struct vec2 {
    float x;
    float y;
} vec2;

static void usage(const char *prog) {
    fprintf(stderr, "usage: %s [--json] iterations base.x base.y end_effector.x end_effector.y r R\n", prog);
}

typedef struct planar_two_link_limb {
    vec2  base;
    vec2  end_effector;
    vec2  midjoint;
    float r;
    float R;
} planar_two_link_limb;

typedef struct reachability_values {
    vec2 u;
    float n;
    int reachability;
} reachability_values;

/* RETURN VALUES:
 *     0 success
 *     1 non positive limb lengths
 *     2 zero distance
 *     3 end effector too far to reach
 *     4 end effector too close to reach
 */
int check_reachability(const planar_two_link_limb *limb, reachability_values *reachability) {
    if (reachability == NULL)
        die("reachability == NULL");

    if (limb->r <= 0.0f || limb->R <= 0.0f) {
        reachability->reachability = 1;
        return 1;
    }

    const vec2 u = { limb->end_effector.x - limb->base.x, limb->end_effector.y - limb->base.y };
    const float n = u.x * u.x + u.y * u.y;

    if (n == 0.0f) {
        reachability->reachability = 2;
        return 2;
    }

    const float radii_sum = limb->r + limb->R;
    if (n > radii_sum * radii_sum) {
        reachability->reachability = 3;
        return 3;
    }

    const float radii_diff = fabsf(limb->r - limb->R);
    if (n < radii_diff * radii_diff) {
        reachability->reachability = 4;
        return 4;
    }

    reachability->u = u;
    reachability->n = n;
    reachability->reachability = 0;
    return 0;
}
int planar_two_link_solve_circles(planar_two_link_limb *limb) {
    reachability_values values;
    if (check_reachability(limb, &values))
        return values.reachability;

    const vec2 u = values.u;
    const float n = values.n;

    const float d = sqrtf(n);
    const float x = (n + limb->r - limb->R) / (2.0f * d);
    const float y = sqrtf(limb->r - x * x);

    const float u_inv_sqrt = 1.0f / d;
    const vec2 u_normalized = { u.x * u_inv_sqrt, u.y * u_inv_sqrt };
    const vec2 u_normalized_perp = { -u_normalized.y, u_normalized.x };

    limb->midjoint.x = limb->base.x + x * u_normalized.x + y * u_normalized_perp.x;
    limb->midjoint.y = limb->base.y + x * u_normalized.y + y * u_normalized_perp.y;

    return values.reachability;
}
static int planar_two_link_solve_law_of_cosines(planar_two_link_limb *limb) {
    reachability_values values;
    if (check_reachability(limb, &values))
        return values.reachability;

    const vec2 u = values.u;
    const float d = sqrtf(values.n);

    const float cos_theta = (d * d + limb->r - limb->R) / (2.0f * d * limb->r);
    const float clamped_cos_theta = fmaxf(-1.0f, fminf(1.0f, cos_theta));
    const float theta = acosf(clamped_cos_theta);
    const float u_angle = atan2f(u.y, u.x);
    const float midjoint_angle = u_angle + theta;

    limb->midjoint.x = limb->base.x + limb->r * cosf(midjoint_angle);
    limb->midjoint.y = limb->base.y + limb->r * sinf(midjoint_angle);

    return 0;
}

typedef int (*solver_fn)(planar_two_link_limb *limb);

typedef struct solver_result {
    int status;
    vec2 midjoint;
    size_t iterations;
    double elapsed_seconds;
} solver_result;

static double elapsed_seconds(clock_t start, clock_t end) {
    return (double)(end - start) / (double)CLOCKS_PER_SEC;
}

static double elapsed_milliseconds(solver_result result) {
    return result.elapsed_seconds * 1000.0;
}

static double average_milliseconds(solver_result result) {
    if (result.iterations == 0)
        return 0.0;

    return elapsed_milliseconds(result) / (double)result.iterations;
}

static double average_nanoseconds(solver_result result) {
    return average_milliseconds(result) * 1000000.0;
}

static const char *reachability_label(int status) {
    switch (status) {
    case 0:
        return "solution found";
    case 1:
        return "negative lengths";
    case 2:
        return "zero distance";
    case 3:
        return "too far";
    case 4:
        return "too close";
    default:
        return "unknown";
    }
}

static solver_result run_solver(size_t iterations, const planar_two_link_limb *input, solver_fn solver) {
    planar_two_link_limb limb = *input;
    clock_t start = 0;
    clock_t end = 0;
    int status = 0;

    start = clock();
    for (size_t i = 0; i < iterations; ++i)
        status = solver(&limb);
    end = clock();

    {
        solver_result result = { 0 };
        result.status = status;
        result.midjoint = limb.midjoint;
        result.iterations = iterations;
        result.elapsed_seconds = elapsed_seconds(start, end);
        return result;
    }
}

static void print_solver_result(const char *label, solver_result result) {
    printf("%s: midjoint=(%f, %f) avg_ns=%0.3f total_ms=%0.6f reachability=%s\n",
           label,
           result.midjoint.x,
           result.midjoint.y,
           average_nanoseconds(result),
           elapsed_milliseconds(result),
           reachability_label(result.status));
}

static void print_json_row_prefix(int *needs_comma) {
    if (*needs_comma != 0)
        printf(",\n");
    else
        *needs_comma = 1;
}

static void print_json_number_or_null(float value) {
    if (isfinite(value))
        printf("%.9g", value);
    else
        printf("null");
}

static void print_json_solver_row(const char *label, solver_result result, int *needs_comma) {
    print_json_row_prefix(needs_comma);
    printf("  {\"label\":\"%s\",\"midjoint_x\":", label);
    print_json_number_or_null(result.midjoint.x);
    printf(",\"midjoint_y\":");
    print_json_number_or_null(result.midjoint.y);
    printf(",\"avg_ns\":%.3f,\"total_ms\":%.6f,\"reachability\":\"%s\"}",
           average_nanoseconds(result),
           elapsed_milliseconds(result),
           reachability_label(result.status));
}

int main(int argc, char **argv) {
    solver_result cosines_result = { 0 };
    solver_result circles_result = { 0 };
    int json_output = 0;
    int argi = 1;

    if (argc > 1 && strcmp(argv[1], "--json") == 0) {
        json_output = 1;
        argi = 2;
    }

    if (argc - argi != 7) {
        usage(argv[0]);
        return 1;
    }

    planar_two_link_limb limb = { 0 };
    limb.base.x         = strtof(argv[argi + 1], NULL);
    limb.base.y         = strtof(argv[argi + 2], NULL);
    limb.end_effector.x = strtof(argv[argi + 3], NULL);
    limb.end_effector.y = strtof(argv[argi + 4], NULL);
    limb.r              = strtof(argv[argi + 5], NULL);
    limb.R              = strtof(argv[argi + 6], NULL);

    size_t iterations = (size_t)strtoul(argv[argi], NULL, 10);

    cosines_result = run_solver(iterations, &limb, planar_two_link_solve_law_of_cosines);
    circles_result = run_solver(iterations, &limb, planar_two_link_solve_circles);

    if (json_output != 0) {
        int needs_comma = 0;

        printf("[\n");
        print_json_solver_row("cosines", cosines_result, &needs_comma);
        print_json_solver_row("circles", circles_result, &needs_comma);

        printf("\n]\n");
        return 0;
    }

    print_solver_result("cosines", cosines_result);
    print_solver_result("circles", circles_result);

    return 0;
}
