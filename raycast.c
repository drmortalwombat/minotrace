#include <math.h>
#include <fixmath.h>
#include <c64/vic.h>
#include <c64/asm6502.h>

#include "raycast.h"
#include "maze.h"
#include "display.h"

//static int sintab[64], costab[64];
//static int dsintab[64], dcostab[64];

#pragma data(tables)

// Table of 8.8 sine for 64 parts of a circle
const int sintab[64] = {
	0, 25, 50, 74, 98, 121, 142, 162, 181, 198, 213, 226, 237, 245, 251, 255, 256, 255, 251, 245, 237, 226, 213, 198, 181, 162, 142, 121, 98, 74, 50, 25, 0, -25, -50, -74, -98, -121, -142, -162, -181, -198, -213, -226, -237, -245, -251, -255, -256, -255, -251, -245, -237, -226, -213, -198, -181, -162, -142, -121, -98, -74, -50, -25
};

// Table of 8.8 cosine for 64 parts of a circle
const int costab[64] = {
	256, 255, 251, 245, 237, 226, 213, 198, 181, 162, 142, 121, 98, 74, 50, 25, 0, -25, -50, -74, -98, -121, -142, -162, -181, -198, -213, -226, -237, -245, -251, -255, -256, -255, -251, -245, -237, -226, -213, -198, -181, -162, -142, -121, -98, -74, -50, -25, 0, 25, 50, 74, 98, 121, 142, 162, 181, 198, 213, 226, 237, 245, 251, 255
};

// Table of scaled 8.8 sine for 64 parts of a circle for one column
const int dsintab[64] = {
	0, -1, -2, -4, -5, -6, -7, -8, -9, -10, -11, -11, -12, -12, -13, -13, -13, -13, -13, -12, -12, -11, -11, -10, -9, -8, -7, -6, -5, -4, -2, -1, 0, 1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 11, 12, 12, 13, 13, 13, 13, 13, 12, 12, 11, 11, 10, 9, 8, 7, 6, 5, 4, 2, 1
};

// Table of scaled 8.8 cosine for 64 parts of a circle for one column
const int dcostab[64] = {
	13, 13, 13, 12, 12, 11, 11, 10, 9, 8, 7, 6, 5, 4, 2, 1, 0, -1, -2, -4, -5, -6, -7, -8, -9, -10, -11, -11, -12, -12, -13, -13, -13, -13, -13, -12, -12, -11, -11, -10, -9, -8, -7, -6, -5, -4, -2, -1, 0, 1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 11, 12, 12, 13, 13
};
#pragma data(data)

// Static table of quotients 4096 / i clamped at 255
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

// Table of LSB and MSB of squares for fast 8 by 8 multiply
#pragma bss(dyntables)
char sqrtabl[256], sqrtabh[256];

#pragma align(sqrtabl, 256)
#pragma align(sqrtabh, 256)

char	blut[136];

// Result of ray cast of full screen for each column
static char col_h[41], col_x[41], col_y[41], col_d[41];

#pragma bss(bss)

// Color lookup for maze block and direction
char	clut[4 * 8] = {
	0x00, 0x00, 0x00, 0x00, 	// MF_EMPTY
	0x30, 0x30, 0x30, 0x30,		// MF_EXIT
	0xf0, 0xe0, 0xf0, 0xe0,		// MF_MINE
	0x00, 0x00, 0x00, 0x00, 	// MF_DUMMY

	0x40, 0x50, 0x60, 0x50,		// MF_RED
	0x70, 0x80, 0x90, 0x80,		// MF_BLUE
	0xa0, 0xb0, 0xc0, 0xb0,		// BF_PURPLE
	0xd0, 0xe0, 0xf0, 0xe0,		// MF_WHITE
};

