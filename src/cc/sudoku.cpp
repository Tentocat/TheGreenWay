// start https://github.com/attractivechaos/plb/blob/master/sudoku/incoming/sudoku_solver.c
/************************************************************************************/
/*                                                                                  */
/* Author: Bill DuPree                                                              */
/* Name: sudoku_solver.c                                                            */
/* Language: C                                                                      */
/* Date: Feb. 25, 2006                                                              */
/* Copyright (C) Feb. 25, 2006, All rights reserved.                                */
/*                                                                                  */
/* This is a program that solves Su Doku (aka Sudoku, Number Place, etc.) puzzles   */
/* primarily using deductive logic. It will only resort to trial-and-error and      */
/* backtracking approaches upon exhausting all of its deductive moves.              */
/*                                                                                  */
/* Puzzles must be of the standard 9x9 variety using the (ASCII) characters '1'     */
/* through '9' for the puzzle solution set. Puzzles should be submitted as 81       */
/* character strings which, when read left-to-right will fill a 9x9 Sudoku grid     */
/* from left-to-right and top-to-bottom. In the puzzle specification, the           */
/* characters 1 - 9 represent the puzzle "givens" or clues. Any other non-blank     */
/* character represents an unsolved cell.                                           */
/*                                                                                  */
/* The puzzle solving algorithm is "home grown." I did not borrow any of the usual  */
/* techniques from the literature, e.g. Donald Knuth's "Dancing Links." Instead     */
/* I "rolled my own" from scratch. As such, its performance can only be blamed      */
/* on yours truly. Still, I feel it is quite fast. On a 333 MHz Pentium II Linux    */
/* box it solves typical medium force puzzles in approximately 800 microseconds or  */
/* about 1,200 puzzles per second, give or take. On an Athlon XP 3000 (Barton core) */
/* it solves about 6,600 puzzles per sec.                                           */
/*                                                                                  */
/* DESCRIPTION OF ALGORITHM:                                                        */
/*                                                                                  */
/* The puzzle algorithm initially assumes every unsolved cell can assume every      */
/* possible value. It then uses the placement of the givens to refine the choices   */
/* available to each cell. I call this the markup phase.                            */
/*                                                                                  */
/* After markup completes, the algorithm then looks for "singleton" cells with      */
/* values that, due to constraints imposed by the row, column, or 3x3 region, may   */
/* only assume one possible value. Once these cells are assigned values, the        */
/* algorithm returns to the markup phase to apply these changes to the remaining    */
/* candidate solutions. The markup/singleton phases alternate until either no more  */
/* changes occur, or the puzzle is solved. I call the markup/singleton elimination  */
/* loop the "Simple Solver" because in a large percentage of cases it solves the    */
/* puzzle.                                                                          */
/*                                                                                  */
/* If the simple solver portion of the algorithm doesn't produce a solution, then   */
/* more advanced deductive rules are applied. I've implemented two additional rules */
/* as part of the deductive puzzle solver. The first is subset elimination wherein  */
/* a row/column/region is scanned for X number of cells with X number of matching   */
/* candidate solutions. If such subsets are found in the row, column, or region,    */
/* then the candidates values from the subset may be eliminated from all other      */
/* unsolved cells within the row, column, or region, respectively.                  */
/*                                                                                  */
/* The second advanced deductive rule examines each region looking for candidate    */
/* values that exclusively align themselves along a single row or column, i.e. a    */
/* a vector. If such candidate values are found, then they may be eliminated from   */
/* the cells outside of the region that are part of the aligned row or column.      */
/*                                                                                  */
/* Note that each of the advanced deductive rules calls all preceeding rules, in    */
/* order, if that advanced rule has effected a change in puzzle markup.             */
/*                                                                                  */
/* Finally, if no solution is found after iteratively applying all deductive rules, */
/* then we begin trial-and-error using recursion for backtracking. A working copy   */
/* is created from our puzzle, and using this copy the first cell with the          */
/* smallest number of candidate solutions is chosen. One of the solutions values is */
/* assigned to that cell, and the solver algorithm is called using this working     */
/* copy as its starting point. Eventually, either a solution, or an impasse is      */
/* reached.                                                                         */
/*                                                                                  */
/* If we reach an impasse, the recursion unwinds and the next trial solution is     */
/* attempted. If a solution is found (at any point) the values for the solution are */
/* added to a list. Again, so long as we are examining all possibilities, the       */
/* recursion unwinds so that the next trial may be attempted. It is in this manner  */
/* that we enumerate puzzles with multiple solutions.                               */
/*                                                                                  */
/* Note that it is certainly possible to add to the list of applied deductive       */
/* rules. The techniques known as "X-Wing" and "Swordfish" come to mind. On the     */
/* other hand, adding these additional rules will, in all likelihood, slow the      */
/* solver down by adding to the computational burden while producing very few       */
/* results. I've seen the law of diminishing returns even in some of the existing   */
/* rules, e.g. in subset elimination I only look at two and three valued subsets    */
/* because taking it any further than that degraded performance.                    */
/*                                                                                  */
/* PROGRAM INVOCATION:                                                              */
/*                                                                                  */
/* This program is a console (or command line) based utility and has the following  */
/* usage:                                                                           */
/*                                                                                  */
/*      sudoku_solver {-p puzzle | -f <puzzle_file>} [-o <outfile>]                 */
/*              [-r <reject_file>] [-1][-a][-c][-g][-l][-m][-n][-s]                 */
/*                                                                                  */
/* where:                                                                           */
/*                                                                                  */
/*        -1      Search for first solution, otherwise all solutions are returned   */
/*        -a      Requests that the answer (solution) be printed                    */
/*        -c      Print a count of solutions for each puzzle                        */
/*        -d      Print the recursive trial depth required to solve the puzzle      */
/*        -e      Print a step-by-step explanation of the solution(s)               */
/*        -f      Takes an argument which specifes an input file                    */
/*                containing one or more unsolved puzzles (default: stdin)          */
/*        -G      Print the puzzle solution(s) in a 9x9 grid format                 */
/*        -g      Print the number of given clues                                   */
/*        -l      Print the recursive trial depth required to solve the puzzle      */
/*        -m      Print an octal mask for the puzzle givens                         */
/*        -n      Number each result                                                */
/*        -o      Specifies an output file for the solutions (default: stdout)      */
/*        -p      Takes an argument giving a single inline puzzle to be solved      */
/*        -r      Specifies an output file for unsolvable puzzles                   */
/*                (default: stderr)                                                 */
/*        -s      Print the puzzle's score or difficulty rating                     */
/*        -?      Print usage information                                           */
/*                                                                                  */
/* The return code is zero if all puzzles had unique solutions,                     */
/* (or have one or more solutions when -1 is specified) and non-zero                */
/* when no unique solution exists.                                                  */
/*                                                                                  */
/* PUZZLE SCORING                                                                   */
/*                                                                                  */
/* A word about puzzle scoring, i.e. rating a puzzle's difficulty, is in order.     */
/* Rating Sudoku puzzles is a rather subjective thing, and thus it is difficult to  */
/* really develop an objective puzzle rating system. I, however, have attempted     */
/* this feat (several times with varying degrees of success ;-) and I think the     */
/* heuristics I'm currently applying aren't too bad for rating the relative         */
/* difficulty of solving a puzzle.                                                  */
/*                                                                                  */
/* The following is a brief rundown of how it works. The initial puzzle markup is   */
/* a "free" operation, i.e. no points are scored for the first markup pass. I feel  */
/* this is appropriate because a person solving a puzzle will always have to do     */
/* their own eyeballing and scanning of the puzzle. Subsequent passes are           */
/* scored at one point per candidate eliminated because these passes indicate       */
/* that more deductive work is required. Secondly, the "reward" for solving a cell  */
/* is set to one point, and as long as the solution only requires simple markup     */
/* and elimination of singletons, this level of reward remains unchanged.           */
/*                                                                                  */
/* This reward changes, however, when advanced solving rules are required. Puzzles  */
/* that remain unsolved after the first pass through the simple solver phase have   */
/* a higher "reward", i.e. it is incremented by two. Thus, if subset or vector      */
/* elimination is required, all subsequently solved cells score higher bounties.    */
/* In addition, the successful application of these deductive techniques score      */
/* their own penalties.                                                             */
/*                                                                                  */
/* Finally, if a trial-and-error approach is called for, then the "reward" is       */
/* incremented by another five points. Thus, the total penalty for each level of    */
/* recursion is an additional seven points per solved cell, i.e.                    */
/* (recursive_depth * 7) + 1 points per solved cell. Trial solutions are also       */
/* penalized by a weighting factor that is based upon the number of unsolved cells  */
/* that remain upon reentry to the solver and the depth of recursion. (I've seen a  */
/* pathological puzzle from the "Minimum Sudoku" web site require 16 levels of      */
/* recursion and score a whopping 228,642 points using this scoring system!)        */
/*                                                                                  */
/* And that brings me to this topic: What do all these points mean?                 */
/*                                                                                  */
/* Well, who knows? This is still subjective, and the weighting system I've chosen  */
/* for point scoring is is largely arbitrary. But based upon feedback from a number */
/* of individuals, a rough scale of difficulty plays out as follows:                */
/*                                                                                  */
/*   DEGREE OF DIFFICULTY   |  SCORE                                                */
/* -------------------------+------------------------------------------             */
/*   TRIVIAL                |  80 points or less                                    */
/*   EASY                   |  81 - 150 points                                      */
/*   MEDIUM                 |  151 - 250 points                                     */
/*   HARD                   |  251 - 400 points                                     */
/*   VERY HARD              |  401 - 900 points                                     */
/*   DIABOLICAL             |  901 and up                                           */
/*                                                                                  */
/* Experience shows that puzzles in the HARD category, in a few cases, will         */
/* require a small amount of trial-and-error. The VERY HARD puzzles will likely     */
/* require trial-and-error, and in some cases more than one level of trial-and-     */
/* error. As for the DIABOLICAL puzzles--why waste your time? These are best left   */
/* to masochists, savants and automated solvers. YMMV.                              */
/*                                                                                  */
/* LICENSE:                                                                         */
/*                                                                                  */
/* This program is free software; you can redistribute it and/or modify             */
/* it under the terms of the GNU General Public License as published by             */
/* the Free Software Foundation; either version 2 of the License, or                */
/* (at your option) any later version.                                              */
/*                                                                                  */
/* This program is distributed in the hope that it will be useful,                  */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of                   */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                    */
/* GNU General Public License for more details.                                     */
/*                                                                                  */
/* You should have received a copy of the GNU General Public License                */
/* along with this program; if not, write to the Free Software                      */
/* Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA       */
/*                                                                                  */
/* CONTACT:                                                                         */
/*                                                                                  */
/* Email: bdupree@techfinesse.com                                                   */
/* Post: Bill DuPree, 609 Wenonah Ave, Oak Park, IL 60304 USA                       */
/*                                                                                  */
/************************************************************************************/
/*                                                                                  */
/* CHANGE LOG:                                                                      */
/*                                                                                  */
/* Rev.	  Date        Init.	Description                                         */
/* -------------------------------------------------------------------------------- */
/* 1.00   2006-02-25  WD	Initial version.                                    */
/* 1.01   2006-03-13  WD	Fixed return code calc. Added signon message.       */
/* 1.10   2006-03-20  WD        Added explain option, add'l speed optimizations     */
/* 1.11   2006-03-23  WD        More simple speed optimizations, cleanup, bug fixes */
/*                                                                                  */
/************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <limits.h>

#define SUDOKU_VERSION "1.11"

#define PUZZLE_ORDER 3
#define PUZZLE_DIM (PUZZLE_ORDER*PUZZLE_ORDER)
#define PUZZLE_CELLS (PUZZLE_DIM*PUZZLE_DIM)

/* Command line options */
#ifdef EXPLAIN
#define OPTIONS "?1acdef:Ggmno:p:r:s"
#else
#define OPTIONS "?1acdf:Ggmno:p:r:s"
#endif
extern char *optarg;
extern int optind, opterr, optopt;

