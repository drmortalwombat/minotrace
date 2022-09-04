#include <c64/vic.h>
#include <c64/memmap.h>
#include <c64/joystick.h>
#include <c64/asm6502.h>
#include <c64/rasterirq.h>
#include <string.h>
#include <conio.h>
#include <math.h>
#include <fixmath.h>
#include <stdlib.h>

char * const Screen	=	(char *)0xc000;
char * const Screen2=	(char *)0xc400;
char * const Font 	= 	(char *)0xc800;
char * const Color	=	(char *)0xd800; 

static const int gstride = 16;

char bgrid[256 * 16];

#pragma align(bgrid, 256)

static const char smask0[] = {0x30, 0x33, 0xfc, 0xff};
static const char smask1[] = {0x03, 0xcc, 0xcf, 0xff};

RIRQCode	IRQFrame;

void init(void)
{
	mmap_trampoline();

	mmap_set(MMAP_NO_ROM);

	rirq_init(true);

	rirq_build(&IRQFrame, 1);
	rirq_write(&IRQFrame, 0, &vic.memptr, 0x02);

	rirq_set(0, 250, &IRQFrame);

	rirq_sort();
	rirq_start();

	memset(Screen, 32, 1000);
	memset(Color, VCOL_WHITE + 8, 1000);
	
	vic_setmode(VICM_TEXT_MC, Screen, Font);

	vic.color_border = VCOL_GREEN;
	vic.color_back = VCOL_BLACK;
	vic.color_back1 = VCOL_RED;
	vic.color_back2 = VCOL_BLUE;

	char b[2] = {0x33, 0xcc};

	for(char c=0; c<2; c++)
	{
		char	pc = 0x55 << c;

		for(char s=0; s<7; s++)
		{
			char p[2];
			if (s < 4)
			{
				p[0] = pc & smask0[s];
				p[1] = pc & smask1[s];
			}
			else
			{
				p[0] = pc | smask0[s - 4];
				p[1] = pc | smask1[s - 4];
			}

			for(char cy=0; cy<8; cy++)
			{
				for(char t=0; t<8; t++)
				{
					Font[8 * (cy + 0 + 16 * s + 128 * c) + t] = t > 7 - cy ? p[t & 1] : 0x00;
					Font[8 * (cy + 8 + 16 * s + 128 * c) + t] = t <= cy ? p[t & 1] : b[t & 1];
				}
			}
		}
	}

	for(char t=0; t<8; t++)
		Font[8 * 112 + t] = b[t & 1];
}

void done(void)
{
	getch();

	vic_setmode(VICM_TEXT, (char *)0x0400, (char *)0x1000);
}

char scanheight[40];
char scancolor[40], pscancolor[40];
char column[25];

char topChars[8] = {0x20, 0x64, 0x6f, 0x79, 0x62, 0xf8, 0xf7, 0xe3};
char botChars[8] = {0x20, 0x63, 0x77, 0x78, 0xe2, 0xf9, 0xef, 0xe4};

char	sindex;

void drawColumn(char col)
{
	__asm
	    {
	    ldx     col

	    lda     scanheight, x
	    lsr
	    lsr
	    lsr
	    ora     #$e0
	    sta     jp + 2

	    lda		sindex
	    sta 	jp + 1

	    lda     scanheight, x
	    and     #7
	    ora		#8
	    ora		scancolor, x
	    tay

	    lda     scanheight, x
	    lsr
	    and     #7
	    ora		scancolor, x

	jp:
	    jmp     $e000

	    }
}

void buildFastcode(void)
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
		        dp += asm_im(dp, ASM_ORA, 0x0f);

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

	        dp += asm_im(dp, ASM_LDA, 0x70);

	        for(char j=yh + 1; j<25; j++)
	        {
	            unsigned fp = sp + 40 * j;
	            dp += asm_ax(dp, ASM_STA, fp);
		       }

	       	dp += asm_np(dp, ASM_RTS);
	    }
	}
}

void drawScreen(void)
{
    for(char x=0; x<40; x++)
        drawColumn(x);
}

char	blut[136];
char	inverse[8192] = {
	255, 
#assign i 1
#repeat
	(4096 / i > 255) ? 255 : 4096 / i,
#assign i i + 1
#until i == 8192
#undef i
}

#pragma align(inverse, 256)

inline void seg3d(char sx, unsigned w, char c)
{
	if (w > 135)
		w = 135;

    scanheight[sx] = w;
	scancolor[sx] = c;
}

char sqrtabl[256], sqrtabh[256];

#pragma align(sqrtabl, 256)
#pragma align(sqrtabh, 256)

inline unsigned square(char c)
{
	return (unsigned)sqrtabl[c] | ((unsigned)sqrtabh[c] << 8);
}

inline unsigned mul88(char a, char b)
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

