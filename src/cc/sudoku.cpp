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
/* recursion unwinds so that the next trial may be attempted. It is in