static char *myname;    /* Name that we were invoked under */

static FILE *solnfile, *rejects;

/* This is the list of cell coordinates specified on a row basis */

static int const row[PUZZLE_DIM][PUZZLE_DIM] = {
    {  0,  1,  2,  3,  4,  5,  6,  7,  8 },
    {  9, 10, 11, 12, 13, 14, 15, 16, 17 },
    { 18, 19, 20, 21, 22, 23, 24, 25, 26 },
    { 27, 28, 29, 30, 31, 32, 33, 34, 35 },
    { 36, 37, 38, 39, 40, 41, 42, 43, 44 },
    { 45, 46, 47, 48, 49, 50, 51, 52, 53 },
    { 54, 55, 56, 57, 58, 59, 60, 61, 62 },
    { 63, 64, 65, 66, 67, 68, 69, 70, 71 },
    { 72, 73, 74, 75, 76, 77, 78, 79, 80 }};

/* This is the list of cell coordinates specified on a column basis */

static int const col[PUZZLE_DIM][PUZZLE_DIM] = {
    {  0,  9, 18, 27, 36, 45, 54, 63, 72 },
    {  1, 10, 19, 28, 37, 46, 55, 64, 73 },
    {  2, 11, 20, 29, 38, 47, 56, 65, 74 },
    {  3, 12, 21, 30, 39, 48, 57, 66, 75 },
    {  4, 13, 22, 31, 40, 49, 58, 67, 76 },
    {  5, 14, 23, 32, 41, 50, 59, 68, 77 },
    {  6, 15, 24, 33, 42, 51, 60, 69, 78 },
    {  7, 16, 25, 34, 43, 52, 61, 70, 79 },
    {  8, 17, 26, 35, 44, 53, 62, 71, 80 }};

