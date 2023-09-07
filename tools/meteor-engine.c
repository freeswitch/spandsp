/*
 * SpanDSP - a series of DSP components for telephony
 *
 * meteor-engine.c - The meteor FIR design algorithm
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

/*! \file */

#include <inttypes.h>
#include <stdbool.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <setjmp.h>
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

#include "meteor-engine.h"
#include "ae.h"

/* version I: Wed Jun 27 13:36:06 EDT 1990 */

/* copyright Prof. K. Steiglitz
            Dept. of Computer Science
            Princeton University
            Princeton, NJ 08544 */

/* Constraint-based design of linear-phase fir filters with
   upper and lower bounds, and convexity constraints.
   Finds minimum length, or optimizes fixed length, or pushes band-edges.
   If L is the filter length, the models are

   odd-length
    cosine:   sum(i from 0 to (L-1)/2) coeff[i]*cos(i*omega)
    sine:     sum(i from 0 to (L-3)/2) coeff[i]*sin((i + 1)*omega)

   even-length
    cosine:   sum(i from 0 to L/2 - 1) coeff[i]*cos((i + 0.5)*omega)
    sine:     sum(i from 0 to L/2 - 1) coeff[i]*sin((i + 0.5)*omega)  */

#define MAX_PIVOTS      1000        /* Maximum no. of pivots */
#define SMALL           1.0e-8      /* Small number used in defining band-edges */
#define LARGE           1.0e+31     /* Large number used in search for minimum cost column */
#define EPS             1.0e-8      /* For testing for zero */

static void make_bands(struct meteor_working_data_s *s, int i)
{
    int j;
    int kmax;

    /* Fill in frequencies to make grid - frequencies are kept as reals
       in radians, and each band has equally spaced grid points */
    if (i == 0)
        s->spec->spec[i].first_col = 1;
    else
        s->spec->spec[i].first_col = s->spec->spec[i - 1].last_col + 1;
    /*endif*/
    kmax = (int) ((s->spec->spec[i].right_freq - s->spec->spec[i].left_freq)*s->spec->grid_points/0.5 + SMALL);
    /* kmax + 1 cols. in this band */
    if (kmax == 0)
    {
        s->freq[s->spec->spec[i].first_col - 1] = 2.0*M_PI*s->spec->spec[i].left_freq;
    }
    else
    {
        for (j = 0;  j <= kmax;  j++)
            s->freq[s->spec->spec[i].first_col + j - 1] = 2.0*M_PI*(s->spec->spec[i].left_freq + (s->spec->spec[i].right_freq - s->spec->spec[i].left_freq)*j/kmax);
        /*endfor*/
    }
    /*endif*/
    s->spec->spec[i].last_col = s->spec->spec[i].first_col + kmax;
}
/*- End of function --------------------------------------------------------*/

static double trig0(struct meteor_working_data_s *s, int i, double freq)
{
    double res;

    /* Trig function in filter transfer function */
    if (s->odd_length)
    {
        if (s->spec->symmetry_type == symmetry_cosine)
            res = cos(i*freq);
        else
            res = sin((i + 1.0)*freq);
        /*endif*/
    }
    else
    {
        if (s->spec->symmetry_type == symmetry_cosine)
            res = cos((i + 0.5)*freq);
        else
            res = sin((i + 0.5)*freq);
        /*endif*/
    }
    /*endif*/
    return res;
}
/*- End of function --------------------------------------------------------*/

static double trig2(struct meteor_working_data_s *s, int i, double freq)
{
    double res;

    /* Second derivative of trig function in filter transfer function */
    if (s->odd_length)
    {
        if (s->spec->symmetry_type == symmetry_cosine)
            res = -i*i*cos(i*freq);
        else
            res = -(i + 1.0)*(i + 1.0)*sin(i*freq);
        /*endif*/
    }
    else
    {
        if (s->spec->symmetry_type == symmetry_cosine)
            res = -(i + 0.5)*(i + 0.5)*cos((i + 0.5)*freq);
        else
            res = -(i + 0.5)*(i + 0.5)*sin((i + 0.5)*freq);
        /*endif*/
    }
    /*endif*/
    return res;
}
/*- End of function --------------------------------------------------------*/

