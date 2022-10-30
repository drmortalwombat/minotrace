#ifndef DISPLAY_H
#define DISPLAY_H


// Define memory regions

#pragma section( tables, 0)

#pragma region( tables, 0xb400, 0xc800, , , {tables} )

#pragma section( dyntables, 0, , , bss)

#pragma region( dyntables, 0x0400, 0x0800, , , {dyntables} )

#pragma section( sprites, 0)

#pragma region( sprites, 0xc800, 0xd000, , , {sprites} )

static char * const Screen	=	(char *)0xc800;
static char * const Screen2=	(char *)0xcc00;
static char * const Font 	= 	(char *)0xd800;
static char * const Color	=	(char *)0xd800; 
static char * const Sprites =  (char *)0xd000;
static char * const Hires	=	(char *)0xe000;

extern bool 		time_running;
extern unsigned 	time_count;
extern signed char	time_digits[5];

extern bool			ntsc;

enum BlockChar
{
	BC_BLACK 	= 	0,
	BC_GREY		=	3,
	BC_WHITE	=	4,
	BC_BLUE		=	19,

	BC_BOX_BLACK = 7,
	BC_BOX_RED = 8,
	BC_BOX_BLUE = 9

};

// Basic display and IRQ init 
void display_init(void);

// Prepare display for game play
void display_game(void);

// Display the title screen
void display_title(void);

// Display the game completed screen
void display_completed(void);

// Flip double buffer
void display_flip(void);

// Reset double buffer pair to known state
void display_reset(void);

// Set the compass direction sprite
void compass_draw(char w);

// Draw the current countdown timer using sprites
void time_draw(void);

// Init the countdown timer to the given duration
void time_init(unsigned seconds);

// Draw big text on top of the 3D Screen using the block char colour
void display_put_bigtext(char x, char y, const char * text, BlockChar c);

// Scroll the text screen one char to the left
void display_scroll_left(void);

// Scroll the text screen one char to the right
void display_scroll_right(void);

// Display phase of the star shutter at the end of the maze
void display_five_star(char t);

// Display mine explosion
void display_explosion(void);

#pragma compile("display.c")

#endif