/* This is the list of cell coordinates specified on a 3x3 region basis */

static int const region[PUZZLE_DIM][PUZZLE_DIM] = {
    {  0,  1,  2,  9, 10, 11, 18, 19, 20 },
    {  3,  4,  5, 12, 13, 14, 21, 22, 23 },
    {  6,  7,  8, 15, 16, 17, 24, 25, 26 },
    { 27, 28, 29, 36, 37, 38, 45, 46, 47 },
    { 30, 31, 32, 39, 40, 41, 48, 49, 50 },
    { 33, 34, 35, 42, 43, 44, 51, 52, 53 },
    { 54, 55, 56, 63, 64, 65, 72, 73, 74 },
    { 57, 58, 59, 66, 67, 68, 75, 76, 77 },
    { 60, 61, 62, 69, 70, 71, 78, 79, 80 }};

/* Flags for cellflags member */
#define GIVEN 1
#define FOUND 2
#define STUCK 3

/* Return codes for funcs that modify puzzle markup */
#define NOCHANGE 0
#define CHANGE   1

typedef struct grd {
    short cellflags[PUZZLE_CELLS];
    short solved[PUZZLE_CELLS];
    short cell[PUZZLE_CELLS];
    short tail, givens, exposed, maxlvl, inc, reward;
    unsigned int score, solncount;
    struct grd *next;
} grid;