static void convex(struct meteor_working_data_s *s, int i)
{
    int row;
    int col;

    /* Set up tableau columns for convexity constraints on magnitude */
    make_bands(s, i);
    for (col = s->spec->spec[i].first_col - 1;  col < s->spec->spec[i].last_col;  col++)
    {
        /* For all frequencies in band */
        s->c[col] = 0.0;
        for (row = 0;  row < s->m;  row++)
        {
            /* Normal constraint is <= */
            if (s->spec->spec[i].sense == sense_convex)
                s->tab[row][col] = -s->tab[row][col];
            else
                s->tab[row][col] = trig2(s, row, s->freq[col]);
            /*endif*/
        }
        /*endfor*/
        s->tab[s->m][col] = 0.0;
    }
    /*endfor*/
}
/*- End of function --------------------------------------------------------*/

static void limit(struct meteor_working_data_s *s, int i)
{
    int row;
    int col;

    /* Sets up tableau columns for upper or lower bounds on transfer
       function for specification i; the bound is linearly interpolated
       between the start and end of the band */
    make_bands(s, i);
    for (col = s->spec->spec[i].first_col - 1;  col < s->spec->spec[i].last_col;  col++)
    {
        /* For all frequencies in band */
        if (s->spec->spec[i].first_col == s->spec->spec[i].last_col)
        {
            s->c[col] = s->spec->spec[i].left_bound;
        }
        else
        {
            switch (s->spec->spec[i].interpolation)
            {
            case interpolation_geometric:
                s->c[col] = s->spec->spec[i].left_bound*exp((double) (col - s->spec->spec[i].first_col + 1) /
                           (s->spec->spec[i].last_col - s->spec->spec[i].first_col)*log(fabs(s->spec->spec[i].right_bound/s->spec->spec[i].left_bound)));
                break;
            case interpolation_arithmetic:
                s->c[col] = s->spec->spec[i].left_bound + (double) (col - s->spec->spec[i].first_col + 1) /
                           (s->spec->spec[i].last_col - s->spec->spec[i].first_col)*(s->spec->spec[i].right_bound - s->spec->spec[i].left_bound);
                break;
            }
            /*endswitch*/
        }
        /*endif*/
        if (s->spec->spec[i].sense == sense_lower)
            s->c[col] = -s->c[col];
        /*endif*/
        for (row = 0;  row < s->m;  row++)
        {
            s->tab[row][col] = (s->spec->spec[i].sense == sense_lower)  ?  -trig0(s, row, s->freq[col])  :  trig0(s, row, s->freq[col]);
        }
        /*endfor*/
        s->tab[s->m][col] = (s->spec->spec[i].hug)  ?  0.0  :  1.0;
    }
    /*endfor*/
}
/*- End of function --------------------------------------------------------*/

static void setup(struct meteor_working_data_s *s)
{
    int i;

    /* Initialize constraints */
    for (i = 0;  i < s->spec->num_specs;  i++)
    {
        switch (s->spec->spec[i].type)
        {
        case constraint_type_convexity:
            convex(s, i);
            break;
        case constraint_type_limit:
            limit(s, i);
            break;
        }
        /*endswitch*/
    }
    /*endfor*/
    s->num_cols = s->spec->spec[s->spec->num_specs - 1].last_col;
}
/*- End of function --------------------------------------------------------*/

