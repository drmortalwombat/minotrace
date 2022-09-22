#ifndef MAZE_H
#define MAZE_H

extern char 	maze_grid[256 * 24];

extern unsigned maze_size;

#pragma align(maze_grid, 256)

inline bool maze_inside(int ipx, int ipy);

void maze_preview(void);

void maze_build_1(unsigned size);

void maze_build_5(unsigned size);

#pragma compile("maze.c")

#endif

