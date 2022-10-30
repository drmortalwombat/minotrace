#ifndef RAYCAST_H
#define RAYCAST_H

extern char	sindex;

extern const int sintab[64], costab[64];
extern const int dsintab[64], dcostab[64];

// Init tables for raycaster
void rcast_init_tables(void);

// Init fast code for raycaster column draw
void rcast_init_fastcode(void);

// Draw the screen after completing the ray cast
void rcast_draw_screen(void);

// Ray cast from position with the given two vectors for forward and right
void rcast_cast_rays(int ipx, int ipy, int idx, int idy, int iddx, int iddy);


#pragma compile("raycast.c")

#endif