inline void dcast(char sx, char ix, char iy, unsigned irx, unsigned iry, signed char dix, signed char diy, unsigned idx, unsigned idy)
{
	const char	*	bp = bgrid + 256 * iy;

	char	udx = idx >> 2, udy = idy >> 2;

	signed char	id = (int)(mul88(irx, udy) - mul88(iry, udx)) >> 8;

	for(;;)
	{
		while (id < 0)
		{
			ix += dix;

			if (bp[ix])
			{
				seg3d(sx, mul88(udx, inverse[irx]) >> 4, bp[ix]);
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
				seg3d(sx, mul88(udy, inverse[iry]) >> 4, 32 + bp[ix]);
				return;
			}

			iry += 256;
			id -= udx;
		}
	}
}

inline void icast(char sx, int ipx, int ipy, int idx, int idy)
{

	char			ix = ipx >> 8, iy = ipy >> 8;

	unsigned		irx = ipx & 255;
	unsigned		iry = ipy & 255;
#if 1
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

#else
	signed char		dix = 1, diy = 1;

	if (idx < 0)
	{
		dix = -1;
		idx = -idx;
	}
	else
		irx ^= 0xff;

	if (idy < 0)
	{
		diy = -1;
		idy = -idy;
	}
	else
		iry ^= 0xff;


	dcast(sx, ix, iy, irx, iry, dix, diy, idx, idy);
#endif
#if 0
	for(;;)
	{
		while (id < 0)
		{
			ix += dix;

			if (bp[ix])
			{
				seg3d(sx, mul88(udx, inverse[irx]) >> 4, bp[ix]);
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
				seg3d(sx, mul88(udy, inverse[iry]) >> 4, 32 + bp[ix]);
				return;
			}

			iry += 256;
			id -= udx;
		}
	}
#endif
}

void fcast(int ipx, int ipy, int idx, int idy, int iddx, int iddy)
{
	for(int i=0; i<40; i++)
	{
		icast(i, ipx, ipy, idx, idy)
		idx += iddx;
		idy += iddy;
	}
}

static int sintab[64], costab[64];
static int dsintab[64], dcostab[64];

bool inside(int ipx, int ipy)
{
	char	ix = ipx >> 8, iy = ipy >> 8;
	return bgrid[iy * 256 + ix] != 0;
}

int main(void)
{
	init();

    buildFastcode();

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

	for(int i=0; i<16; i++)
	{
		bgrid[i * 256 + 0] = 144;
		bgrid[i * 256 + 255] = 144;
	}

	for(int i=0; i<256; i++)
	{
		bgrid[i] = 144;
		bgrid[i + 256 * 15] = 144;
	}

	char	k = 16;
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

	for(int i=0; i<16; i++)
	{
		for(int j=(i & 1); j<256; j+=2)
		{
			if (bgrid[i * 256 + j])
				bgrid[i * 256 + j] += 32;
		}
	}


	float	px = 1.3, py = 3.4;
	int		ipx = px * 256, ipy = py * 256;
	char	w = 0;

	for(int i=0; i<136; i++)
	{
		int	t = 72 / (i + 8);
		if (t > 6) t = 6;

		blut[i] = 16 * (6 - t);
	}
#if 0
	for(unsigned i=1; i<4096; i++)
	{
		unsigned re = 4096 / i;
		if (re > 255)
			re = 255;
		inverse[i] = re;
		vic.color_border++;
	}
#endif
	for(unsigned i=0; i<256; i++)
	{
		unsigned	s = i * i;
		sqrtabl[i] = s & 0xff;
		sqrtabh[i] = s >> 8;
		vic.color_border++;
	}

	vic.color_border = VCOL_GREEN;

#if 0
	int	vx = 0, vy = 0;
	signed char dw = 0;

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

		ipx += (vx + 8) >> 4;
		ipy += (vy + 8) >> 4;

		int	wx = lmul8f8s(vx, co) + lmul8f8s(vy, si);
		int	wy = lmul8f8s(vy, co) - lmul8f8s(vx, si);		

		wx = (wx * 15 + 8) >> 4;
		wy = (wy + 1) >> 1;

		if (joyb[0])
			wx += 128;
		if (wx >= 2048)
			wx = 2048;
		else if (wx < -2048)
			wx = -2048;

		vx = lmul8f8s(wx, co) - lmul8f8s(wy, si);
		vy = lmul8f8s(wy, co) + lmul8f8s(wx, si);

		if (inside(ipx + 0x40, ipy))
		{
			ipx = ((ipx + 0x40) & 0xff00) - 0x40;
			vx = -vx;
		}
		if (inside(ipx - 0x40, ipy))
		{
			ipx = ((ipx - 0x40) & 0xff00) + 0x140;
			vx = -vx;
		}
		if (inside(ipx, ipy + 0x40))
		{
			ipy = ((ipy + 0x40) & 0xff00) - 0x40;
			vy = -vy;
		}
		if (inside(ipx, ipy - 0x40))
		{
			ipy = ((ipy - 0x40) & 0xff00) + 0x140;
			vy = -vy;
		}
	}
#elif 0
	for(int i=0; i<10000; i++)
	{
		int		co = costab[w], si = sintab[w];

		int	idx = co + si, idy = si - co;
		int	iddx = -dsintab[w], iddy = dcostab[w];

		vic.color_border++;
		fcast(ipx, ipy, idx, idy, iddx, iddy);
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
	done();

	return 0;
}