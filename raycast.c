#include <math.h>
#include <fixmath.h>
#include <c64/vic.h>
#include <c64/asm6502.h>

#include "raycast.h"
#include "maze.h"
#include "display.h"

static int sintab[64], costab[64];
static int dsintab[64], dcostab[64];


char	inverse[4096] = {
	255, 
#assign i 1
#repeat
	(4096 / i > 255) ? 255 : 4096 / i,
#assign i i + 1
#until i == 4096
#undef i
};

#pragma align(inverse, 256)

char sqrtabl[256], sqrtabh[256];

#pragma align(sqrtabl, 256)
#pragma align(sqrtabh, 256)

char	blut[136];
char	clut[24] = {
	0x00, 0x00, 0x00, 0x00, // Fake empty slot
	0x40, 0x50, 0x60, 0x50,
	0x70, 0x80, 0x90, 0x80,
	0xa0, 0xb0, 0xc0, 0xb0,
	0xd0, 0xe0, 0xf0, 0xe0,
	0xe0, 0xe0, 0xe0, 0xe0
};

void rcast_init_tables(void)
{
	for(int i=0; i<64; i++)
	{
		float	f = 256 * sin(i * (PI / 32));
		int j = (i + 48) & 63;
		sintab[i] = f;
		costab[j] = f;
		dsintab[i] = f * 0.05;
		dcostab[j] = f * 0.05;
		vic.color_border++;
	}	

	for(unsigned i=0; i<256; i++)
	{
		unsigned	s = i * i;
		sqrtabl[i] = s & 0xff;
		sqrtabh[i] = s >> 8;
		vic.color_border++;
	}

	for(int i=0; i<136; i++)
	{
		int	t = 72 / (i + 8);
		if (t > 6) t = 6;

		blut[i] = 16 * (6 - t);
	}
}

static void drawColumn(char col, char height, char color)
{
	__asm
	    {
	    ldx     col

	    lda     height
	    lsr
	    lsr
	    lsr
	    ora     #$e0
	    sta     jp + 2

	    lda		sindex
	    sta 	jp + 1

	    lda     height
	    and     #7
	    bne		w1
	    ldy		#2
	    bne		w2
	w1:	    
	    ora		color
	    tay
	w2:

	    lda     height
	    lsr
	    and     #7
	    ora		#8
	    ora		color
	jp:
	    jmp     $e000

	    }
}

void rcast_init_fastcode(void)
{
	for(int s=0; s<2; s++)
	{
		unsigned	sp = (unsigned)Screen + 0x0400 * s;

	    for(int i=0; i<=16; i++)
	    {
	        char * dp = (char *)0xe000 + 256 * i + 128 * s;

	        char yl = 8 - (i >> 1);
	        char yh = 8 + i;

	        if (yl > 0)
	        {
	            unsigned fp = sp + 40 * (yl - 1);
	            dp += asm_ax(dp, ASM_STA, fp);
	        }

	        if (yl < yh)
	        {
		        dp += asm_im(dp, ASM_AND, 0xf0);

		        for(char j=yl; j<yh; j++)
		        {
		            unsigned fp = sp + 40 * j;
		            dp += asm_ax(dp, ASM_STA, fp);
		        }
		    }

	        if (yh < 25)
	        {
	        	dp += asm_np(dp, ASM_TYA);
	            unsigned fp = sp + 40 * yh;
	            dp += asm_ax(dp, ASM_STA, fp);
	        }

	        dp += asm_im(dp, ASM_LDA, 0x00);

	        for(char j=0; j<yl - 1; j++)
	        {
	            unsigned fp =sp + 40 * j;
	            dp += asm_ax(dp, ASM_STA, fp);
	        }

	        dp += asm_im(dp, ASM_LDA, 0x03);

	        for(char j=yh + 1; j<25; j++)
	        {
	            unsigned fp = sp + 40 * j;
	            dp += asm_ax(dp, ASM_STA, fp);
		       }

	       	dp += asm_np(dp, ASM_RTS);
	    }
	}
}

