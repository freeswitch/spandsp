/*
 * SpanDSP - a series of DSP components for telephony
 *
 * meteor-engine.h - The meteor FIR design algorithm
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2013 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#if !defined(_METEOR_ENGINE_H_)
#define _METEOR_ENGINE_H_

#define num_specs_MAX       20          /* Max. no. of specifications */
#define MAX_COEFFS          64          /* Max. no. of coefficients */
#define MAX_TAPS            129         /* Max. size of n, where there are n+1 grid-points */
#define NCOL_MAX            6000        /* Max. no. of columns allowed in tableau */

enum meteor_result_e
{
    badly_formed_requirements = -1,
    optimum_obtained = -2,
    too_many_columns = -3,
    too_many_pivots = -4,
    unbounded_dual = -5,
    infeasible_dual = -6,
    infeasible_primal = -7,
    no_feasible_solution_found = -8,
    no_feasible_band_edge_found = -9
};

enum symmetry_types_e
{
    symmetry_cosine,
    symmetry_sine
};

enum constraint_types_e
{
    constraint_type_convexity,
    constraint_type_limit
};

enum sense_e
{
    sense_lower,
    sense_upper,
    sense_envelope,
    sense_concave,
    sense_convex
};

enum interpolation_e
{
    interpolation_arithmetic,
    interpolation_geometric
};

#if defined(__cplusplus)
extern "C"
{
#endif

struct meteor_constraint_s
{
    const char *name;                       /* A name to use to refer to this definition */
    enum constraint_types_e type;           /* Type of band */
    double left_freq;                       /* Band edges as read in */
    double right_freq;
    double left_bound;
    double right_bound;
    enum sense_e sense;                     /* Sense of constraint. */
    enum interpolation_e interpolation;     /* Interpolation method */
    int first_col;                          /* Leftmost column of spec */
    int last_col;                           /* Rightmost column of spec */
    bool hug;                               /* Allow this constraint to be hugged? */
    int band_pushed;                        /* Band edges pushed */
};

struct meteor_spec_s
{
    const char *filter_name;
    double sample_rate;
    enum symmetry_types_e symmetry_type;    /* Cosine or sine symmetry */
    int grid_points;                        /* There are n+1 grid-points from 0 to pi */
    int shortest;                           /* Range of L = 2*m-1, 2*m, or 2*m+1 */
    int longest;                            /* Range of L = 2*m-1, 2*m, or 2*m+1 */
    int num_specs;                          /* No. of bands */
    struct meteor_constraint_s spec[num_specs_MAX];
};

struct meteor_working_data_s
{
    struct meteor_spec_s *spec;
    bool unbounded;
    bool optimal;                           /* Flags for simplex */
    int iteration;                          /* Iteration count, index */
    int num_pivots;                         /* Pivot count */
    int pivot_col;                          /* Pivot column */
    int pivot_row;                          /* Pivot row */
    double pivot_element;                   /* Pivot element */
    double cbar;                            /* Price when searching for entering column */
    enum meteor_result_e result;            /* Result of simplex */
    int m;                                  /* No. of coefficients, left and right half m */
    int length;                             /* Filter length = 2*m-1, 2*m, 2*m+1 */

    int phase;                              /* Phase */

    double coeff[MAX_COEFFS];               /* Coefficients */

    double price[MAX_COEFFS + 1];           /* Shadow prices = row -1 of carry = -dual variables = -coefficients */
    int basis[MAX_COEFFS + 1];              /* Basis columns, negative integers artificial */

    double carry[MAX_COEFFS + 2][MAX_COEFFS + 2];   /* Inverse-basis matrix of the revised simplex method */

    double tab[MAX_COEFFS + 1][NCOL_MAX];   /* Tableau */
    double cur_col[MAX_COEFFS + 2];         /* Current column */
    double cur_cost;                        /* Current cost */

    double freq[NCOL_MAX];                  /* Frequencies at grid points */
    double d[NCOL_MAX];                     /* Current cost vector */
    double c[NCOL_MAX];                     /* Cost in original problem */

    bool found_feasible_solution;           /* Found feasible solution */
    int smallest_m;                         /* Range of m */
    int largest_m;                          /* Range of m */
    int best_m;                             /* Best order */
    int num_cols;                           /* Number of columns */
    enum
    {
        find_len,
        max_dist,
        push_edge
    } what_to_do;                           /* Type of optimization */
    int num_pushed;                         /* Number of band edges pushed */
    enum
    {
        rr,
        ll
    } which_way;                            /* Push which way? */
    double low_limit;                       /* Lower limit for finding if primal is feasible */
    bool odd_length;                        /* Odd-length filters? */
    FILE *log_fd;
};

void output_filter_performance_as_csv_file(struct meteor_working_data_s *s, const char *file_name);

int meteor_design_filter(struct meteor_working_data_s *s, struct meteor_spec_s *t, double coeffs[]);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
