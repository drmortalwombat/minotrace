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
		display_put_bigtext(0, 4, s"ready");

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