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

#pragma data(tables)
const char WallFontData[] = {
	#embed ctm_chars "walls.ctm"
};
#pragma data(data)

bool 		time_running;
signed char	time_digits[5];
bool		ntsc;

RIRQCode	IRQFrame;

__interrupt void irq_service(void);

static const char b[2] = {0xcc, 0x33};


const char DataTitleBits[] = {
	#embed 8000 0 lzo "titleimg.bin"
};

#pragma data(tables)

const char DataTitleColor0[] = {
	#embed 1000 8000 lzo "titleimg.bin"
};

const char DataTitleColor1[] = {
	#embed 1000 9000 lzo "titleimg.bin"
};
#pragma data(data)

// Expand the font for the wall characters
void display_font_expand(void)
{
	// Copy the first 32 characters verbatim from the
	// static character set
	memcpy(Font, WallFontData, 32 * 8);

	// Expand the remaining 13 characters into 16 characters each for
	// top, inner and bottom of a wall segment

	for(char c=0; c<13; c++)
	{
		const char * sp = WallFontData + 16 * 8 + 8 * c;

		for(char cy=0; cy<8; cy++)
		{
			char * fp = Font + 8 * (cy + 16 * c + 0x30);

			// Upper wall character
			for(char t=0; t<8; t++)
			{

				if (t > 7 - cy)
					fp[t + 64] = sp[t];	// Inside wall
				else if (t == 7 - cy)
					fp[t + 64] = 0xff;  // Upper white border line
				else
					fp[t + 64] = 0x00;	// Black ceiling
			}

			// Inner and bottom wall characters
			for(char t=0; t<8; t++)
			{
				if (cy == 0)
					fp[t] = sp[t];		// Full wall segment
				else if (t < cy)
					fp[t] = sp[t];		// Inside wall
				else if (t == cy)
					fp[t] = 0x00;		// Black bottom line of wall
				else
					fp[t] = b[t & 1];	// Checkered floor
			}
		}
	}

}

void display_init(void)
{
	__asm {
		sei
	}

	cia_init();

	// Detect PAL or NTSC
	vic_waitTop();
	vic_waitBottom();
	char	max = 0;
	while (vic.ctrl1 & VIC_CTRL1_RST8)
	{
		if (vic.raster > max)
			max = vic.raster;
	}

	ntsc = max < 8;

	// Bank out ROM
	mmap_set(MMAP_NO_ROM);

	// Init raster IRQ with memory map handler	
	rirq_init_memmap();

	// Black border and display off during initialization
	vic.color_border = VCOL_BLACK;
	vic.ctrl1 = VIC_CTRL1_RSEL + 3;

	vic_waitBottom();

	// Turn off IO range at 0xd000
	mmap_set(MMAP_RAM);

	// Expand sprite data into RAM under IO area
	oscar_expand_lzo(Sprites, SpriteData);

	// Turn IO area back on
	mmap_set(MMAP_NO_ROM);

	// Init sound system
	music_init(TUNE_GAME_2);
	sidfx_init();
	sid.fmodevol = 15;	

	// Build raster interrupt for double buffer page flip at top
	// of screen
	rirq_build(&IRQFrame, 2);
	rirq_write(&IRQFrame, 0, &vic.memptr, 0x06);
	rirq_call(&IRQFrame, 1, irq_service);

	rirq_set(0, 5, &IRQFrame);

	// Start raster IRQ processing
	rirq_sort();
	rirq_start();

	// Turn of IO range
	mmap_set(MMAP_RAM);

	// Expand font into IO range
	display_font_expand();

	// Turn IO area back on
	mmap_set(MMAP_NO_ROM);

	// Init sprite sub system
	spr_init(Screen);
}

char title_char_buffer[64];

// Expand character for title scroller

void title_char_expand(char c)
{
	// Bank in character ROM

	mmap_set(MMAP_CHAR_ROM);

	// Start address of character in ROM

	const char * sp = (char *)0xd000 + 8 * c;

	// Eight rows

	for(char iy=0; iy<8; iy++)
	{
		// Character definition byte of this row		
		char b = sp[iy];

		// Expand into a bit of eight bytes
		for(char ix=0; ix<8; ix++)
		{
			unsigned	u = (title_char_buffer[40 + ix] << 8) | b;
			u <<= 1;
			title_char_buffer[40 + ix] = u >> 8;
			b = u & 0xff;
		}
	}

	// Bank out character ROM

	mmap_set(MMAP_NO_ROM);
}

