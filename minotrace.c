#include <c64/vic.h>
#include <c64/memmap.h>
#include <c64/joystick.h>
#include <c64/asm6502.h>
#include <c64/rasterirq.h>
#include <c64/sprites.h>
#include <c64/sid.h>
#include <audio/sidfx.h>
#include <oscar.h>
#include <string.h>
#include <conio.h>
#include <math.h>
#include <fixmath.h>
#include <stdlib.h>
#include <stdio.h>
#include "gamemusic.h"
#include "raycast.h"
#include "maze.h"
#include "display.h"

const SIDFX	SIDFXBounce[1] = {{
	1000, 2048, 
	SID_CTRL_GATE | SID_CTRL_NOISE,
	SID_ATK_2 | SID_DKY_6,
	0xf0  | SID_DKY_750,
	-20, 0,
	4, 20,
	1
}};



void maze_print(void)
{
	for(int i=0; i<256; i++)
	{
		for(int j=0; j<24; j++)
		{
			if (bgrid[i + 256 * j] >= 0x10)
				printf("#");
			else
				printf(".");
		}
		printf("\n");
	}
}

int bdir[4] = {1, 256, -1, -256};

bool maze_check(int p, char d)
{
	int p1 = p + bdir[d];
	int p2 = p1 + bdir[d];
	int p3 = p2 + bdir[d];

	int	d0 = bdir[(d + 1) & 3], d1 = bdir[(d + 3) & 3];

	return 
		bgrid[p1] == 0xff &&
		bgrid[p1 + d0] == 0xff &&
		bgrid[p1 + d1] == 0xff &&
		bgrid[p1 + 2 * d0] == 0xff &&
		bgrid[p1 + 2 * d1] == 0xff &&
		bgrid[p2] == 0xff &&
		bgrid[p2 + d0] == 0xff &&
		bgrid[p2 + d1] == 0xff &&
		bgrid[p2 + 2 * d0] == 0xff &&
		bgrid[p2 + 2 * d1] == 0xff &&
		bgrid[p3] == 0xff &&
		bgrid[p3 + d0] == 0xff &&
		bgrid[p3 + d1] == 0xff;
}

bool maze_check_3(int p, char d)
{
	int p1 = p + bdir[d];
	int p2 = p1 + bdir[d];
	int p3 = p2 + bdir[d];

	int	d0 = bdir[(d + 1) & 3], d1 = bdir[(d + 3) & 3];

	return 
		bgrid[p1] == 0xff &&
		bgrid[p1 + d0] == 0xff &&
		bgrid[p1 + d1] == 0xff &&
		bgrid[p2] == 0xff &&
		bgrid[p2 + d0] == 0xff &&
		bgrid[p2 + d1] == 0xff;
}

void maze_build_1(void)
{
	memset(bgrid, 0xff, 24 * 256);

	for(int i=0; i<24; i++)
	{
		bgrid[i * 256 + 0] = 0xfe;
		bgrid[i * 256 + 255] = 0xfe;
	}

	for(int i=0; i<256; i++)
	{
		bgrid[i] = 0xfe;
		bgrid[i + 256 * 23] = 0xfe;
	}

	int	p = 256 * 12 + 1;
	bgrid[p] = 0xfc;

	for(;;)
	{
		char d = rand() & 3;

		char i = 0;
		while (i < 4 && !maze_check(p, d))
		{
			d = (d + 1) & 3;
			i++;
		}

		if (i == 4)
		{
			p -= bdir[bgrid[p]];
			if (bgrid[p] == 0xfc)
			{
				bgrid[p] = 0;

				for(int i=0; i<24; i++)
				{
					for(int j=0; j<256; j++)
					{
						if (bgrid[i * 256 + j] > 0x80)
							bgrid[i * 256 + j] = 16 + 128 * (1 & ((i >> 2) ^ (j >> 2)));
						else
						{
							bgrid[i * 256 + j] = 0;
							bgrid[i * 256 + j - 256] = 0;
							if (j > 1)
							{
								bgrid[i * 256 + j - 1] = 0;
								bgrid[i * 256 + j - 257] = 0;
							}
						}
					}
				}

				return;
			}
		}
		else
		{			
			p += bdir[d];
			bgrid[p] = d;
		}
	}
}