void rcast_init_tables(void)
{
#if 0
	for(int i=0; i<64; i++)
	{
		float	f = 256 * sin(i * (PI / 32));
		int j = (i + 48) & 63;
		sintab[i] = f;
		costab[j] = f;
		dsintab[i] = - f * 0.05;
		dcostab[j] = f * 0.05;
		vic.color_border++;
	}	
#endif

	// Init table of squares
	for(unsigned i=0; i<256; i++)
	{
		unsigned	s = i * i;
		sqrtabl[i] = s & 0xff;
		sqrtabh[i] = s >> 8;
//		vic.color_border++;
	}

	for(int i=0; i<136; i++)
	{
		int	t = 72 / (i + 8);
		if (t > 6) t = 6;

		blut[i] = 16 * (6 - t);
	}
}

// Stub to call a fastcode function to fill a column with a given height and color
static void drawColumn(char col, char height, char color)
{
	__asm
	    {
	    ldx     col

	    // MSB address of function is based on height of column in chars
	    lda     height
	    lsr
	    lsr
	    lsr
	    ora     #$e0
	    sta     jp + 2

	    // LSB is based on double buffer screen address
	    lda		sindex
	    sta 	jp + 1

	    // Select bottom char based on height, replace the non exiting
	    // zero height char with the shared one
	    lda     height
	    and     #7
	    bne		w1
	    ldy		#2
	    bne		w2
	w1:	    
	    ora		color
	    tay
	w2:

		// Select top char based on height / 2
	    lda     height
	    lsr
	    and     #7
	    ora		#8
	    ora		color

	    // Call fastcode
	jp:
	    jmp     $e000

	    }
}

void rcast_init_fastcode(void)
{
	// Two sets of fastcode for the double buffering
	for(int s=0; s<2; s++)
	{
		// Top left screen coordinate
		unsigned	sp = (unsigned)Screen + 0x0400 * s;

		// Column heights in chars can range from 0 to 16, top section is half height
	    for(int i=0; i<=16; i++)
	    {
	    	// Start address of fastcode routine
	        char * dp = (char *)0xe000 + 256 * i + 128 * s;

	        // Start position of filled section
	        char yl = 8 - (i >> 1);
	        // Height of bottom section
	        char yh = 8 + i;

	        // Offset into fastcode
	        char di = 0;

	        // Check if top partial filled char is visible
	        if (yl > 0)
	        {
	        	// Write top char
	            unsigned fp = sp + 40 * (yl - 1);
	            di += asm_ax(dp + di, ASM_STA, fp);
	        }

	        // Check if there are inner chars to fill
	        if (yl < yh)
	        {
	        	// Load inner char for color, by masking out bits from top char
		        di += asm_im(dp + di, ASM_AND, 0xf0);

		        // Add stores to fill the inner range
		        for(char j=yl; j<yh; j++)
		        {
		            unsigned fp = sp + 40 * j;
		            di += asm_ax(dp + di, ASM_STA, fp);
		        }
		    }

		    // Is there space for the partial filled bottom char
	        if (yh < 25)
	        {
	        	// Draw bottom char
	        	di += asm_np(dp + di, ASM_TYA);
	            unsigned fp = sp + 40 * yh;
	            di += asm_ax(dp + di, ASM_STA, fp);
	        }

	        // Load empty char
	        di += asm_im(dp + di, ASM_LDA, 0x00);

	        // Fill empty top region
	        for(char j=0; j<yl - 1; j++)
	        {
	            unsigned fp =sp + 40 * j;
	            di += asm_ax(dp + di, ASM_STA, fp);
	        }

	        // Load checkerd bottom char
	        di += asm_im(dp + di, ASM_LDA, 0x03);

	        // Fill floor region
	        for(char j=yh + 1; j<25; j++)
	        {
	            unsigned fp = sp + 40 * j;
	            di += asm_ax(dp + di, ASM_STA, fp);
		    }

		    // And done
	       	di += asm_np(dp + di, ASM_RTS);
	    }
	}
}

// Square of two unsigned bytes using table lookup
static inline unsigned square(char c)
{
	return (unsigned)sqrtabl[c] | ((unsigned)sqrtabh[c] << 8);
}

// Multiply two eight bit numbers using binomials (a + b)^2 = a^2 + 2ab + b^2
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

// Calculate height of a column with given distance r and scale d
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

