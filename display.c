#include "display.h"
#include "gamemusic.h"
#include <c64/vic.h>
#include <c64/memmap.h>
#include <c64/rasterirq.h>
#include <c64/sprites.h>
#include <c64/sid.h>

const char SpriteData[] = {
	#embed spd_sprites lzo "minotrace.spd"
};

static const char smask0[] = {0x30, 0x33, 0xfc, 0xff};
static const char smask1[] = {0x03, 0xcc, 0xcf, 0xff};

RIRQCode	IRQFrame;

__interrupt void irq_service(void);


void display_init(void)
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
	{
		Font[8 * 112 + t] = b[t & 1];
		Font[8 * 113 + t] = 255;
	}

	sidfx_init();

	sid.fmodevol = 15;
}


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

void display_flip(void)
{
	rirq_data(&IRQFrame, 0, (sindex >> 3) | 2);
}

void display_put_bigtext(char x, char y, const char * text)
{
	mmap_set(MMAP_CHAR_ROM);

	__assume(y < 25);
	
	char * ldp = Screen + (sindex << 3) + 40 * y + x;

	char c;
	while (c = *text++)
	{
		const char * sp = (char *)0xd000 + 8 * c;

		char * dp = ldp;
		for(char iy=0; iy<8; iy++)
		{
			char b = sp[iy];
			for(char ix=0; ix<8; ix++)
			{
				if (b & 0x80)
					dp[ix] = 113;
				b <<= 1;
			}
			dp += 40;
		}
		ldp += 8;
	}

	mmap_set(MMAP_NO_ROM);
}

void display_scroll_left(void)
{
	for(char i=0; i<24; i++)
	{
		#pragma unroll(full)
		for(char j=0; j<39; j++)
			Screen[40 * i + j] = Screen[40 * i + j + 1];

	}
}