void maze_build_3(void)
{
	memset(bgrid, 0xff, 24 * 256);

	for(int i=0; i<24; i++)
	{
		bgrid[i * 256 + 0] = 0xfe;
		bgrid[i * 256 + 255] = 0xfe;
	}

	for(int i=0; i<256; i++)
	{
		bgrid[i] = 0xfe;
		bgrid[i + 256 * 23] = 0xfe;
	}

	int	p = 256 * 12 + 1;
	bgrid[p] = 0xfc;

	for(;;)
	{
		char d = rand() & 3;

		char i = 0;
		while (i < 4 && !maze_check_3(p, d))
		{
			d = (d + 1) & 3;
			i++;
		}

		if (i == 4)
		{
			p -= bdir[bgrid[p]];
			if (bgrid[p] == 0xfc)
			{
				bgrid[p] = 0;

				for(int i=0; i<24; i++)
				{
					for(int j=0; j<256; j++)
					{
						if (bgrid[i * 256 + j] > 0x80)
							bgrid[i * 256 + j] = 16 + 128 * (1 & ((i >> 2) ^ (j >> 2)));
						else
							bgrid[i * 256 + j] = 0;
					}
				}

				return;
			}
		}
		else
		{			
			p += bdir[d];
			bgrid[p] = d;
		}
	}
}

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

void maze_build_path(const char * kns, char steps, char sel, char wall)
{
	memset(bgrid, 0xa0, 24 * 256);

	for(int i=0; i<24; i++)
	{
		bgrid[i * 256 + 0] = 0x20;
		bgrid[i * 256 + 255] = 0x20;
	}

	int k0 = kns[0];
	bool	t0 = true, t1 = true;

	for(int i=0; i<40; i++)
		rand();

	for(int i=1; i<255; i++)
	{
		char j = i & (steps - 1);

		if (j == sel)
		{
			if (rand() & 1)
			{
				t0 = true; t1 = false;
			}
			else
			{
				t0 = false; t1 = true;;
			}
		}

		int k1 = kns[j];

		int kmin1 = MIN(k0, k1) - 2;
		int kmax1 = MAX(k0, k1) + 2;

		int kmin2 = 23 - kmax1;
		int kmax2 = 23 - kmin1;

		for(int k=1; k<23; k++)
		{
			if ((j < wall || t0) && k > kmin1 && k <= kmax1 || (j < wall || t1) && k >= kmin2 && k < kmax2)
				bgrid[256 * k + i] = 0;
			else
				bgrid[256 * k + i] = 0x10 + 0x80 * (i & 1);
		}

		k0 = k1;
	}
}

void maze_build_2(void)
{
	char	kns[16];

	for(int i=0; i<16; i++)
		kns[i] = 7 + 5 * sin((i + 3) * (PI / 8));

	maze_build_path(kns, 16, 5, 13);
}

void maze_build_4(void)
{
	char	kns[32];

	for(int i=0; i<32; i++)
		kns[i] = 7 + 5 * sin((i + 3) * (PI / 16));

	maze_build_path(kns, 32, 5, 26);
}

void maze_build_5(void)
{
	char	kns[16];

	for(int i=0; i<16; i++)
		kns[i] = 12 - 10 * sin(i * (PI / 16));

	maze_build_path(kns, 16, 0, 0);
}


void maze_build_6(void)
{
	memset(bgrid, 0xa0, 24 * 256);

	for(int i=0; i<24; i++)
	{
		bgrid[i * 256 + 0] = 0x20;
		bgrid[i * 256 + 255] = 0x20;
	}

	signed char	x = 6;
	for(int i=1; i<255; i+=3)
	{

		for(int k=1; k<23; k++)
		{
			if (k < x + 5 || k >= x + 7)
			{
				bgrid[256 * k + i + 0] = 0x10 + 0x80 * (k & 1);
			}
			else
			{
				bgrid[256 * k + i + 0] = 0;
			}

			if (k < x || k >= x + 12)
			{
				bgrid[256 * k + i + 1] = 0x10 + 0x80 * (k & 1);
				bgrid[256 * k + i + 2] = 0x10 + 0x80 * (k & 1);
			}
			else
			{
				bgrid[256 * k + i + 1] = 0;
				bgrid[256 * k + i + 2] = 0;
			}

		}

		x = x - 5 + rand() % 10;
		if (x < 1) x = 6;
		if (x > 12) x = 7;
	}
}