typedef int (*return_soln)(grid *g);

static grid *soln_list = NULL;

typedef struct {
    short row, col, region;
} cellmap;

/* Array structure to help map cell index back to row, column, and region */
static cellmap const map[PUZZLE_CELLS] = {
    { 0, 0, 0 },
    { 0, 1, 0 },
    { 0, 2, 0 },
    { 0, 3, 1 },
    { 0, 4, 1 },
    { 0, 5, 1 },
    { 0, 6, 2 },
    { 0, 7, 2 },
    { 0, 8, 2 },
    { 1, 0, 0 },
    { 1, 1, 0 },
    { 1, 2, 0 },
    { 1, 3, 1 },
    { 1, 4, 1 },
    { 1, 5, 1 },
    { 1, 6, 2 },
    { 1, 7, 2 },
    { 1, 8, 2 },
    { 2, 0, 0 },
    { 2, 1, 0 },
    { 2, 2, 0 },
    { 2, 3, 1 },
    { 2, 4, 1 },
    { 2, 5, 1 },
    { 2, 6, 2 },
    { 2, 7, 2 },
    { 2, 8, 2 },
    { 3, 0, 3 },
    { 3, 1, 3 },
    { 3, 2, 3 },
    { 3, 3, 4 },
    { 3, 4, 4 },
    { 3, 5, 4 },
    { 3, 6, 5 },
    { 3, 7, 5 },
    { 3, 8, 5 },
    { 4, 0, 3 },
    { 4, 1, 3 },
    { 4, 2, 3 },
    { 4, 3, 4 },
    { 4, 4, 4 },
    { 4, 5, 4 },
    { 4, 6, 5 },
    { 4, 7, 5 },
    { 4, 8, 5 },
    { 5, 0, 3 },
    { 5, 1, 3 },
    { 5, 2, 3 },
    { 5, 3, 4 },
    { 5, 4, 4 },
    { 5, 5, 4 },
    { 5, 6, 5 },
    { 5, 7, 5 },
    { 5, 8, 5 },
    { 6, 0, 6 },
    { 6, 1, 6 },
    { 6, 2, 6 },
    { 6, 3, 7 },
    { 6, 4, 7 },
    { 6, 5, 7 },
    { 6, 6, 8 },
    { 6, 7, 8 },
    { 6, 8, 8 },
    { 7, 0, 6 },
    { 7, 1, 6 },
    { 7, 2, 6 },
    { 7, 3, 7 },
    { 7, 4, 7 },
    { 7, 5, 7 },
    { 7, 6, 8 },
    { 7, 7, 8 },
    { 7, 8, 8 },
    { 8, 0, 6 },
    { 8, 1, 6 },
    { 8, 2, 6 },
    { 8, 3, 7 },
    { 8, 4, 7 },
    { 8, 5, 7 },
    { 8, 6, 8 },
    { 8, 7, 8 },
    { 8, 8, 8 }
};