static inline unsigned square(char c)
{
	return (unsigned)sqrtabl[c] | ((unsigned)sqrtabh[c] << 8);
}

static inline unsigned mul88(char a, char b)
{
	unsigned	s = a + b;
	if (s >= 256)
	{
		s &= 0xff;
		return ((square(s) - square(a) - square(b)) >> 1) + (s << 8);
	}
	else
		return (square(s) - square(a) - square(b)) >> 1;
}

static inline char colheight(unsigned d, unsigned r)
{
	if (r >= 4096)
		return 0;
	else
	{
		unsigned	h = mul88(d, inverse[r]) >> 4;
		if (h >= 256)
			return 255;
		else
			return h;
	}
}

static char col_h[41], col_x[41], col_y[41], col_d[41];

static inline void dcast(char sx, char ix, char iy, unsigned irx, unsigned iry, signed char dix, signed char diy, unsigned idx, unsigned idy)
{
	const char	*	bp = maze_grid + 256 * iy;

	char	udx = idx >> 2, udy = idy >> 2;

	signed char	id = (int)(mul88(irx, udy) - mul88(iry, udx)) >> 8;

	for(;;)
	{
		while (id < 0)
		{
			ix += dix;

			if (bp[ix])
			{
				col_x[sx] = ix;
				col_y[sx] = (char)((unsigned)bp >> 8);
				col_d[sx] = dix < 0 ? bp[ix] | 0 : bp[ix] | 2;
				col_h[sx] = colheight(udx, irx);
				return;
			}

			irx += 256;
			id += udy;
		}

		while (id >= 0)
		{
			bp += 256 * diy;

			if (bp[ix])
			{
				col_x[sx] = ix;
				col_y[sx] = (char)((unsigned)bp >> 8);
				col_d[sx] = diy < 0 ? bp[ix] | 1 : bp[ix] | 3;
				col_h[sx] = colheight(udy, iry);
				return;
			}

			iry += 256;
			id -= udx;
		}
	}
}

void rcast_draw_screen(void)
{
    for(char x=0; x<40; x++)
    {
    	char w = col_h[x];

   		if (w > 135)
			w = 135;

		drawColumn(x, w, clut[col_d[x]]);
    }
}

inline void icast(char sx, int ipx, int ipy, int idx, int idy)
{

	char			ix = ipx >> 8, iy = ipy >> 8;

	unsigned		irx = ipx & 255;
	unsigned		iry = ipy & 255;

	if (idx < 0)
	{
		if (idy < 0)
		{
			dcast(sx, ix, iy, irx, iry, -1, -1, -idx, -idy);
		}
		else
		{
			dcast(sx, ix, iy, irx, iry ^ 0xff, -1, 1, -idx, idy);
		}
	}
	else
	{
		if (idy < 0)
		{
			dcast(sx, ix, iy, irx ^ 0xff, iry, 1, -1, idx, -idy);
		}
		else
		{
			dcast(sx, ix, iy, irx ^ 0xff, iry ^ 0xff, 1, 1, idx, idy);
		}
	}
}

void rcast_cast_rays(int ipx, int ipy, int idx, int idy, int iddx, int iddy)
{
	clut[20] ^= 0x10;
	clut[21] ^= 0x10;
	clut[22] ^= 0x10;
	clut[23] ^= 0x10;

	icast(0, ipx, ipy, idx, idy);

	for(int i=0; i<39; i+=2)
	{
		icast(i + 2, ipx, ipy, idx + 2 * iddx, idy + 2 * iddy);
		if (col_x[i] == col_x[i + 2] && col_y[i] == col_y[i + 2] && col_d[i] == col_d[i + 2])
		{
			col_x[i + 1] = col_x[i];
			col_y[i + 1] = col_y[i];
			col_d[i + 1] = col_d[i];
			col_h[i + 1] = (col_h[i] + col_h[i + 2]) >> 1;
		}
		else
		{
			icast(i + 1, ipx, ipy, idx + iddx, idy + iddy);			
		}

		idx += 2 * iddx;
		idy += 2 * iddy;
	}

}