void view_scroll_left(void)
{
	for(char i=0; i<24; i++)
	{
		#pragma unroll(full)
		for(char j=0; j<39; j++)
			Screen[40 * i + j] = Screen[40 * i + j + 1];

	}
}

void maze_preview(void)
{
	vic.ctrl2 = VIC_CTRL2_MCM;
	for(int i=0; i<256; i++)
	{
		vic_waitBottom();
		vic.ctrl2 = VIC_CTRL2_MCM + 4;
		view_scroll_left();

		#pragma unroll(full)
		for(char j=0; j<24; j++)
		{
			char p = bgrid[256 * j + i];
			if (p)
				p += 0x27;
			Screen[40 * j + 39] = p;
		}

		vic_waitBottom();
		vic.ctrl2 = VIC_CTRL2_MCM;
		vic_waitTop();
	}

	for(int i=0; i<40; i++)
	{
		vic_waitBottom();
		vic.ctrl2 = VIC_CTRL2_MCM + 4;
		view_scroll_left();

		#pragma unroll(full)
		for(char j=0; j<24; j++)
			Screen[40 * j + 39] = 0;

		vic_waitBottom();
		vic.ctrl2 = VIC_CTRL2_MCM;
		vic_waitTop();
	}
}

int main(void)
{
#if 0
	maze_build_6();
	maze_print();
	return 0;
#else
	display_init();

    rcast_init_tables();
    rcast_init_fastcode();

	maze_build_5();
	
	for(int i=0; i<24; i++)
	{
		bgrid[i * 256 + 0] = 144;
		bgrid[i * 256 + 255] = 144;
	}

	for(int i=0; i<256; i++)
	{
		bgrid[i] = 144;
		bgrid[i + 256 * 23] = 144;
	}

	maze_preview();

	char	k = 16;
#if 0
	for(int i=5; i<256; i+=5)
	{
		int j0 = rand() % 12 + 1;

		for(int j=1; j<j0; j++)
			bgrid[i + j * 256] = k;
		for(int j=j0+3; j<15; j++)
			bgrid[i + j * 256] = k;
		if (j0 > 1)
		{
			bgrid[i - 1 + (j0 - 1) * 256] = k;
			bgrid[i + 1 + (j0 - 1) * 256] = k;
		}
		if (j0 + 3 < 15)
		{
			bgrid[i - 1 + (j0 + 3) * 256] = k;
			bgrid[i + 1 + (j0 + 3) * 256] = k;
		}

		k ^= 128;

	}

#endif

#if 0
	for(int i=0; i<16; i++)
	{
		for(int j=(i & 1); j<256; j+=2)
		{
			if (bgrid[i * 256 + j])
				bgrid[i * 256 + j] += 32;
		}
	}
#endif

	float	py = 12.5, px = 1.5;
	int		ipx = px * 256, ipy = py * 256;
	char	w = 0;

//	vic.color_border = VCOL_GREEN;

	music_patch_voice3(false);

#if 1
	int	vx = 0, vy = 0;
	signed char dw = 0;

	for(;;)
	{
		for(char i=0; i<8; i++)
			bgrid[8 * 256 + i * 256 + 255] ^= 0x80;

		int		co = costab[w], si = sintab[w];

		int	idx = co + si, idy = si - co;
		int	iddx = -dsintab[w], iddy = dcostab[w];

		rcast_cast_rays(ipx, ipy, idx, idy, iddx, iddy);

		sindex ^= 0x80;
		rcast_draw_screen();
		display_flip();

		time_draw();
		compass_draw(w);

		joy_poll(0);

		if (joyx[0])
		{
			if (dw == 0)
				dw = 4 * joyx[0];
			else
			{
				dw += joyx[0];
				if (dw > 8)
					dw = 8;
				else if (dw < -8)
					dw = -8;
			}

			w = (w + ((dw + 2) >> 2)) & 63;	
		}
		else
			dw = 0;

		int	wx = lmul8f8s(vx, co) + lmul8f8s(vy, si);
		int	wy = lmul8f8s(vy, co) - lmul8f8s(vx, si);		

		wx = (wx * 15 + 8) >> 4;
		wy = (wy + 1) >> 1;

		if (joyb[0])
			wx += 128;
		else if (joyy[0] > 0)
			wx -= 32;

		if (wx >= 2048)
			wx = 2048;
		else if (wx < -2048)
			wx = -2048;

		vx = lmul8f8s(wx, co) - lmul8f8s(wy, si);
		vy = lmul8f8s(wy, co) + lmul8f8s(wx, si);

		ipx += (vx + 8) >> 4;
		ipy += (vy + 8) >> 4;

		static const int wdist = 0x40;
		static const int bspeed = 0x100;

		bool	bounce = false;

		if (vx > 0 && maze_inside(ipx + wdist, ipy))
		{
			ipx = ((ipx + wdist) & 0xff00) - wdist;
			if (vx > bspeed)
				bounce = true;
			vx = -(vx >> 1);
		}
		else if (vx < 0 && maze_inside(ipx - wdist, ipy))
		{
			ipx = ((ipx - wdist) & 0xff00) + (0x100 + wdist);
			if (vx < -bspeed)
				bounce = true;
			vx = -vx >> 1;
		}

		if (vy > 0 && maze_inside(ipx, ipy + wdist))
		{
			ipy = ((ipy + wdist) & 0xff00) - wdist;
			if (vy > bspeed)
				bounce = true;
			vy = -(vy >> 1);
		}
		else if (vy < 0 && maze_inside(ipx, ipy - wdist))
		{
			ipy = ((ipy - wdist) & 0xff00) + (0x100 + wdist);
			if (vy < -bspeed)
				bounce = true;
			vy = -vy >> 1;
		}

		if (bounce)
		{
			sidfx_play(2, SIDFXBounce, 1);
		}
	}
#elif 1
	for(int i=0; i<10000; i++)
	{
		int		co = costab[w], si = sintab[w];

		int	idx = co + si, idy = si - co;
		int	iddx = -dsintab[w], iddy = dcostab[w];

		vic.color_border++;
		rfcast(ipx, ipy, idx, idy, iddx, iddy);

//		fcast(ipx, ipy, idx, idy, iddx, iddy);
		vic.color_border--;

		sindex ^= 0x80;
		drawScreen();
		rirq_data(&IRQFrame, 0, (sindex >> 3) | 2);

		joy_poll(0);
		w = (w + 1) & 63;
	}
#else
	for(;;)
	{
		int		co = costab[w], si = sintab[w];

		int	idx = co + si, idy = si - co;
		int	iddx = -dsintab[w], iddy = dcostab[w];

		fcast(ipx, ipy, idx, idy, iddx, iddy);

		sindex ^= 0x80;
		drawScreen();
		rirq_data(&IRQFrame, 0, (sindex >> 3) | 2);

		joy_poll(0);
		w = (w + joyx[0]) & 63;
		ipx -= (co * joyy[0] + 2) >> 2;
		ipy -= (si * joyy[0] + 2) >> 2;

		if (inside(ipx + 0x40, ipy))
			ipx = ((ipx + 0x40) & 0xff00) - 0x40;
		if (inside(ipx - 0x40, ipy))
			ipx = ((ipx - 0x40) & 0xff00) + 0x140;
		if (inside(ipx, ipy + 0x40))
			ipy = ((ipy + 0x40) & 0xff00) - 0x40;
		if (inside(ipx, ipy - 0x40))
			ipy = ((ipy - 0x40) & 0xff00) + 0x140;
	}
#endif
//	done();
#endif
	return 0;
}