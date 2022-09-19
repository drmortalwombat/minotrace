#ifndef MAZE_H
#define MAZE_H

extern char bgrid[256 * 24];

#pragma align(bgrid, 256)

inline bool maze_inside(int ipx, int ipy);

void maze_preview(void);

void maze_build_5(void);

#pragma compile("maze.c")

#endif