static void column_search(struct meteor_working_data_s *s)
{
    int i;
    int col;
    double cost;

    /* Look for favorable column to enter basis.
       returns lowest cost and its column number, or turns on the flag optimal */
    /* Set up price vector */
    for (i = 0;  i <= s->m;  i++)
        s->price[i] = -s->carry[0][i + 1];
    /*endfor*/
    s->optimal = false;
    s->cbar = LARGE;
    s->pivot_col = 0;
    for (col = 0;  col < s->num_cols;  col++)
    {
        cost = s->d[col];
        for (i = 0;  i <= s->m;  i++)
            cost -= s->price[i]*s->tab[i][col];
        /*endfor*/
        if (s->cbar > cost)
        {
            s->cbar = cost;
            s->pivot_col = col + 1;
        }
        /*endif*/
    }
    /*endfor*/
    if (s->cbar > -EPS)
        s->optimal = true;
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

static void row_search(struct meteor_working_data_s *s)
{
    int i;
    int j;
    double ratio;
    double min_ratio;

    /* Look for pivot row. returns pivot row number, or turns on the flag unbounded */
    /* Generate column */
    for (i = 1;  i <= (s->m + 1);  i++)
    {
        /* Current column = B inverse * original col. */
        s->cur_col[i] = 0.0;
        for (j = 0;  j <= s->m;  j++)
            s->cur_col[i] += s->carry[i][j + 1]*s->tab[j][s->pivot_col - 1];
        /*endfor*/
    }
    /*endfor*/
    /* First element in current column */
    s->cur_col[0] = s->cbar;
    s->pivot_row = -1;
    min_ratio = LARGE;
    /* Ratio test */
    for (i = 0;  i <= s->m;  i++)
    {
        if (s->cur_col[i + 1] > EPS)
        {
            ratio = s->carry[i + 1][0]/s->cur_col[i + 1];
            if (min_ratio > ratio)
            {
                /* Favorable row */
                min_ratio = ratio;
                s->pivot_row = i;
                s->pivot_element = s->cur_col[i + 1];
            }
            else
            {
                /* Break tie with max pivot */
                if (min_ratio == ratio  &&  s->pivot_element < s->cur_col[i + 1])
                {
                    s->pivot_row = i;
                    s->pivot_element = s->cur_col[i + 1];
                }
                /*endif*/
            }
            /*endif*/
        }
        /*endif*/
    }
    /*endfor*/
    s->unbounded = (s->pivot_row == -1);
}
/*- End of function --------------------------------------------------------*/

static double pivot(struct meteor_working_data_s *s)
{
    int i;
    int j;

    s->basis[s->pivot_row] = s->pivot_col;
    for (j = 0;  j <= (s->m + 1);  j++)
        s->carry[s->pivot_row + 1][j] /= s->pivot_element;
    /*endfor*/
    for (i = 0;  i <= (s->m + 1);  i++)
    {
        if ((i - 1) != s->pivot_row)
        {
            for (j = 0;  j <= (s->m + 1);  j++)
                s->carry[i][j] -= s->carry[s->pivot_row + 1][j]*s->cur_col[i];
            /*endfor*/
        }
        /*endif*/
    }
    /*endfor*/
    return -s->carry[0][0];
}
/*- End of function --------------------------------------------------------*/

static double change_phase(struct meteor_working_data_s *s)
{
    int i;
    int j;
    int b;

    /* Change phase from 1 to 2, by switching to the original cost vector */
    s->phase = 2;
    for (i = 0;  i <= s->m;  i++)
    {
        if (s->basis[i] <= 0)
            printf("...artificial basis element %5d remains in basis after phase 1\n", s->basis[i]);
        /*endif*/
    }
    /*endfor*/
    /* Switch to original cost vector */
    for (i = 0;  i < s->num_cols;  i++)
        s->d[i] = s->c[i];
    /*endfor*/
    for (j = 0;  j <= (s->m + 1);  j++)
    {
        s->carry[0][j] = 0.0;
        for (i = 0;  i <= s->m;  i++)
        {
            /* Ignore artificial basis elements that are still in basis */
            b = s->basis[i];
            if (b >= 1)
                s->carry[0][j] -= s->c[b - 1]*s->carry[i + 1][j];
            /*endif*/
        }
        /*endfor*/
    }
    /*endfor*/
    return -s->carry[0][0];
}
/*- End of function --------------------------------------------------------*/

static double magnitude_response(struct meteor_working_data_s *s, double freq)
{
    int i;
    double temp;

    /* Compute magnitude function, given radian frequency f */
    temp = 0.0;
    for (i = 0;  i < s->m;  i++)
        temp += s->coeff[i]*trig0(s, i, freq);
    /*endfor*/
    return temp;
}
/*- End of function --------------------------------------------------------*/

static double half_magnitude_response(struct meteor_working_data_s *s, double freq)
{
    int i;
    double temp;

    /* Compute magnitude function, given radian frequency f */
    temp = 0.0;
    for (i = 0;  i < (s->m + 1)/2;  i++)
        temp += s->coeff[i]*trig0(s, i, freq);
    /*endfor*/
    return temp;
}
/*- End of function --------------------------------------------------------*/

static int simplex(struct meteor_working_data_s *s)
{
    int i;
    int j;
    int col;
    int row;
    bool done;

    /* Simplex for linear programming */
    done = false;
    s->phase = 1;
    for (i = 0;  i <= (s->m + 1);  i++)
    {
        for (j = 0;  j <= (MAX_COEFFS + 1);  j++)
            s->carry[i][j] = 0.0;
        /*endfor*/
    }
    /*endfor*/
    /* Artificial basis */
    for (i = 1;  i <= (s->m + 1);  i++)
        s->carry[i][i] = 1.0;
    /*endfor*/
    /* - initial cost */
    s->carry[0][0] = -1.0;
    s->cur_cost = -s->carry[0][0];
    /* Variable minimized in primal */
    s->carry[s->m + 1][0] = 1.0;
    /* Initial, artificial basis */
    for (i = 0;  i <= s->m;  i++)
        s->basis[i] = -i;
    /*endfor*/
    /* Check number of columns */
    if (s->num_cols <= NCOL_MAX)
    {
        /* Initialize cost for phase 1 */
        for (col = 0;  col < s->num_cols;  col++)
        {
            s->d[col] = 0.0;
            for (row = 0;  row <= s->m;  row++)
                s->d[col] -= s->tab[row][col];
            /*endfor*/
        }
        /*endfor*/
    }
    else
    {
        printf("...termination: too many columns for storage\n");
        done = true;
        s->result = too_many_columns;
    }
    /*endif*/
    s->num_pivots = 0;
    while (s->num_pivots < MAX_PIVOTS  &&  !done  &&  (s->cur_cost > s->low_limit  ||  s->phase == 1))
    {
        column_search(s);
        if (s->optimal)
        {
            if (s->phase == 1)
            {
                if (s->cur_cost > EPS)
                {
                    /* Dual of problem is infeasible */
                    /* This happens if all specs are hugged */
                    done = true;
                    s->result = infeasible_dual;
                }
                else
                {
                    if (s->num_pivots != 1  &&  s->num_pivots%10 != 0)
                        printf("Pivot %d cost = %.5f\n", s->num_pivots, s->cur_cost);
                    /*endif*/
                    printf("Phase 1 successfully completed\n");
                    s->cur_cost = change_phase(s);
                }
                /*endif*/
            }
            else
            {
                if (s->num_pivots != 1  &&  s->num_pivots%10 != 0)
                    printf("Pivot %d cost = %.5f\n", s->num_pivots, s->cur_cost);
                /*endif*/
                printf("Phase 2 successfully completed\n");
                done = true;
                s->result = optimum_obtained;
            }
            /*endif*/
        }
        else
        {
            row_search(s);
            if (s->unbounded)
            {
                /* Dual of problem is unbounded */
                done = true;
                s->result = unbounded_dual;
            }
            else
            {
                s->cur_cost = pivot(s);
                s->num_pivots++;
                if (s->num_pivots == 1  ||  s->num_pivots%10 == 0)
                    printf("Pivot %d cost = %.5f\n", s->num_pivots, s->cur_cost);
                /*endif*/
            }
            /*endif*/
        }
        /*endif*/
    }
    /*endwhile*/
    if (s->cur_cost <= s->low_limit  &&  s->phase == 2)
    {
        if (s->num_pivots != 1  &&  s->num_pivots%10 != 0)
            printf("Pivot %d cost = %.5f\n", s->num_pivots, s->cur_cost);
        /*endif*/
        s->result = infeasible_primal;
    }
    /*endif*/
    if (s->num_pivots >= MAX_PIVOTS)
    {
        printf("...termination: maximum number of pivots exceeded\n");
        s->result = too_many_pivots;
    }
    /*endif*/

    /* Optimal */
    return s->result;
}
/*- End of function --------------------------------------------------------*/

static void print_result(int result)
{
    /* Print enumerated result type */
    switch (result)
    {
    case badly_formed_requirements:
        printf("badly formed requirements\n");
        break;
    case optimum_obtained:
        printf("optimum obtained\n");
        break;
    case too_many_columns:
        printf("too many columns in specifications\n");
        break;
    case too_many_pivots:
        printf("too many pivots\n");
        break;
    case unbounded_dual:
        printf("infeasible (unbounded dual)\n");
        break;
    case infeasible_dual:
        printf("infeasible or unbounded\n");
        break;
    case infeasible_primal:
        printf("infeasible\n");
        break;
    case no_feasible_solution_found:
        printf("no infeasible solution found\n");
        break;
    case no_feasible_band_edge_found:
        printf("no infeasible bend edge found\n");
        break;
    }
    /*endswitch*/
}
/*- End of function --------------------------------------------------------*/

static int get_m(struct meteor_working_data_s *s)
{
    int left_m;
    int right_m;
    bool found_m;
    bool checked_left;
    bool checked_right;
    int result;
    int length;
    int i;

    /* Find best order (and hence length) */
    s->found_feasible_solution = false;
    left_m = s->smallest_m;
    right_m = s->largest_m;
    found_m = false;
    checked_left = false;
    checked_right = false;
    for (s->iteration = 0;  !found_m;  s->iteration++)
    {
        if (s->iteration == 0)
        {
            /* First time through */
            s->m = left_m + (right_m - left_m)/2;
        }
        /*endif*/
        printf("\nIteration %d\n", s->iteration);

        if (s->odd_length)
        {
            if (s->spec->symmetry_type == symmetry_cosine)
                length = s->m*2 - 1;
            else
                length = s->m*2 + 1;
            /*endif*/
        }
        else
        {
            length = s->m*2;
        }
        /*endif*/
        printf("L=%d\n", length);

        setup(s);
        result = simplex(s);
        print_result(result);
        if (result == optimum_obtained)
        {
            s->found_feasible_solution = true;
            right_m = s->m;
            s->best_m = s->m;
            /* Right side of bracket has been checked */
            checked_right = true;
            if (s->odd_length)
            {
                if (s->spec->symmetry_type == symmetry_cosine)
                    length = s->best_m*2 - 1;
                else
                    length = s->best_m*2 + 1;
                /*endif*/
            }
            else
            {
                length = s->best_m*2;
            }
            /*endif*/
            printf("New best length L=%d\n", length);

            for (i = 0;  i < s->m;  i++)
                s->coeff[i] = -s->carry[0][i + 1];
            /*endfor*/
        }
        /*endif*/

        if (result != optimum_obtained)
        {
            left_m = s->m;
            /* Left side of bracket has been checked */
            checked_left = true;
        }
        /*endif*/

        if (right_m > left_m + 1)
            s->m = left_m + (right_m - left_m)/2;
        /*endif*/

        if (right_m == left_m + 1)
        {
            if (!checked_left)
            {
                s->m = left_m;
                checked_left = true;
            }
            else if (!checked_right)
            {
                s->m = right_m;
                checked_right = true;
            }
            else
            {
                found_m = true;
            }
            /*endif*/
        }
        /*endif*/

        if (right_m == left_m)
            found_m = true;
        /*endif*/
    }
    /*endfor*/

    if (!s->found_feasible_solution)
        return no_feasible_solution_found;
    /*endif*/
    s->m = s->best_m;

    putchar('\n');
    if (s->odd_length)
    {
        if (s->spec->symmetry_type == symmetry_cosine)
            printf("Best length L=%d\n", s->best_m*2 - 1);
        else
            printf("Best length L=%d\n", s->best_m*2 + 1);
        /*endif*/
    }
    /*endif*/

    if (!s->odd_length)
        printf("Best length L=%d\n", s->best_m*2);
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int get_edge(struct meteor_working_data_s *s)
{
    double left_edge;
    double right_edge;
    double newe;
    double beste;
    double one_space;
    double stop_space;
    int i;

    /* Optimize band edge */
    one_space = 0.5/s->spec->grid_points;   /* Space between grid points */
    stop_space = one_space/10.0;
    /* Stop criterion is 1/10 nominal grid spacing */
    if (s->which_way == rr)
    {
        /* Start with rightmost left edge */
        left_edge = s->spec->spec[s->spec->spec[0].band_pushed - 1].left_freq;
        for (i = 1;  i < s->num_pushed;  i++)
        {
            if (s->spec->spec[s->spec->spec[i].band_pushed - 1].left_freq > left_edge)
                left_edge = s->spec->spec[s->spec->spec[i].band_pushed - 1].left_freq;
            /*endif*/
        }
        /*endfor*/
        right_edge = 0.5;
    }
    else
    {
        /* Start with leftmost right edge */
        left_edge = 0.0;
        right_edge = s->spec->spec[s->spec->spec[0].band_pushed - 1].right_freq;
        for (i = 1;  i < s->num_pushed;  i++)
        {
            if (s->spec->spec[s->spec->spec[i].band_pushed - 1].right_freq < right_edge)
                right_edge = s->spec->spec[s->spec->spec[i].band_pushed - 1].right_freq;
            /*endif*/
        }
        /*endfor*/
    }
    /*endif*/
    s->found_feasible_solution = false;
    for (s->iteration = 0;  (right_edge - left_edge) > stop_space;  s->iteration++)
    {
        newe = (right_edge + left_edge)/2.0;
        printf("\nIteration %d\n", s->iteration);
        printf("Trying new edge = %10.4f\n", newe);
        for (i = 0;  i < s->num_pushed;  i++)
        {
            if (s->which_way == rr)
                s->spec->spec[s->spec->spec[i].band_pushed - 1].right_freq = newe;
            else
                s->spec->spec[s->spec->spec[i].band_pushed - 1].left_freq = newe;
            /*endif*/
        }
        /*endif*/
        setup(s);
        s->result = simplex(s);
        print_result(s->result);
        if (s->result == optimum_obtained)
        {
            if (s->which_way == rr)
                left_edge = newe;
            else
                right_edge = newe;
            /*endif*/
            s->found_feasible_solution = true;
            beste = newe;
            for (i = 0;  i < s->m;  i++)
                s->coeff[i] = -s->carry[0][i + 1];
            /*endfor*/
        }
        else
        {
            if (s->which_way == rr)
                right_edge = newe;
            else
                left_edge = newe;
            /*endif*/
        }
        /*endif*/
    }
    /*endfor*/
    putchar('\n');
    if (!s->found_feasible_solution)
        return no_feasible_band_edge_found;
    /*endif*/
    printf("Found edge = %10.4f\n", beste);
    for (i = 0;  i < s->num_pushed;  i++)
    {
        if (s->which_way == rr)
            s->spec->spec[s->spec->spec[i].band_pushed - 1].right_freq = beste;
        else
            s->spec->spec[s->spec->spec[i].band_pushed - 1].left_freq = beste;
        /*endif*/
    }
    /*endfor*/
    for (i = 0;  i < s->spec->num_specs;  i++)
        make_bands(s, i);
    /*endfor*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int get_max_dist(struct meteor_working_data_s *s)
{
    int i;

    /* Maximize distance from constraints */
    printf("Optimization: maximize distance from constraints\n");
    setup(s);
    s->result = simplex(s);
    print_result(s->result);
    if (s->result != optimum_obtained)
        return s->result;
    /*endif*/
    printf("Final cost = distance from constraints = %.5f\n", s->cur_cost);
    /* Record coefficients */
    for (i = 0;  i < s->m;  i++)
        s->coeff[i] = -s->carry[0][i + 1];
    /*endfor*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int meteor_get_coefficients(struct meteor_working_data_s *s, double coeffs[])
{
    int i;
    int j;

    j = 0;
    if (s->odd_length  &&  s->spec->symmetry_type == symmetry_cosine)
    {
        for (i = s->m - 1;  i >= 1;  i--)
            coeffs[j++] = s->coeff[i]/2.0;
        /*endfor*/
        coeffs[j++] = s->coeff[0];
        for (i = 1;  i < s->m;  i++)
            coeffs[j++] = s->coeff[i]/2.0;
        /*endfor*/
    }
    else if (!s->odd_length  &&  s->spec->symmetry_type == symmetry_cosine)
    {
        for (i = s->m - 1;  i >= 0;  i--)
            coeffs[j++] = s->coeff[i]/2.0;
        /*endfor*/
        for (i = 0;  i < s->m;  i++)
            coeffs[j++] = s->coeff[i]/2.0;
        /*endfor*/
    }
    else if (s->odd_length  &&  s->spec->symmetry_type == symmetry_sine)
    {
        /* L = length, odd */
        /* Negative of the first m coefs. */
        for (i = s->m - 1;  i >= 0;  i--)
            coeffs[j++] = -s->coeff[i]/2.0;
        /*endfor*/
        /* Middle coefficient is always 0 */
        coeffs[j++] = 0.0;
        for (i = 0;  i < s->m;  i++)
            coeffs[j++] = s->coeff[i]/2.0;
        /*endfor*/
    }
    else if (!s->odd_length  &&  s->spec->symmetry_type == symmetry_sine)
    {
        /* Negative of the first m coefs. */
        for (i = s->m - 1;  i >= 0;  i--)
            coeffs[j++] = -s->coeff[i]/2.0;
        /*endfor*/
        for (i = 0;  i < s->m;  i++)
            coeffs[j++] = s->coeff[i]/2.0;
        /*endfor*/
    }
    /*endif*/
    return j;
}
/*- End of function --------------------------------------------------------*/

static int vet_data(struct meteor_working_data_s *s)
{
    int i;
    bool all_hugged;
    char ch;

    printf("Filter name: '%s'\n", s->spec->filter_name);

    if (s->spec->shortest < 1  ||  s->spec->longest > MAX_TAPS)
    {
        printf("Shortest or longest out of range\n");
        return badly_formed_requirements;
    }
    /*endif*/

    if ((s->spec->shortest & 1) != (s->spec->longest & 1))
    {
        printf("Parity of smallest andlongest unequal\n");
        return badly_formed_requirements;
    }
    /*endif*/

    s->odd_length = s->spec->shortest & 1;
    if (s->odd_length)
    {
        if (s->spec->symmetry_type == symmetry_cosine)
        {
            s->smallest_m = (s->spec->shortest + 1)/2;
            s->largest_m = (s->spec->longest + 1)/2;
        }
        else
        {
            s->smallest_m = (s->spec->shortest - 1)/2;
            s->largest_m = (s->spec->longest - 1)/2;
        }
        /*endif*/
    }
    else
    {
        s->smallest_m = s->spec->shortest/2;
        s->largest_m = s->spec->longest/2;
    }
    /*endif*/

    if (s->spec->shortest != s->spec->longest)
    {
        s->what_to_do = find_len;
        printf("Finding minimum length: range %d to %d\n", s->spec->shortest, s->spec->longest);
    }
    else
    {
        s->m = s->smallest_m;
        s->length = s->spec->shortest;

        printf("Fixed length of %4d\n", s->length);
        scanf("%c%*[^\n]", &ch);
        /* Right, left, or neither: edges to be pushed? */
        getchar();

        if (ch == 'n')
        {
            s->what_to_do = max_dist;
        }
        else
        {
            s->what_to_do = push_edge;

            s->which_way = (ch == 'r')  ?  rr  :  ll;

            scanf("%d%*[^\n]", &s->num_pushed);
            getchar();
            for (i = 0;  i < s->num_pushed;  i++)
                scanf("%d", &s->spec->spec[i].band_pushed);
            /*endfor*/
            scanf("%*[^\n]");
            getchar();

            printf("Pushing band edges right\n", (s->which_way == rr)  ?  "right"  :  "left");

            printf("Constraint numbers: ");
            for (i = 0;  i < s->num_pushed;  i++)
                printf("%3d ", s->spec->spec[i].band_pushed);
            /*endfor*/
            putchar('\n');
        }
        /*endif*/
    }
    /*endif*/

    for (i = 0;  i < s->spec->num_specs;  i++)
    {
        printf("Constraint name '%s'\n", s->spec->spec[i].name);
        switch (s->spec->spec[i].type)
        {
        case constraint_type_convexity:
            switch (s->spec->spec[i].sense)
            {
            case sense_convex:
                printf("Constraint %2d: convexity, sense convex\n", i);
                break;
            case sense_concave:
                printf("Constraint %2d: convexity, sense concave\n", i);
                break;
            }
            /*endswitch*/

            printf("  Band edges: %10.4f %10.4f\n", s->spec->spec[i].left_freq, s->spec->spec[i].right_freq);
            break;
        case constraint_type_limit:
            if (s->spec->spec[i].interpolation == interpolation_geometric  &&  s->spec->spec[i].left_bound*s->spec->spec[i].right_bound == 0.0)
            {
                printf("Geometrically interpolated band edge in constraint %5d is zero\n", i);
                return badly_formed_requirements;
            }
            /*endif*/

            switch (s->spec->spec[i].sense)
            {
            case sense_lower:
                printf("  Constraint %2d: lower limit\n", i);
                break;
            case sense_upper:
                printf("  Constraint %2d: upper limit\n", i);
                break;
            case sense_envelope:
                printf("  Constraint %2d: envelope limit\n", i);
                break;
            }
            /*endswitch*/

            switch (s->spec->spec[i].interpolation)
            {
            case interpolation_geometric:
                printf("  Geometric interpolation\n");
                break;
            case interpolation_arithmetic:
                printf("  Arithmetic interpolation\n");
                break;
            }
            /*endswitch*/

            if (s->spec->spec[i].hug)
                printf("  This constraint will be hugged\n");
            else
                printf("  This constraint will be optimized\n");
            /*endif*/

            printf("  Band edges: %10.4f %10.4f\n", s->spec->spec[i].left_freq, s->spec->spec[i].right_freq);
            printf("  Bounds:     %10.4f %10.4f\n", s->spec->spec[i].left_bound, s->spec->spec[i].right_bound);
            break;
        }
        /*endswitch*/
        make_bands(s, i);
        printf("  Initial columns:    %10d %10d\n", s->spec->spec[i].first_col, s->spec->spec[i].last_col);
    }
    s->num_cols = s->spec->spec[s->spec->num_specs - 1].last_col;

    printf("Number of specs = %5d\n", s->spec->num_specs);
    printf("Initial number of columns = %5d\n", s->num_cols);

    all_hugged = true;
    for (i = 0;  i < s->spec->num_specs;  i++)
    {
        if (s->spec->spec[i].type == constraint_type_limit  &&  !s->spec->spec[i].hug)
            all_hugged = false;
        /*endif*/
    }
    /*endfor*/

    if (all_hugged)
    {
        printf("All constraints are hugged: ill-posed problem\n");
        return badly_formed_requirements;
    }
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

void output_filter_performance_as_csv_file(struct meteor_working_data_s *s, const char *file_name)
{
    int i;
    double mg;
    double mg2;
    FILE *magnitude_file;

    if (s->log_fd)
    {
        magnitude_file = s->log_fd;
    }
    else
    {
        /* Print final frequency response */
        if ((magnitude_file = fopen(file_name, "wb")) == NULL)
        {
            fprintf(stderr, "Cannot open file '%s'\n", file_name);
            exit(2);
        }
        /*endif*/
    }
    /*endif*/

    if (s->spec->filter_name  &&  s->spec->filter_name[0])
    {
        fprintf(magnitude_file, "%s\n", s->spec->filter_name);
    }
    /*endif*/
    fprintf(magnitude_file, "Frequency, Gain (dB), Gain (linear), Half gain (linear)\n");
    /* Magnitude on regular grid */
    for (i = 0;  i <= s->spec->grid_points;  i++)
    {
        mg = fabs(magnitude_response(s, i*M_PI/s->spec->grid_points));
        if (mg == 0.0)
            mg = SMALL;
        /*endif*/
        mg2 = fabs(half_magnitude_response(s, i*M_PI/s->spec->grid_points));
        if (mg2 == 0.0)
            mg2 = SMALL;
        /*endif*/
        fprintf(magnitude_file, "%10.4lf, %.10lf, %.5lf, %.5lf\n", 0.5*s->spec->sample_rate*i/s->spec->grid_points, 20.0*log10(mg), mg, mg2);
    }
    /*endfor*/
    fprintf(magnitude_file, "\nMagnitude at band edges\n\n");
    for (i = 0;  i < s->spec->num_specs;  i++)
    {
        if (s->spec->spec[i].type == constraint_type_limit)
        {
            fprintf(magnitude_file, "%10.4f %.5E\n",
                    s->freq[s->spec->spec[i].first_col - 1]*0.5/M_PI, magnitude_response(s, s->freq[s->spec->spec[i].first_col - 1]));
            fprintf(magnitude_file, "%10.4f %.5E\n",
                    s->freq[s->spec->spec[i].last_col - 1]*0.5/M_PI, magnitude_response(s, s->freq[s->spec->spec[i].last_col - 1]));
            putchar('\n');
        }
        /*endif*/
    }
    /*endfor*/
    if (s->log_fd == NULL)
        fclose(magnitude_file);
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

int meteor_design_filter(struct meteor_working_data_s *s, struct meteor_spec_s *t, double coeffs[])
{
    int res;

    memset(s, 0, sizeof(*s));

    s->spec = t;
    if ((res = vet_data(s)) < 0)
        return res;
    /*endif*/
    /* Dual cost negative => primal infeasible */
    s->low_limit = -EPS;
    switch (s->what_to_do)
    {
    case find_len:
        res = get_m(s);
        break;
    case push_edge:
        res = get_edge(s);
        break;
    case max_dist:
        res = get_max_dist(s);
        break;
    }
    /*endswitch*/
    if (res < 0)
        return res;
    /*endif*/
    return meteor_get_coefficients(s, coeffs);
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