// Title color change color lookup table
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

// Game completed change color lookup table
const char ccompllut[16] = {
	VCOL_BLACK,
	VCOL_WHITE,
	VCOL_DARK_GREY,
	VCOL_CYAN,
	VCOL_PURPLE,
	VCOL_GREEN,
	VCOL_BLUE,
	VCOL_WHITE,

	VCOL_MED_GREY,
	VCOL_DARK_GREY,
	VCOL_LT_GREY,
	VCOL_DARK_GREY,
	VCOL_MED_GREY,
	VCOL_LT_GREEN,
	VCOL_LT_BLUE,
	VCOL_LT_GREY
};

// Color change lookup table for title scroller
char titlelut[256];

#pragma align(titlelut, 256);

// Expand the title text row into the color ram of the
// multicolor title screen

void title_line_expand(void)
{	
	// Target address
	char * sdp = Screen + 12 * 40, * cdp = Color + 12 * 40;

	// Source address
	char * ssp = Screen2, * csp = Screen2 + 512;

	// Fourty columns
	for(char ix=0; ix<40; ix++)
	{
		// Get byte with one bit per row
		unsigned m = title_char_buffer[ix];

		#pragma unroll(full)
		for(char iy=0; iy<8; iy++)
		{
			// Shift char byte left
			m <<= 1;
			// Check "carry"
			bool	b = (m & 0x0100) != 0;

			// Get source color
			char	sc = ssp[40 * iy + ix], cc = csp[40 * iy + ix];

			// Replace color if bit is set
			sc = b ? titlelut[sc] : sc;
			cc = b ? titlelut[cc] : cc;

			// Write target color
			sdp[40 * iy + ix] = sc;
			cdp[40 * iy + ix] = cc;
		}
	}

	// Scroll title line to the left
	for(char ix=0; ix<48; ix++)
		title_char_buffer[ix] = title_char_buffer[ix + 1];
}

const char  TitleText[] =
	S"RACE AGAINST THE CLOCK TO ESCAPE FROM THE CONVOLUTED DUNGEONS OF THE RAINBOW ZEBRA --- "
	S"DESIGN AND CODING BY DR.MORTAL WOMBAT, MUSIC BY CRISPS --- "
	S"VISIT US ON ITCH.IO FOR MORE --- "
	S"USE JOYSTICK TO STEER, PUSH BUTTON TO SPEED UP --- "
	S"GREETZ TO ECTE, SEPA, STEPZ AND PIGEON64 ---                        ";

const char  CompleteText[] =
	S"CONGRATULATIONS YOU COMPLETED ALL THE MAZES IN TIME --- ";

// Scroll the title text until user presses joystick button
void display_scroll_title(const char * text)
{
	bool	down = true;

	char x = 0, lx = 0;
	do {
		// Wait for top line of title text
		vic_waitTop();
		vic_waitLine(200);

		// Expand line into color buffer
		title_line_expand();
		lx ++;
		if (lx == 8)
		{
			// Expand one character after eight scrolls
			title_char_expand(text[x]);
			x++;
			lx = 0;
			if (!text[x])
				x = 0;
		}

		// Check joystick
		joy_poll(0);

		// Wait for up before accepting down
		if (!joyb[0])
			down = false;

	} while (down || !joyb[0]);
}

void display_show_title(void)
{
	// Turn off all sprite
	vic.spr_enable = 0;

	// Turn of display during init
	vic_waitBottom();
	vic.ctrl1 = VIC_CTRL1_RSEL + 3;
	vic_waitTop();
	vic_waitBottom();

	// Expand bitmap and color data
	oscar_expand_lzo(Hires, DataTitleBits);

	oscar_expand_lzo(Screen, DataTitleColor0);
	oscar_expand_lzo(Color, DataTitleColor1);

	vic.color_border = VCOL_BLACK;
	vic.color_back = VCOL_BLACK;

	// Display multicolor bitmap screen
	vic_setmode(VICM_HIRES_MC, Screen, Hires);
	rirq_data(&IRQFrame, 0, 0x28);

	// Copy color into back buffer for scrolling
	memcpy(Screen2, Screen + 12 * 40, 8 * 40);
	memcpy(Screen2 + 512, Color + 12 * 40, 8 * 40);	
}

