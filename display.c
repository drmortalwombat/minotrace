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

const char WallFontData[] = {
	#embed ctm_chars "walls.ctm"
};

static const char smask0[] = {0x30, 0x33, 0xfc, 0xff};
static const char smask1[] = {0x03, 0xcc, 0xcf, 0xff};



bool time_running;
signed char	time_digits[5];

RIRQCode	IRQFrame;

__interrupt void irq_service(void);

static const char b[2] = {0xcc, 0x33};


void display_font_expand(void)
{
	memcpy(Font, WallFontData, 32 * 8);

	for(char c=0; c<12; c++)
	{
		const char * sp = WallFontData + 16 * 8 + 8 * c;

		for(char cy=0; cy<8; cy++)
		{
			char * fp = Font + 8 * (cy + 16 * c + 64);

			for(char t=0; t<8; t++)
			{

				if (t > 7 - cy)
					fp[t + 64] = sp[t];
				else if (t == 7 - cy)
					fp[t + 64] = 0xff;
				else
					fp[t + 64] = 0x00;
			}

			for(char t=0; t<8; t++)
			{
				if (cy == 0)
					fp[t] = sp[t];
				else if (t < cy)
					fp[t] = sp[t];
				else if (t == cy)
					fp[t] = 0x00;
				else
					fp[t] = b[t & 1];
			}
		}
	}

}

void display_init(void)
{
	mmap_trampoline();

	mmap_set(MMAP_RAM);

	oscar_expand_lzo(Sprites, SpriteData);

	mmap_set(MMAP_NO_ROM);

	music_init(TUNE_GAME_2);

	rirq_init(true);

	rirq_build(&IRQFrame, 2);
	rirq_write(&IRQFrame, 0, &vic.memptr, 0x02);
	rirq_call(&IRQFrame, 1, irq_service);

	rirq_set(0, 250, &IRQFrame);

	rirq_sort();
	rirq_start();

	memset(Screen, 0, 1000);
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

	display_font_expand();

	sidfx_init();

	sid.fmodevol = 15;
}


void time_dec(void)
{
	if (time_count > 0)
	{
		time_count--;

		if (--time_digits[4] >= 0)
			return;
		time_digits[4] = 9;

		if (--time_digits[3] >= 0)
			return;
		time_digits[3] = 4;

		if (--time_digits[2] >= 0)
			return;
		time_digits[2] = 9;

		if (--time_digits[1] >= 0)
			return;
		time_digits[1] = 5;

		if (--time_digits[0] >= 0)
			return;
		time_digits[0] = 9;
	}
}

void compass_draw(char w)
{
	char	*	sp = Screen + 0x3f8 + 8 * sindex;

	sp[0] = 80 + (((w + 4) >> 3) & 7);
}

void time_draw(void)
{
	char	*	sp = Screen + 0x3f8 + 8 * sindex;

	sp[1] = 64 + time_digits[0];
	sp[2] = 64 + 10;
	sp[3] = 64 + time_digits[1];
	sp[4] = 64 + time_digits[2];
	sp[5] = 64 + 11;
	sp[6] = 64 + time_digits[3];
	sp[7] = 64 + time_digits[4];
}

void time_init(unsigned seconds)
{
	time_count = seconds * 50;

	time_digits[3] = time_digits[4] = 0;
	time_digits[2] = seconds % 10; seconds /= 10;
	time_digits[1] = seconds %  6; seconds /= 6;
	time_digits[0] = seconds;
}

__interrupt void irq_service(void)
{
	if (time_running)
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
					dp[ix] = 4;
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
	for(char j=0; j<39; j++)
	{
		#pragma unroll(full)
		for(char i=0; i<12; i++)
			Screen[40 * i + j] = Screen[40 * i + j + 1];
	}
	for(char j=0; j<39; j++)
	{
		#pragma unroll(full)
		for(char i=12; i<24; i++)
			Screen[40 * i + j] = Screen[40 * i + j + 1];
	}
}
