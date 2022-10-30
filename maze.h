#ifndef MAZE_H
#define MAZE_H

#include "gamemusic.h"

enum MazeGenerator
{
	MGEN_LABYRINTH_1,
	MGEN_LABYRINTH_3,
	MGEN_DOORS,
	MGEN_MINEFIELD,
	MGEN_GATES,
	MGEN_CURVES_1,
	MGEN_CURVES_2,
	MGEN_CROSS
};

enum MazeFields
{
	MF_EMPTY = 0,
	MF_EXIT = 4,
	MF_MINE = 8,
	MF_DUMMY = 12,

	MF_RED = 16,
	MF_BLUE = 20,
	MF_PURPLE = 24,
	MF_WHITE = 28
};

// Specification of a level
struct MazeInfo
{
	MazeGenerator	gen;		// Generator to use
	unsigned		seed;		// Static seed for random generator
	char			size;		// Size of the maze
	char			colors;		// Back- and Frontcolor
	Tune			tune;		// Tune to play
	char			time;		// Countdown
};

// Data table for the maze 25 rows of 256 fields
extern char 	maze_grid[256 * 25];

// Size of the maze in columns
extern unsigned maze_size;

// Explicitly align maze data on page boundary
#pragma align(maze_grid, 256)

// Check if coordinates point to a wall
inline bool maze_inside(int ipx, int ipy);

// Change a block in the maze
inline void maze_set(int ipx, int ipy, MazeFields f);

// Get the type of a block in the maze
inline MazeFields maze_field(int ipx, int ipy);

// Allow the player to preview the maze
void maze_preview(void);

// Debug print the maze to the console
void maze_print(void);

// Build a maze from the given specification
void maze_build(const MazeInfo * info);

#pragma compile("maze.c")

#endif