static const short symtab[1<<PUZZLE_DIM] = {
    '.','1','2','.','3','.','.','.','4','.','.','.','.','.','.','.','5','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.',
    '6','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.',
    '7','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.',
    '.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.',
    '8','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.',
    '.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.',
    '.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.',
    '.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.',
    '9','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.',
    '.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.',
    '.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.',
    '.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.',
    '.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.',
    '.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.',
    '.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.',
    '.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.'};

static int enumerate_all = 1;
static int lvl = 0;

#ifdef EXPLAIN
static int explain = 0;
#endif

/* Function prototype(s) */
static int mult_elimination(grid *g);
static void print_grid(char *sud, FILE *h);
static char *format_answer(grid *g, char *outbuf);
static void diagnostic_grid(grid *g, FILE *h);

static inline int is_given(int c) { return (c >= '1') && (c <= '9'); }

#if defined(DEBUG)
static void mypause()
{
    char buf[8];
    LogPrintf("\tPress enter -> ");
    fgets(buf, 8, stdin);
}
#endif

#if 0
/* Generic (and slow) bitcount function */
static int bitcount(short cell)
{
    int i, count, mask;
    
    mask = 1;
    for (i = count = 0; i < 16; i++) {
        if (mask & cell) count++;
        mask <<= 1;
    }
    return count;
}
#endif

/*****************************************************/
/* Return the number of '1' bits in a cell.          */
/* Rather than count bits, do a quick table lookup.  */
/* Warning: Only valid for 9 low order bits.         */
/*****************************************************/

static inline short bitcount(short cell)
{
    static const short bcounts[512] = {
        0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4,1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,
        1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
        1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
        2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
        1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
        2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
        2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
        3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,4,5,5,6,5,6,6,7,5,6,6,7,6,7,7,8,
        1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
        2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
        2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
        3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,4,5,5,6,5,6,6,7,5,6,6,7,6,7,7,8,
        2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
        3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,4,5,5,6,5,6,6,7,5,6,6,7,6,7,7,8,
        3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,4,5,5,6,5,6,6,7,5,6,6,7,6,7,7,8,
        4,5,5,6,5,6,6,7,5,6,6,7,6,7,7,8,5,6,6,7,6,7,7,8,6,7,7,8,7,8,8,9};
    
    return bcounts[cell];
}

#ifdef EXPLAIN

/**************************************************/
/* Indent two spaces for each level of recursion. */
/**************************************************/
static inline void explain_indent(FILE *h)
{
    int i;
    
    for (i = 0; i < lvl-1; i++) fprintf(h, "  ");
}

/******************************************************************/
/* Construct a string representing the possible values a cell may */
/* contain according to current markup.                           */
/******************************************************************/
static char *clues(short cell)
{
    int i, m, multi, mask;
    static char buf[64], *p;
    
    multi = m = bitcount(cell);
    
    if (!multi) return "NULL";
    
    if (multi > 1) {
        strlcpy(buf, "tuple (",ARRAYSIZE(buf));
    }
    else {
        strlcpy(buf, "value ",ARRAYSIZE(buf));
    }
    
    p = buf + strlen(buf);
    
    for (mask = i = 1; i <= PUZZLE_DIM; i++) {
        if (mask & cell) {
            *p++ = symtab[mask];
            multi -= 1;
            if (multi) { *p++ = ','; *p++ = ' '; }
        }
        mask <<= 1;
    }
    if (m > 1) *p++ = ')';
    *p = 0;
    return buf;
}

/*************************************************************/
/* Explain removal of a candidate value from a changed cell. */
/*************************************************************/
static void explain_markup_elim(grid *g, int chgd, int clue)
{
    int chgd_row, chgd_col, clue_row, clue_col;
    
    chgd_row = map[chgd].row+1;
    chgd_col = map[chgd].col+1;
    clue_row = map[clue].row+1;
    clue_col = map[clue].col+1;
    
    explain_indent(solnfile);
    fprintf(solnfile, "Candidate %s removed from row %d, col %d because of cell at row %d, col %d\n",
            clues(g->cell[clue]), chgd_row, chgd_col, clue_row, clue_col);
}