void display_title(void)
{
	display_show_title();

	// Prepare full byte LUT for scroller
	for(int i=0; i<256; i++)
	{
		titlelut[i] = ctitlelut[i & 0x0f] | (ctitlelut[i >> 4] << 4);
	}

	memset(title_char_buffer, 0, 64);

	display_scroll_title(TitleText);
}

void display_completed(void)
{
	display_show_title();

	// Prepare full byte LUT for scroller
	for(int i=0; i<256; i++)
	{
		titlelut[i] = ccompllut[i & 0x0f] | (ccompllut[i >> 4] << 4);
	}

	memset(title_char_buffer, 0, 64);

	// Show DrMortalWombat and Crisps sprites
	spr_set(0, true, 24 + 126, 50 + 70, 64 + 25, VCOL_BLACK, false, false, false);
	spr_set(1, true, 24 + 126, 50 + 70, 64 + 24, VCOL_GREEN, true, false, false);

	spr_set(2, true, 24 + 210, 50 + 70, 64 + 27, VCOL_BLACK, false, false, false);
	spr_set(3, true, 24 + 210, 50 + 70, 64 + 26, VCOL_ORANGE, true, false, false);

	vic.spr_mcolor0 = VCOL_BLUE;
	vic.spr_mcolor1 = VCOL_WHITE;

	display_scroll_title(CompleteText);
}

void display_game(void)
{
	// Hide display during init
	vic_waitBottom();
	vic.ctrl1 = VIC_CTRL1_RSEL + 3;
	vic_waitTop();
	vic_waitBottom();

	// Clear screen
	memset(Screen, 0, 1000);
	memset(Color, VCOL_WHITE + 8, 1000);

	vic_waitBottom();

	// Set multicolor text mode
	vic_setmode(VICM_TEXT_MC, Screen, Font);
	rirq_data(&IRQFrame, 0, 0x26);

	vic.color_border = VCOL_BLACK;
	vic.color_back = VCOL_BLACK;
	vic.color_back1 = VCOL_YELLOW;
	vic.color_back2 = VCOL_LT_BLUE;

	vic.spr_mcolor0 = VCOL_DARK_GREY;
	vic.spr_mcolor1 = VCOL_WHITE;

	// Prepare compass and countdown sprites
	spr_set(0, true, 72, 50, 64 + 13, VCOL_LT_BLUE, true, false, false);
	for(int i=1; i<8; i++)
		spr_set(i, true, 80 + 24 * i, 50, 64 + 12, VCOL_YELLOW, true, false, false);
}

// Decrement countdown timer
void time_dec(void)
{
	// Stop when out of time
	if (time_count > 0)
	{
		// Decrement number of seconds, when 100th at zero
		if (time_digits[4] == 0 && time_digits[3] == 0)
			time_count--;

		// Still time left
		if (time_count > 0)
		{
			// 100th second
			if (--time_digits[4] >= 0)
				return;
			time_digits[4] = 9;

			if (--time_digits[3] >= 0)
				return;
			time_digits[3] = ntsc ? 5 : 4;

			// Seconds
			if (--time_digits[2] >= 0)
				return;
			time_digits[2] = 9;

			if (--time_digits[1] >= 0)
				return;
			time_digits[1] = 5;

			// Minutes
			if (--time_digits[0] >= 0)
				return;
			time_digits[0] = 9;
		}
	}
}

static inline char * sprimg_base(void)
{
	return Screen + (sindex ? 0x0400 : 0x000) + 0x3f8;
}

void compass_draw(char w)
{
	char	*	sp = sprimg_base();

	// Select one of eight compass sprites
	sp[0] = 80 + (((w + 4) >> 3) & 7);
}

void time_draw(void)
{
	char	*	sp = sprimg_base();;

	// Update time sprites
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
	time_count = seconds + 1;

	// Init time sprites
	time_digits[3] = time_digits[4] = 0;
	time_digits[2] = seconds % 10; seconds /= 10;
	time_digits[1] = seconds %  6; seconds /= 6;
	time_digits[0] = seconds;
}

