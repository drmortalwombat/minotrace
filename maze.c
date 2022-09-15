#include "maze.h"

char 	bgrid[256 * 24];


bool maze_inside(int ipx, int ipy)
{
	char	ix = ipx >> 8, iy = ipy >> 8;
	return bgrid[iy * 256 + ix] != 0;
}

