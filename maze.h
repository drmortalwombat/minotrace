#ifndef MAZE_H
#define MAZE_H

#include "gamemusic.h"

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

struct MazeInfo
{
	unsigned	seed;
	char		size;
	char		colors;
	Tune		tune;
	char		time;
};

extern char 	maze_grid[256 * 25];

extern unsigned maze_size;

#pragma align(maze_grid, 256)

inline bool maze_inside(int ipx, int ipy);

inline MazeFields maze_field(int ipx, int ipy);

void maze_preview(void);

void maze_print(void);

void maze_build_1(unsigned size);

void maze_build_5(unsigned size);

void maze_build_minefield(unsigned size);

void maze_build(const MazeInfo * info);

#pragma compile("maze.c")

#endif

