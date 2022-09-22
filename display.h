#ifndef DISPLAY_H
#define DISPLAY_H

static char * const Screen	=	(char *)0xc000;
static char * const Screen2=	(char *)0xc400;
static char * const Font 	= 	(char *)0xc800;
static char * const Color	=	(char *)0xd800; 
static char * const Sprites =  (char *)0xd000;

extern bool 		time_running;
extern unsigned 	time_count;
extern signed char	time_digits[5];

void display_init(void);

void display_flip(void);

void compass_draw(char w);

void time_draw(void);

void time_init(unsigned seconds);

void display_put_bigtext(char x, char y, const char * text);

void display_scroll_left(void);

#pragma compile("display.c")

#endif