/*****************************************/
/* Dump the state of the current markup. */
/*****************************************/
static void explain_current_markup(grid *g)
{
    if (g->exposed >= PUZZLE_CELLS) return;
    
    fprintf(solnfile, "\n");
    explain_indent(solnfile);
    fprintf(solnfile, "Current markup is as follows:");
    diagnostic_grid(g, solnfile);
    fprintf(solnfile, "\n");
}

/****************************************/
/* Explain the solving of a given cell. */
/****************************************/
static void explain_solve_cell(grid *g, int chgd)
{
    int chgd_row, chgd_col;
    
    chgd_row = map[chgd].row+1;
    chgd_col = map[chgd].col+1;
    
    explain_indent(solnfile);
    fprintf(solnfile, "Cell at row %d, col %d solved with %s\n",
            chgd_row, chgd_col, clues(g->cell[chgd]));
}

/******************************************************************/
/* Explain the current impasse reached during markup elimination. */
/******************************************************************/
static void explain_markup_impasse(grid *g, int chgd, int clue)
{
    int chgd_row, chgd_col, clue_row, clue_col;
    
    chgd_row = map[chgd].row+1;
    chgd_col = map[chgd].col+1;
    clue_row = map[clue].row+1;
    clue_col = map[clue].col+1;
    
    explain_indent(solnfile);
    fprintf(solnfile, "Impasse for cell at row %d, col %d because cell at row %d, col %d removes last candidate\n",
            chgd_row, chgd_col, clue_row, clue_col);
    explain_current_markup(g);
}

/****************************************/
/* Explain naked and/or hidden singles. */
/****************************************/
static void explain_singleton(grid *g, int chgd, int mask, char *vdesc)
{
    int chgd_row, chgd_col, chgd_reg;
    
    chgd_row = map[chgd].row+1;
    chgd_col = map[chgd].col+1;
    chgd_reg = map[chgd].region+1;
    
    explain_indent(solnfile);
    fprintf(solnfile, "Cell of region %d at row %d, col %d will only solve for %s in this %s\n",
            chgd_reg, chgd_row, chgd_col, clues(mask), vdesc);
    explain_solve_cell(g, chgd);
}

/*********************************/
/* Explain initial puzzle state. */
/*********************************/
static void explain_markup()
{
    fprintf(solnfile, "\n");
    explain_indent(solnfile);
    fprintf(solnfile, "Assume all cells may contain any values in the range: [1 - 9]\n");
}

/************************/
/* Explain given clues. */
/************************/
static void explain_given(int cell, char val)
{
    int cell_row, cell_col;
    
    cell_row = map[cell].row+1;
    cell_col = map[cell].col+1;
    
    explain_indent(solnfile);
    fprintf(solnfile, "Cell at row %d, col %d is given clue value %c\n", cell_row, cell_col, val);
}

/*******************************************/
/* Explain region/row/column interactions. */
/*******************************************/
static void explain_vector_elim(char *desc, int i, int cell, int val, int region)
{
    int cell_row, cell_col;
    
    cell_row = map[cell].row+1;
    cell_col = map[cell].col+1;
    
    explain_indent(solnfile);
    fprintf(solnfile, "Candidate %s removed from cell at row %d, col %d because it aligns along %s %d in region %d\n",
            clues(val), cell_row, cell_col, desc, i+1, region+1);
}

/******************************************************************/
/* Explain the current impasse reached during vector elimination. */
/******************************************************************/
static void explain_vector_impasse(grid *g, char *desc, int i, int cell, int val, int region)
{
    int cell_row, cell_col;
    
    cell_row = map[cell].row+1;
    cell_col = map[cell].col+1;
    
    explain_indent(solnfile);
    fprintf(solnfile, "Impasse at cell at row %d, col %d because candidate %s aligns along %s %d in region %d\n",
            cell_row, cell_col, clues(val), desc, i+1, region+1);
    explain_current_markup(g);
}

/*****************************************************************/
/* Explain the current impasse reached during tuple elimination. */
/*****************************************************************/
static void explain_tuple_impasse(grid *g, char *desc, int elt, int tuple, int count, int bits)
{
    explain_indent(solnfile);
    fprintf(solnfile, "Impasse in %s %d because too many (%d) cells have %d-valued %s\n",
            desc, elt+1, count, bits, clues(tuple));
    explain_current_markup(g);
}