__interrupt void irq_service(void)
{
	// Service interrupt, decrement time, and play sounds

	if (time_running)
		time_dec();
	sidfx_loop_2();
	music_play();
}

void display_flip(void)
{
	// Update raster IRQ for page flip

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
	// Map in character rom
	mmap_set(MMAP_CHAR_ROM);

	// Hint for the compiler, max 25 rows of text on screen
	__assume(y < 25);

	// Target address of first row on screen
	char * ldp = Screen + (sindex << 3) + 40 * y + x;

	// Loop over text until terminating 0
	char c;
	while (c = *text++)
	{
		// Address of character data in rom
		const char * sp = (char *)0xd000 + 8 * c;

		// Loop over rows in character
		char * dp = ldp;
		for(char iy=0; iy<8; iy++)
		{
			char b = sp[iy];
			// Loop over bits in character
			for(char ix=0; ix<8; ix++)
			{
				// Check left most bit
				if (b & 0x80)
					dp[ix] = bc;
				b <<= 1;
			}
			// Next row
			dp += 40;
		}
		// Next char
		ldp += 8;
	}

	// Map IO space back in
	mmap_set(MMAP_NO_ROM);
}

void display_scroll_left(void)
{
	// Copy is unrolled in three sections to race the beam
	for(char j=0; j<39; j++)
	{
		#pragma unroll(full)
		for(char i=0; i<7; i++)
			Screen[40 * i + j] = Screen[40 * i + j + 1];
	}
	for(char j=0; j<39; j++)
	{
		#pragma unroll(full)
		for(char i=7; i<15; i++)
			Screen[40 * i + j] = Screen[40 * i + j + 1];
	}
	for(char j=0; j<39; j++)
	{
		#pragma unroll(full)
		for(char i=15; i<25; i++)
			Screen[40 * i + j] = Screen[40 * i + j + 1];
	}
}

void display_scroll_right(void)
{
	// Copy is unrolled in three sections to race the beam
	for(signed char j=38; j>=0; j--)
	{
		#pragma unroll(full)
		for(char i=0; i<7; i++)
			Screen[40 * i + j + 1] = Screen[40 * i + j];
	}
	for(signed char j=38; j>=0; j--)
	{
		#pragma unroll(full)
		for(char i=7; i<15; i++)
			Screen[40 * i + j + 1] = Screen[40 * i + j];
	}
	for(signed char j=38; j>=0; j--)
	{
		#pragma unroll(full)
		for(char i=15; i<25; i++)
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
	// Base pointer on screen
	char * ldp = Screen + (sindex << 3);

	// Unroll into page size segments (4 times unrolled with 0..249 loop counter)
	#pragma unroll(page)
	for(int i=0; i<1000; i++)
	{
		// Draw white inside of star
		ldp[i] = display_five_table[i] < t ? BC_WHITE : BC_BLACK;
	}
}

void display_explosion(void)
{
	// Velocity and position of explosion particles
	signed char	dx[128], dy[128];
	char		xp[128], yp[128];

	// Initialize particles
	for(char i=0; i<128; i++)
	{
		char s = i & 63;
		xp[i] = (rand() & 7) + 80;
		yp[i] = (rand() & 7) + 96;
		dx[i] = costab[s] >> 4;
		dy[i] = (sintab[s] >> 3) - 8;
	}

	// Loop 20 times over all particles
	char * ldp = Screen + (sindex << 3);
	for(int j=0; j<20; j++)
	{
		for(char i=0; i<128; i++)
		{
			// Particle still visible?
			if (yp[i] < 192)
			{
				// Erase particle from screen
				ldp[40 * (yp[i] >> 3) + (xp[i] >> 2)] = BC_BLACK;

				// Move
				int	x = xp[i] + (dx[i] >> 2), y = yp[i] + (dy[i] >> 2);

				// Check if particle left screen
				if (y < 0 || y >= 192 || x < 0 || x >= 160)
					yp[i] = 255;
				else
				{
					xp[i] = x;
					yp[i] = y;

					// Draw particle
					ldp[40 * (y >> 3) + (x >> 2)] = BC_WHITE;

					// Increase speed
					dx[i] += (dx[i] >> 2);
					dy[i] += (dy[i] >> 2);

					// Gravity
					dy[i] += 8;
				}
			}
		}
	}
}
