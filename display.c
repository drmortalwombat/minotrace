#include "display.h"
#include "gamemusic.h"
#include <c64/vic.h>
#include <c64/memmap.h>
#include <c64/rasterirq.h>
#include <c64/sprites.h>
#include <c64/sid.h>
#include <c64/cia.h>

#pragma data(sprites)
const char SpriteData[] = {
	#embed spd_sprites lzo "minotrace.spd"
};
#pragma data(data)

const char WallFontData[] = {
	#embed ctm_chars "walls.ctm"
};

bool time_running;
signed char	time_digits[5];

RIRQCode	IRQFrame;

__interrupt void irq_service(void);

static const char b[2] = {0xcc, 0x33};


const char DataTitleBits[] = {
	#embed 8000 0 lzo "titleimg.bin"
};

const char DataTitleColor0[] = {
	#embed 1000 8000 lzo "titleimg.bin"
};

const char DataTitleColor1[] = {
	#embed 1000 9000 lzo "titleimg.bin"
};

void display_font_expand(void)
{
	memcpy(Font, WallFontData, 32 * 8);

	for(char c=0; c<13; c++)
	{
		const char * sp = WallFontData + 16 * 8 + 8 * c;

		for(char cy=0; cy<8; cy++)
		{
			char * fp = Font + 8 * (cy + 16 * c + 0x30);

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
	cia_init();

	mmap_set(MMAP_ROM);

	rirq_init_memmap();

	vic.ctrl1 = VIC_CTRL1_RSEL + 3;
	vic_waitBottom();

	mmap_set(MMAP_RAM);

	oscar_expand_lzo(Sprites, SpriteData);

	mmap_set(MMAP_NO_ROM);

	music_init(TUNE_GAME_2);
	sidfx_init();
	sid.fmodevol = 15;	

	rirq_build(&IRQFrame, 2);
	rirq_write(&IRQFrame, 0, &vic.memptr, 0x06);
	rirq_call(&IRQFrame, 1, irq_service);

	rirq_set(0, 250, &IRQFrame);

	rirq_sort();
	rirq_start();

	mmap_set(MMAP_RAM);

	display_font_expand();

	mmap_set(MMAP_NO_ROM);

	spr_init(Screen);
}

char title_char_buffer[64];

void title_char_expand(char c)
{
	mmap_set(MMAP_CHAR_ROM);

	const char * sp = (char *)0xd000 + 8 * c;

	for(char iy=0; iy<8; iy++)
	{
		char b = sp[iy];
		for(char ix=0; ix<8; ix++)
		{
			unsigned	u = (title_char_buffer[40 + ix] << 8) | b;
			u <<= 1;
			title_char_buffer[40 + ix] = u >> 8;
			b = u & 0xff;
		}
	}

	mmap_set(MMAP_NO_ROM);
}

const char ctitlelut[16] = {
	VCOL_BLACK,
	VCOL_WHITE,
	VCOL_BLUE,
	VCOL_CYAN,
	VCOL_PURPLE,
	VCOL_GREEN,
	VCOL_BLUE,
	VCOL_LT_BLUE,

	VCOL_PURPLE,
	VCOL_PURPLE,
	VCOL_LT_BLUE,
	VCOL_DARK_GREY,
	VCOL_MED_GREY,
	VCOL_LT_GREEN,
	VCOL_LT_BLUE,
	VCOL_LT_GREY
};

char titlelut[256];

#pragma align(titlelut, 256);

void title_line_expand(void)
{	
	char * sdp = Screen + 16 * 40, * cdp = Color + 16 * 40;
	char * ssp = Screen2, * csp = Screen2 + 512;

	for(char ix=0; ix<40; ix++)
	{
		unsigned m = title_char_buffer[ix];
		#pragma unroll(full)
		for(char iy=0; iy<8; iy++)
		{
			m <<= 1;
			bool	b = (m & 0x0100) != 0;
			char	sc = ssp[40 * iy + ix], cc = csp[40 * iy + ix];

			sc = b ? titlelut[sc] : sc;
			cc = b ? titlelut[cc] : cc;

			sdp[40 * iy + ix] = sc;
			cdp[40 * iy + ix] = cc;
		}
	}

	for(char ix=0; ix<48; ix++)
		title_char_buffer[ix] = title_char_buffer[ix + 1];
}

const char  Text[] =
	S"RACE AGAINST THE CLOCK TO ESCAPE FROM THE EVIL DUNGEONS --- "
	S"DESIGN AND CODING BY DR.MORTAL WOMBAT, MUSIC BY CRISPS --- "
	S"VISIT US ON ITCH.IO FOR MORE --- "
	S"GREETZ TO ECTE, SEPA, STEPZ AND PIGEON64 --- ";

void display_title(void)
{
	vic_waitBottom();
	vic.ctrl1 = VIC_CTRL1_RSEL + 3;
	vic_waitTop();
	vic_waitBottom();

	oscar_expand_lzo(Hires, DataTitleBits);

	oscar_expand_lzo(Screen, DataTitleColor0);
	oscar_expand_lzo(Color, DataTitleColor1);

	vic.color_border = VCOL_BLACK;
	vic.color_back = VCOL_BLACK;

	vic_setmode(VICM_HIRES_MC, Screen, Hires);
	rirq_data(&IRQFrame, 0, 0x28);

	memcpy(Screen2, Screen + 16 * 40, 8 * 40);
	memcpy(Screen2 + 512, Color + 16 * 40, 8 * 40);	

	for(int i=0; i<256; i++)
	{
		titlelut[i] = ctitlelut[i & 0x0f] | (ctitlelut[i >> 4] << 4);
	}

	char x = 0, lx = 0;
	do {
		vic_waitTop();
		vic_waitBottom();
		vic.color_border++;
		title_line_expand();
		vic.color_border++;
		lx ++;
		if (lx == 8)
		{
			title_char_expand(Text[x]);
			x++;
			lx = 0;
			if (!Text[x])
				x = 0;
		}
		vic.color_border--;
		vic.color_border--;

		joy_poll(0);
	} while (!joyb[0]);
}

void display_game(void)
{
	vic_waitBottom();
	vic.ctrl1 = VIC_CTRL1_RSEL + 3;
	vic_waitTop();
	vic_waitBottom();

	memset(Screen, 0, 1000);
	memset(Color, VCOL_WHITE + 8, 1000);

	vic_waitBottom();

	vic_setmode(VICM_TEXT_MC, Screen, Font);
	rirq_data(&IRQFrame, 0, 0x26);

	vic.color_border = VCOL_BLACK;
	vic.color_back = VCOL_BLACK;
	vic.color_back1 = VCOL_YELLOW;
	vic.color_back2 = VCOL_LT_BLUE;

	vic.spr_mcolor0 = VCOL_DARK_GREY;
	vic.spr_mcolor1 = VCOL_WHITE;

	for(int i=0; i<8; i++)
		spr_set(i, true, 80 + 24 * i, 50, 64 + 12, VCOL_YELLOW, true, false, false);
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

static inline char * sprimg_base(void)
{
	return Screen + (sindex ? 0x0400 : 0x000) + 0x3f8;
}

void compass_draw(char w)
{
	char	*	sp = sprimg_base();

	sp[0] = 80 + (((w + 4) >> 3) & 7);
}

void time_draw(void)
{
	char	*	sp = sprimg_base();;

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
	rirq_data(&IRQFrame, 0, (sindex >> 3) | 0x26);
}

void display_reset(void)
{
	sindex = 0;
	rirq_data(&IRQFrame, 0, 0x26);	
	memset(Screen, 0, 1000);
}


void display_put_bigtext(char x, char y, const char * text, BlockChar bc)
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
					dp[ix] = bc;
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
		for(char i=12; i<25; i++)
			Screen[40 * i + j] = Screen[40 * i + j + 1];
	}
}

void display_scroll_right(void)
{
	for(signed char j=38; j>=0; j--)
	{
		#pragma unroll(full)
		for(char i=0; i<12; i++)
			Screen[40 * i + j + 1] = Screen[40 * i + j];
	}
	for(signed char j=38; j>=0; j--)
	{
		#pragma unroll(full)
		for(char i=12; i<25; i++)
			Screen[40 * i + j + 1] = Screen[40 * i + j];
	}
}

#pragma data(tables)

static const char display_five_table[] = {
	190, 188, 186, 185, 183, 181, 180, 179, 178, 178, 178, 178, 168, 153, 140, 127, 115, 103, 92, 82, 72, 82, 92, 103, 115, 127, 140, 153, 168, 178, 178, 178, 178, 179, 180, 181, 183, 185, 186, 188,
	178, 176, 174, 172, 170, 168, 167, 165, 164, 163, 163, 163, 163, 149, 135, 122, 109, 97, 86, 76, 66, 76, 86, 97, 109, 122, 135, 149, 163, 163, 163, 163, 164, 165, 167, 168, 170, 172, 174, 176,
	166, 164, 161, 159, 157, 155, 154, 152, 150, 149, 148, 148, 148, 144, 130, 117, 104, 92, 80, 70, 60, 70, 80, 92, 104, 117, 130, 144, 148, 148, 148, 149, 150, 152, 154, 155, 157, 159, 161, 164,
	154, 152, 149, 147, 145, 143, 141, 139, 137, 136, 135, 134, 133, 133, 126, 112, 98, 86, 75, 64, 54, 64, 75, 86, 98, 112, 126, 133, 133, 134, 135, 136, 137, 139, 141, 143, 145, 147, 149, 152,
	143, 140, 138, 135, 133, 131, 128, 126, 124, 122, 121, 120, 119, 118, 119, 107, 93, 81, 69, 58, 48, 58, 69, 81, 93, 107, 119, 118, 119, 120, 121, 122, 124, 126, 128, 131, 133, 135, 138, 140,
	132, 129, 126, 124, 121, 119, 116, 114, 112, 110, 108, 106, 105, 104, 104, 103, 88, 75, 63, 52, 42, 52, 63, 75, 88, 103, 104, 104, 105, 106, 108, 110, 112, 114, 116, 119, 121, 124, 126, 129,
	130, 121, 115, 112, 110, 107, 105, 102, 100, 97, 95, 93, 91, 90, 89, 89, 84, 70, 57, 46, 36, 46, 57, 70, 84, 89, 89, 90, 91, 93, 95, 97, 100, 102, 105, 107, 110, 112, 115, 121,
	137, 129, 120, 111, 102, 96, 93, 91, 88, 85, 83, 81, 79, 77, 75, 74, 74, 65, 52, 40, 30, 40, 52, 65, 74, 74, 75, 77, 79, 81, 83, 85, 88, 91, 93, 96, 102, 111, 120, 129,
	145, 136, 127, 119, 110, 101, 92, 84, 77, 74, 71, 69, 66, 64, 62, 60, 59, 59, 47, 34, 24, 34, 47, 59, 59, 60, 62, 64, 66, 69, 71, 74, 77, 84, 92, 101, 110, 119, 127, 136,
	153, 144, 135, 127, 118, 109, 100, 91, 82, 74, 65, 58, 55, 52, 50, 48, 46, 45, 42, 29, 18, 29, 42, 45, 46, 48, 50, 52, 55, 58, 65, 74, 82, 91, 100, 109, 118, 127, 135, 144,
	162, 153, 144, 135, 126, 117, 108, 99, 90, 81, 73, 64, 55, 46, 38, 36, 33, 31, 30, 23, 12, 23, 30, 31, 33, 36, 38, 46, 55, 64, 73, 81, 90, 99, 108, 117, 126, 135, 144, 153,
	171, 162, 153, 144, 135, 126, 117, 108, 99, 90, 81, 72, 63, 54, 45, 36, 27, 19, 17, 15, 6, 15, 17, 19, 27, 36, 45, 54, 63, 72, 81, 90, 99, 108, 117, 126, 135, 144, 153, 162,
	180, 171, 162, 153, 144, 135, 126, 117, 108, 99, 90, 81, 72, 63, 54, 45, 36, 27, 18, 9, 0, 9, 18, 27, 36, 45, 54, 63, 72, 81, 90, 99, 108, 117, 126, 135, 144, 153, 162, 171,
	190, 181, 172, 163, 154, 145, 136, 127, 118, 109, 100, 91, 82, 73, 64, 56, 47, 38, 24, 11, 12, 11, 24, 38, 47, 56, 64, 73, 82, 91, 100, 109, 118, 127, 136, 145, 154, 163, 172, 181,
	200, 191, 182, 173, 164, 155, 146, 138, 129, 120, 111, 102, 94, 85, 75, 61, 47, 34, 21, 17, 24, 17, 21, 34, 47, 61, 75, 85, 94, 102, 111, 120, 129, 138, 146, 155, 164, 173, 182, 191,
	211, 202, 193, 184, 175, 167, 158, 149, 140, 132, 123, 113, 99, 85, 71, 57, 44, 32, 23, 28, 36, 28, 23, 32, 44, 57, 71, 85, 99, 113, 123, 132, 140, 149, 158, 167, 175, 184, 193, 202,
	222, 213, 204, 196, 187, 178, 170, 161, 151, 137, 122, 108, 95, 81, 68, 55, 42, 31, 34, 40, 48, 40, 34, 31, 42, 55, 68, 81, 95, 108, 122, 137, 151, 161, 170, 178, 187, 196, 204, 213,
	234, 225, 216, 208, 199, 189, 174, 160, 146, 132, 118, 105, 91, 78, 65, 53, 41, 40, 45, 52, 60, 52, 45, 40, 41, 53, 65, 78, 91, 105, 118, 132, 146, 160, 174, 189, 199, 208, 216, 225,
	246, 238, 226, 212, 198, 184, 170, 156, 142, 128, 115, 102, 89, 76, 64, 52, 46, 51, 56, 63, 72, 63, 56, 51, 46, 52, 64, 76, 89, 102, 115, 128, 142, 156, 170, 184, 198, 212, 226, 238,
	250, 235, 221, 207, 193, 179, 165, 152, 138, 125, 112, 99, 86, 74, 62, 52, 57, 62, 68, 75, 84, 75, 68, 62, 57, 52, 62, 74, 86, 99, 112, 125, 138, 152, 165, 179, 193, 207, 221, 235,
	245, 231, 217, 203, 189, 175, 162, 149, 135, 122, 110, 97, 85, 73, 61, 63, 68, 73, 80, 87, 96, 87, 80, 73, 68, 63, 61, 73, 85, 97, 110, 122, 135, 149, 162, 175, 189, 203, 217, 231,
	240, 227, 213, 199, 186, 172, 159, 146, 133, 120, 108, 95, 84, 72, 69, 74, 79, 85, 91, 99, 108, 99, 91, 85, 79, 74, 69, 72, 84, 95, 108, 120, 133, 146, 159, 172, 186, 199, 213, 227,
	236, 223, 209, 196, 182, 169, 156, 143, 131, 118, 106, 94, 83, 75, 80, 85, 90, 96, 103, 111, 120, 111, 103, 96, 90, 85, 80, 75, 83, 94, 106, 118, 131, 143, 156, 169, 182, 196, 209, 223,
	233, 219, 206, 193, 180, 167, 154, 141, 129, 117, 105, 93, 82, 86, 91, 96, 101, 108, 115, 123, 132, 123, 115, 108, 101, 96, 91, 86, 82, 93, 105, 117, 129, 141, 154, 167, 180, 193, 206, 219,
	229, 216, 203, 190, 177, 164, 152, 139, 127, 115, 104, 92, 92, 97, 102, 107, 113, 119, 127, 135, 144, 135, 127, 119, 113, 107, 102, 97, 92, 92, 104, 115, 127, 139, 152, 164, 177, 190, 203, 216
};

#pragma data(data)

void display_five_star(char t)
{
	char * ldp = Screen + (sindex << 3);

	#pragma unroll(page)
	for(int i=0; i<1000; i++)
	{
		ldp[i] = display_five_table[i] < t ? BC_WHITE : BC_BLACK;
	}
}

void display_explosion(void)
{
	signed char	dx[128], dy[128];
	char		xp[128], yp[128];

	for(char i=0; i<128; i++)
	{
		char s = i & 63;
		xp[i] = (rand() & 7) + 80;
		yp[i] = (rand() & 7) + 96;
		dx[i] = costab[s] >> 4;
		dy[i] = (sintab[s] >> 3) - 8;
	}

	char * ldp = Screen + (sindex << 3);
	for(int j=0; j<20; j++)
	{
		for(char i=0; i<128; i++)
		{
			if (yp[i] < 192)
			{
				ldp[40 * (yp[i] >> 3) + (xp[i] >> 2)] = BC_BLACK;

				int	x = xp[i] + (dx[i] >> 2), y = yp[i] + (dy[i] >> 2);


				if (y < 0 || y >= 192 || x < 0 || x >= 160)
					yp[i] = 255;
				else
				{
					xp[i] = x;
					yp[i] = y;

					ldp[40 * (y >> 3) + (x >> 2)] = BC_WHITE;

					dx[i] += (dx[i] >> 2);
					dy[i] += (dy[i] >> 2);
					dy[i] += 8;
				}
			}
		}
	}
}