/*********************************************************************/
/* Explain the removal of a tuple of candidate solutions from a cell */
/*********************************************************************/
static void explain_tuple_elim(char *desc, int elt, int tuple, int cell)
{
    explain_indent(solnfile);
    fprintf(solnfile, "Values of %s in %s %d removed from cell at row %d, col %d\n",
            clues(tuple), desc, elt+1, map[cell].row+1, map[cell].col+1);
    
}

/**************************************************/
/* Indicate that a viable solution has been found */
/**************************************************/
static void explain_soln_found(grid *g)
{
    char buf[90];
    
    fprintf(solnfile, "\n");
    explain_indent(solnfile);
    fprintf(solnfile, "Solution found: %s\n", format_answer(g, buf));
    print_grid(buf, solnfile);
    fprintf(solnfile, "\n");
}

/***************************/
/* Show the initial puzzle */
/***************************/
static void explain_grid(grid *g)
{
    char buf[90];
    
    fprintf(solnfile, "Initial puzzle: %s\n", format_answer(g, buf));
    print_grid(buf, solnfile);
    explain_current_markup(g);
    fprintf(solnfile, "\n");
}

/*************************************************/
/* Explain attempt at a trial and error solution */
/*************************************************/
static void explain_trial(int cell, int value)
{
    explain_indent(solnfile);
    fprintf(solnfile, "Attempt trial where cell at row %d, col %d is assigned value %s\n",
            map[cell].row+1, map[cell].col+1, clues(value));
}

/**********************************************/
/* Explain back out of current trial solution */
/**********************************************/
static void explain_backtrack()
{
    if (lvl <= 1) return;
    
    explain_indent(solnfile);
    fprintf(solnfile, "Backtracking\n\n");
}

#define EXPLAIN_MARKUP                                 if (explain) explain_markup()
#define EXPLAIN_CURRENT_MARKUP(g)                      if (explain) explain_current_markup((g))
#define EXPLAIN_GIVEN(cell, val)	               if (explain) explain_given((cell), (val))
#define EXPLAIN_MARKUP_ELIM(g, chgd, clue)             if (explain) explain_markup_elim((g), (chgd), (clue))
#define EXPLAIN_MARKUP_SOLVE(g, cell)                  if (explain) explain_solve_cell((g), (cell))
#define EXPLAIN_MARKUP_IMPASSE(g, chgd, clue)          if (explain) explain_markup_impasse((g), (chgd), (clue))
#define EXPLAIN_SINGLETON(g, chgd, mask, vdesc)        if (explain) explain_singleton((g), (chgd), (mask), (vdesc))
#define EXPLAIN_VECTOR_ELIM(desc, i, cell, v, r)       if (explain) explain_vector_elim((desc), (i), (cell), (v), (r))
#define EXPLAIN_VECTOR_IMPASSE(g, desc, i, cell, v, r) if (explain) explain_vector_impasse((g), (desc), (i), (cell), (v), (r))
#define EXPLAIN_VECTOR_SOLVE(g, cell)                  if (explain) explain_solve_cell((g), (cell))
#define EXPLAIN_TUPLE_IMPASSE(g, desc, j, c, count, i) if (explain) explain_tuple_impasse((g), (desc), (j), (c), (count), (i))
#define EXPLAIN_TUPLE_ELIM(desc, j, c, cell)           if (explain) explain_tuple_elim((desc), (j), (c), (cell))
#define EXPLAIN_TUPLE_SOLVE(g, cell)                   if (explain) explain_solve_cell((g), (cell))
#define EXPLAIN_SOLN_FOUND(g)			       if (explain) explain_soln_found((g));
#define EXPLAIN_GRID(g)			               if (explain) explain_grid((g));
#define EXPLAIN_TRIAL(cell, val)		       if (explain) explain_trial((cell), (val));
#define EXPLAIN_BACKTRACK                              if (explain) explain_backtrack();
#define EXPLAIN_INDENT(h)			       if (explain) explain_indent((h))

#else

