#ifndef KINETEST_SOLVERS_H
#define KINETEST_SOLVERS_H

typedef struct { float x, y, upper, lower, orientation; } IKInput;
typedef struct { float x, y; int status; } IKResult;
/* Status values match Limb.Reachability. Coordinates are relative to the base. */
IKResult ik_circles(const IKInput *input);
IKResult ik_cosines(const IKInput *input);
#endif
