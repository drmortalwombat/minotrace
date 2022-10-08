#ifndef DISPLAY_H
#define DISPLAY_H


#pragma section( tables, 0)

#pragma region( tables, 0xc000, 0xc800, , , {tables} )

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

enum BlockChar
{
	BC_BLACK 	= 	0,
	BC_GREY		=	3,
	BC_WHITE	=	4,
	BC_BLUE		=	19,

};

void display_init(void);

void display_game(void);

void display_title(void);

void display_flip(void);

void display_reset(void);

void compass_draw(char w);

void time_draw(void);

void time_init(unsigned seconds);

void display_put_bigtext(char x, char y, const char * text, BlockChar c);

void display_scroll_left(void);

void display_scroll_right(void);

void display_five_star(char t);

void display_explosion(void);

#pragma compile("display.c")

#endif