#define EXPLAIN_MARKUP
#define EXPLAIN_CURRENT_MARKUP(g)
#define EXPLAIN_GIVEN(cell, val)
#define EXPLAIN_MARKUP_ELIM(g, chgd, clue)
#define EXPLAIN_MARKUP_SOLVE(g, cell)
#define EXPLAIN_MARKUP_IMPASSE(g, chgd, clue)
#define EXPLAIN_SINGLETON(g, chgd, mask, vdesc);
#define EXPLAIN_VECTOR_ELIM(desc, i, cell, v, r)
#define EXPLAIN_VECTOR_IMPASSE(g, desc, i, cell, v, r)
#define EXPLAIN_VECTOR_SOLVE(g, cell)
#define EXPLAIN_TUPLE_IMPASSE(g, desc, j, c, count, i)
#define EXPLAIN_TUPLE_ELIM(desc, j, c, cell)
#define EXPLAIN_TUPLE_SOLVE(g, cell)
#define EXPLAIN_SOLN_FOUND(g)
#define EXPLAIN_GRID(g)
#define EXPLAIN_TRIAL(cell, val)
#define EXPLAIN_BACKTRACK
#define EXPLAIN_INDENT(h)

#endif


/*****************************************************/
/* Initialize a grid to an empty state.              */
/* At the start, all cells can have any value        */
/* so set all 9 lower order bits in each cell.       */
/* In effect, the 9x9 grid now has markup that       */
/* specifies that each cell can assume any value     */
/* of 1 through 9.                                   */
/*****************************************************/

static void init_grid(grid *g)
{
    int i;
    
    for (i = 0; i < PUZZLE_CELLS; i++) g->cell[i] = 0x01ff;
    memset(g->cellflags, 0, PUZZLE_CELLS*sizeof(g->cellflags[0]));
    g->exposed = 0;
    g->givens = 0;
    g->inc = 0;
    g->maxlvl = 0;
    g->score = 0;
    g->solncount = 0;
    g->reward = 1;
    g->next = NULL;
    g->tail = 0;
    EXPLAIN_MARKUP;
}

/*****************************************************/
/* Convert a puzzle from the input format,           */
/* i.e. a string of 81 non-blank characters          */
/* with ASCII digits '1' thru '9' specified          */
/* for the givens, and non-numeric characters        */
/* for the remaining cells. The string, read         */
/* left-to-right fills the 9x9 Sudoku grid           */
/* in left-to-right, top-to-bottom order.            */
/*****************************************************/

static void cvt_to_grid(grid *g, char *game)
{
    int i;
    
    init_grid(g);
    
    for (i = 0; i < PUZZLE_CELLS; i++) {
        if (is_given(game[i])) {
            /* warning -- ASCII charset assumed */
            g->cell[i] = 1 << (game[i] - '1');
            g->cellflags[i] = GIVEN;
            g->givens += 1;
            g->solved[g->exposed++] = i;
            EXPLAIN_GIVEN(i, game[i]);
        }
    }
    EXPLAIN_GRID(g);
}

/****************************************************************/
/* Print the partially solved puzzle and all associated markup  */
/* in 9x9 fashion.                                              */
/****************************************************************/

static void diagnostic_grid(grid *g, FILE *h)
{
    int i, j, flag;
    short c;
    char line1[40], line2[40], line3[40], cbuf1[5], cbuf2[5], cbuf3[5], outbuf[PUZZLE_CELLS+1];
    
    /* Sanity check */
    for (flag = 1, i = 0; flag && i < PUZZLE_CELLS; i++) {
        if (bitcount(g->cell[i]) != 1) {
            flag = 0;
        }
    }
    
    /* Don't need to print grid with diagnostic markup? */
    if (flag) {
        format_answer(g, outbuf);
        print_grid(outbuf, h);
        fflush(h);
        return;
    }
    
    strlcpy(cbuf1, "   |", ARRAYSIZE(cbuf1));
    strlcpy(cbuf2, cbuf1,  ARRAYSIZE(cbuf2));
    strlcpy(cbuf3, cbuf1,  ARRAYSIZE(cbuf3));
    fprintf(h, "\n");
    
    for (i = 0; i < PUZZLE_DIM; i++) {
        
        *line1 = *line2 = *line3 = 0;
        
        for (j = 0; j < PUZZLE_DIM; j++) {
            
            c = g->cell[row[i][j]];
            
            if (bitcount(c) == 1) {
                strlcpy(cbuf1, "   |", ARRAYSIZE(cbuf1));
                strlcpy(cbuf2, cbuf1,  ARRAYSIZE(cbuf2));
                strlcpy(cbuf3, cbuf1,  ARRAYSIZE(cbuf3));
                cbuf2[1] = symtab[c];
            }
            else {
                if (c & 1) cbuf1[0] = '*'; else cbuf1[0] = '.';
                if (c & 2) cbuf1[1] = '*'; else cbuf1[1] = '.';
                if (c & 4) cbuf1[2] = '*'; else cbuf1[2] = '.';
             