// Cast a single ray with unsigned fractionals
//   sx  : column on screen
//   ix  : x position in blocks in maze
//   iy  : y position in blocks in maze
//   irx : distance in x direction to wall in 8.8
//   iry : distacne in y direction to wall in 8.8
//   dix : direction in x (+1, 0, -1)
//   diy : direction in y (+1 ,0, -1)
//   idx : fractional move in x direction
//   idy : fractional move in y direction
static inline void dcast(char sx, char ix, char iy, unsigned irx, unsigned iry, signed char dix, signed char diy, unsigned idx, unsigned idy)
{
	// Start position in maze
	const char	*	bp = maze_grid + 256 * iy;

	// Sacrifice some bits of directional precision to fit into a signed byte
	char	udx = idx >> 2, udy = idy >> 2;

	// Calculate start fraction
	signed char	id = (int)(mul88(irx, udy) - mul88(iry, udx)) >> 8;

	for(;;)
	{
		// Loop while next x block is closer in ray direction
		while (id < 0)
		{
			// Advance in x direction
			ix += dix;

			// Check if filled block reached
			if (bp[ix])
			{
				// Remember block and distance
				col_x[sx] = ix;
				col_y[sx] = (char)((unsigned)bp >> 8);
				col_d[sx] = dix < 0 ? bp[ix] | 0 : bp[ix] | 2;
				col_h[sx] = colheight(udx, irx);
				return;
			}

			// Increase distance in x by one block
			irx += 256;

			// Update fraction
			id += udy;
		}

		// Loop while next y block is closer in ray direction
		while (id >= 0)
		{
			// Advance in y direction
			bp += 256 * diy;

			// Check if filled block reached
			if (bp[ix])
			{
				// Remember block and distance
				col_x[sx] = ix;
				col_y[sx] = (char)((unsigned)bp >> 8);
				col_d[sx] = diy < 0 ? bp[ix] | 1 : bp[ix] | 3;
				col_h[sx] = colheight(udy, iry);
				return;
			}

			// Increase distance in y by one block
			iry += 256;

			// Update fraction			
			id -= udx;
		}
	}
}

void rcast_draw_screen(void)
{
	// For each column
    for(char x=0; x<40; x++)
    {
    	// Get height of column
    	char w = col_h[x];

    	// Clip
   		if (w > 135)
			w = 135;

		// Call fastcode to fill
		drawColumn(x, w, clut[col_d[x]]);
    }
}

// Cast a single ray with signed fractionals
// sx  : column on screen
// ipx : x position in maze in 8.8
// ipy : y position in maze in 8.8
// idx : x component of ray in 8.8
// idy : y component of ray in 8.8

inline void icast(char sx, int ipx, int ipy, int idx, int idy)
{
	// Get integer portion of position
	char			ix = ipx >> 8, iy = ipy >> 8;

	// Unsigned fractional portion of position
	unsigned		irx = ipx & 255;
	unsigned		iry = ipy & 255;

	// Expand inline code of dcast for each possible sign combination of
	// ray direction
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
	// Let mine flicker
	clut[MF_MINE + 0] ^= 0x10;
	clut[MF_MINE + 1] ^= 0x10;
	clut[MF_MINE + 2] ^= 0x10;
	clut[MF_MINE + 3] ^= 0x10;

	// Cast left most ray
	icast(0, ipx, ipy, idx, idy);

	// Step through all columns in a step size of two
	for(int i=0; i<39; i+=2)
	{
		// Cast ray
		icast(i + 2, ipx, ipy, idx + 2 * iddx, idy + 2 * iddy);

		// Check if this is the same block as the one of the previous
		// column (two to the left)
		if (col_x[i] == col_x[i + 2] && col_y[i] == col_y[i + 2] && col_d[i] == col_d[i + 2])
		{
			// If so, just interpolate the column inbetween
			col_x[i + 1] = col_x[i];
			col_y[i + 1] = col_y[i];
			col_d[i + 1] = col_d[i];
			col_h[i + 1] = (col_h[i] + col_h[i + 2]) >> 1;
		}
		else
		{
			// Cast ray for column inbetween
			icast(i + 1, ipx, ipy, idx + iddx, idy + iddy);			
		}

		// Next column
		idx += 2 * iddx;
		idy += 2 * iddy;
	}

}

