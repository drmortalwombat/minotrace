#ifndef RAYCAST_H
#define RAYCAST_H

extern char	sindex;

extern const int sintab[64], costab[64];
extern const int dsintab[64], dcostab[64];


void rcast_init_tables(void);

void rcast_init_fastcode(void);

void rcast_draw_screen(void);

void rcast_cast_rays(int ipx, int ipy, int idx, int idy, int iddx, int iddy);


#pragma compile("raycast.c")

#endif
