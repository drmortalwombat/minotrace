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

char * const Screen	=	(char *)0xc000;
char * const Screen2=	(char *)0xc400;
char * const Font 	= 	(char *)0xc800;
char * const Color	=	(char *)0xd800; 
char * const Sprites =  (char *)0xd000;

const char SpriteData[] = {
	#embed spd_sprites lzo "minotrace.spd"
};

char bgrid[256 * 24];

#pragma align(bgrid, 256)

static const char smask0[] = {0x30, 0x33, 0xfc, 0xff};
static const char smask1[] = {0x03, 0xcc, 0xcf, 0xff};

RIRQCode	IRQFrame;

__interrupt void irq_service(void);


void init(void)
{
	mmap_trampoline();

	mmap_set(MMAP_RAM);

	oscar_expand_lzo(Sprites, SpriteData);

	mmap_set(MMAP_NO_ROM);

	music_init(TUNE_GAME);

	rirq_init(true);

	rirq_build(&IRQFrame, 2);
	rirq_write(&IRQFrame, 0, &vic.memptr, 0x02);
	rirq_call(&IRQFrame, 1, irq_service);

	rirq_set(0, 250, &IRQFrame);

	rirq_sort();
	rirq_start();

	memset(Screen, 32, 1000);
	memset(Color, VCOL_WHITE + 8, 1000);
	
	vic_setmode(VICM_TEXT_MC, Screen, Font);

	vic.color_border = VCOL_BLACK;
	vic.color_back = VCOL_BLACK;
	vic.color_back1 = VCOL_RED;
	vic.color_back2 = VCOL_BLUE;

	vic.spr_mcolor0 = VCOL_DARK_GREY;
	vic.spr_mcolor1 = VCOL_WHITE;

	spr_init(Screen);

	for(int i=0; i<8; i++)
	{
		spr_set(i, true, 80 + 24 * i, 50, 64 + 12, VCOL_YELLOW, true, false, false);
	}

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

	sidfx_init();

	sid.fmodevol = 15;
}

void done(void)
{
	getch();

	vic_setmode(VICM_TEXT, (char *)0x0400, (char *)0x1000);
}

char	sindex;

signed char	time[6] = {0, 3, 0, 0, 0, 0};

void time_dec(void)
{
	if (--time[5] >= 0)
		return;
	time[5] = 9;

	if (--time[4] >= 0)
		return;
	time[4] = 4;

	if (--time[3] >= 0)
		return;
	time[3] = 9;

	if (--time[2] >= 0)
		return;
	time[2] = 5;

	if (--time[1] >= 0)
		return;
	time[1] = 9;

	if (--time[0] >= 0)
		return;
	time[0] = 5;
}

void compass_draw(char w)
{
	char	*	sp = Screen + 0x3f8 + 8 * sindex;

	sp[0] = 80 + (((w + 4) >> 3) & 7);
}

void time_draw(void)
{
	char	*	sp = Screen + 0x3f8 + 8 * sindex;

	sp[1] = 64 + time[1];
	sp[2] = 64 + 10;
	sp[3] = 64 + time[2];
	sp[4] = 64 + time[3];
	sp[5] = 64 + 11;
	sp[6] = 64 + time[4];
	sp[7] = 64 + time[5];
}

__interrupt void irq_service(void)
{
	time_dec();
	sidfx_loop_2();
	music_play();
}

const SIDFX	SIDFXBounce[1] = {{
	1000, 2048, 
	SID_CTRL_GATE | SID_CTRL_NOISE,
	SID_ATK_2 | SID_DKY_6,
	0xf0  | SID_DKY_750,
	-20, 0,
	4, 20,
	1
}};



void drawColumn(char col, char height, char color)
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
	    ora		#8
	    ora		color
	    tay

	    lda     height
	    lsr
	    and     #7
	    ora		color

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

char	blut[136];
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

inline char colheight(unsigned d, unsigned r)
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

char col_h[41], col_x[41], col_y[41], col_d[41];

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
				col_x[sx] = ix;
				col_y[sx] = (char)((unsigned)bp >> 8);
				col_d[sx] = dix < 0 ? bp[ix] + 16 : bp[ix] + 32;
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
				col_d[sx] = diy < 0 ? bp[ix] + 0 : bp[ix] + 48;
				col_h[sx] = colheight(udy, iry);
				return;
			}

			iry += 256;
			id -= udx;
		}
	}
}

void drawScreen(void)
{
    for(char x=0; x<40; x++)
    {
    	char w = col_h[x];

   		if (w > 135)
			w = 135;

		drawColumn(x, w, col_d[x]);
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

void fcast(int ipx, int ipy, int idx, int idy, int iddx, int iddy)
{
	for(int i=0; i<40; i++)
	{
		icast(i, ipx, ipy, idx, idy);
		idx += iddx;
		idy += iddy;
	}
}

void rfcast(int ipx, int ipy, int idx, int idy, int iddx, int iddy)
{
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

static int sintab[64], costab[64];
static int dsintab[64], dcostab[64];

bool inside(int ipx, int ipy)
{
	char	ix = ipx >> 8, iy = ipy >> 8;
	return bgrid[iy * 256 + ix] != 0;
}

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

	for(int i=0; i<136; i++)
	{
		int	t = 72 / (i + 8);
		if (t > 6) t = 6;

		blut[i] = 16 * (6 - t);
	}

	for(unsigned i=0; i<256; i++)
	{
		unsigned	s = i * i;
		sqrtabl[i] = s & 0xff;
		sqrtabh[i] = s >> 8;
		vic.color_border++;
	}

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

		fcast(ipx, ipy, idx, idy, iddx, iddy);

		sindex ^= 0x80;
		drawScreen();
		rirq_data(&IRQFrame, 0, (sindex >> 3) | 2);

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

		if (vx > 0 && inside(ipx + wdist, ipy))
		{
			ipx = ((ipx + wdist) & 0xff00) - wdist;
			if (vx > bspeed)
				bounce = true;
			vx = -(vx >> 1);
		}
		else if (vx < 0 && inside(ipx - wdist, ipy))
		{
			ipx = ((ipx - wdist) & 0xff00) + (0x100 + wdist);
			if (vx < -bspeed)
				bounce = true;
			vx = -vx >> 1;
		}

		if (vy > 0 && inside(ipx, ipy + wdist))
		{
			ipy = ((ipy + wdist) & 0xff00) - wdist;
			if (vy > bspeed)
				bounce = true;
			vy = -(vy >> 1);
		}
		else if (vy < 0 && inside(ipx, ipy - wdist))
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
	done();
#endif
	return 0;
}