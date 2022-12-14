#include "maze.h"
#include "display.h"
#include <c64/vic.h>
#include <math.h>

char 			maze_grid[256 * 25];
unsigned 		maze_size;

static unsigned maze_seed = 31232;

// Maze generator random function
unsigned int maze_rand(void)
{
    maze_seed ^= maze_seed << 7;
    maze_seed ^= maze_seed >> 9;
    maze_seed ^= maze_seed << 8;
	return maze_seed;
}

// Random number in the range 0..limit-1

unsigned int maze_randb(char limit)
{
	return maze_rand() % limit;
}


bool maze_inside(int ipx, int ipy)
{
	char	ix = ipx >> 8, iy = ipy >> 8;
	return maze_grid[iy * 256 + ix] >= MF_RED;
}

MazeFields maze_field(int ipx, int ipy)
{
	char	ix = ipx >> 8, iy = ipy >> 8;
	return (MazeFields)(maze_grid[iy * 256 + ix]);
}

void maze_set(int ipx, int ipy, MazeFields f)
{
	char	ix = ipx >> 8, iy = ipy >> 8;
	maze_grid[iy * 256 + ix] = f;
}

// Char lookup table for maze preview
static const char mazelut[] = {
	0x10, 0x06, 0x05, 0x00,
	0x12, 0x15, 0x18, 0x1b
};

void maze_preview(void)
{
	// Switch from autoscroll to user scroll, when user pushes joystick direction
	bool		autoscroll = true;
	char		lx = 0;
	int			mx = -1;

	// Joystick button may be pressed on entering the function
	bool	down = true;

	// Screen to 38 columns to enable pixel level scrolling
	vic.ctrl2 = VIC_CTRL2_MCM;

	for(;;)
	{
		if (autoscroll)
		{
			// Autoscroll by four pixels per frame
			vic_waitTop();
			vic_waitBottom();
			vic.ctrl2 = VIC_CTRL2_MCM + 4;
			display_scroll_left();

			// Check for completion of preview
			mx++;
			if (mx == maze_size + 40)
				break;			

			// Prepare new column to the right
			if (mx < maze_size)
			{
				// Expand full column display
				#pragma unroll(full)
				for(char j=0; j<25; j++)
				{
					char p = maze_grid[256 * j + mx];
					Screen[40 * j + 39] = mazelut[p >> 2];
				}
			}
			else
			{
				// Fill in a black column outside of maze
				#pragma unroll(full)
				for(char j=0; j<25; j++)
					Screen[40 * j + 39] = 0;					
			}

			// Next four pixels
			vic_waitBottom();
			vic.ctrl2 = VIC_CTRL2_MCM;
		}

		// Check joystick button
		joy_poll(0);
		if (!joyb[0])
			down = false;
		
		if (joyb[0] && !down)
		{
			// Button was up and is now down, so exit preview
			vic.ctrl2 = VIC_CTRL2_MCM | VIC_CTRL2_CSEL;
			return;
		}
		else if (joyx[0] < 0)
		{
			// End auto scroll
			autoscroll = false;

			if (lx < 7)
			{
				// Scroll one pixel to the left
				lx++;
				vic_waitBottom();
				vic.ctrl2 = VIC_CTRL2_MCM + lx;
				vic_waitTop();
			}
			else
			{
				// Full column scroll and reset pixel scroller
				vic_waitBottom();
				vic.ctrl2 = VIC_CTRL2_MCM;
				lx = 0;

				display_scroll_right();

				// New column to the left
				mx--;
				if (mx >= 39)
				{
					#pragma unroll(full)
					for(char j=0; j<25; j++)
					{
						char p = maze_grid[256 * j + mx - 39];
						Screen[40 * j] = mazelut[p >> 2];
					}
				}
				else
				{
					#pragma unroll(full)
					for(char j=0; j<25; j++)
						Screen[40 * j] = 0;					
				}
			}
		}
		else if (joyx[0] > 0)
		{
			autoscroll = false;

			if (lx > 0)
			{
				// Scroll one pixel to the right
				lx--;
				vic_waitBottom();
				vic.ctrl2 = VIC_CTRL2_MCM + lx;
				vic_waitTop();
			}
			else
			{
				// Full column scroll and reset pixel scroller
				vic_waitBottom();
				vic.ctrl2 = VIC_CTRL2_MCM + 7;
				lx = 7;

				display_scroll_left();

				// New column to the right
				mx++;
				if (mx < maze_size)
				{
					#pragma unroll(full)
					for(char j=0; j<25; j++)
					{
						char p = maze_grid[256 * j + mx];
						Screen[40 * j + 39] = mazelut[p >> 2];
					}
				}
				else
				{
					#pragma unroll(full)
					for(char j=0; j<25; j++)
						Screen[40 * j + 39] = 0;					
				}
			}
		}
	}

	for(int i=0; i<40; i++)
	{
		vic_waitBottom();
		vic.ctrl2 = VIC_CTRL2_MCM + 4;
		display_scroll_left();

		#pragma unroll(full)
		for(char j=0; j<25; j++)
			Screen[40 * j + 39] = 0;

		vic_waitBottom();
		vic.ctrl2 = VIC_CTRL2_MCM;
		vic_waitTop();
	}

	vic.ctrl2 = VIC_CTRL2_MCM | VIC_CTRL2_CSEL;	
}



void maze_print(void)
{
	for(int i=0; i<maze_size; i++)
	{
		for(int j=0; j<25; j++)
		{
			if (maze_grid[i + 256 * j] == MF_MINE)
				printf("X");
			else if (maze_grid[i + 256 * j] > 0)
				printf("#");
			else
				printf(".");
		}
		printf("\n");
	}
}

int bdir[4] = {1, 256, -1, -256};
int bdir2[4] = {2, 512, -2, -512};
int bdir4[4] = {4, 1024, -4, -1024};

bool maze_check(int p, char d)
{
	int p1 = p + bdir[d];
	int p2 = p1 + bdir[d];
	int p3 = p2 + bdir[d];

	int	d0 = bdir[(d + 1) & 3], d1 = bdir[(d + 3) & 3];

	if (maze_grid[p1] != 0xff)
		return false;

	if (maze_grid[p1 + d0] < 0xfe)
		return false;
	if (maze_grid[p1 + d0] == 0xff && maze_grid[p1 + 2 * d0] < 0xfe)
		return false;
	if (maze_grid[p1 + d1] < 0xfe)
		return false;
	if (maze_grid[p1 + d1] == 0xff && maze_grid[p1 + 2 * d1] < 0xfe)
		return false;

	if (maze_grid[p2] == 0xfe)
		return true;

	if (maze_grid[p2 + d0] < 0xfe)
		return false;
	if (maze_grid[p2 + d0] == 0xff && maze_grid[p2 + 2 * d0] < 0xfe)
		return false;
	if (maze_grid[p2 + d1] < 0xfe)
		return false;
	if (maze_grid[p2 + d1] == 0xff && maze_grid[p2 + 2 * d1] < 0xfe)
		return false;

	if (maze_grid[p3] == 0xfe)
		return true;

	if (maze_grid[p3 + d0] < 0xfe)
		return false;
	if (maze_grid[p3 + d1] < 0xfe)
		return false;

	return true;
}

bool maze_check_3(int p, char d)
{
	int p1 = p + bdir[d];
	int p2 = p1 + bdir[d];

	int	d0 = bdir[(d + 1) & 3], d1 = bdir[(d + 3) & 3];

	return 
		maze_grid[p1] == 0xff &&
		maze_grid[p1 + d0] >= 0xfe &&
		maze_grid[p1 + d1] >= 0xfe &&
		maze_grid[p2] >= 0xfe &&
		maze_grid[p2 + d0] >= 0xfe &&
		maze_grid[p2 + d1] >= 0xfe;
}

bool maze_check_3by4(int p, char d)
{
	int p1 = p + bdir4[d];
	int p2 = p1 + bdir4[d];

	int	d0 = bdir4[(d + 1) & 3], d1 = bdir4[(d + 3) & 3];

	return 
		p1 >= 0 &&
		maze_grid[p1] == 0xff &&
		maze_grid[p1 + d0] >= 0xfe &&
		maze_grid[p1 + d1] >= 0xfe &&
		(p2 < 0 ||
			maze_grid[p2] >= 0xfe &&
			maze_grid[p2 + d0] >= 0xfe &&
			maze_grid[p2 + d1] >= 0xfe);
}

// Build an empty maze with border
void maze_build_border(unsigned size, char fill, char border)
{
	// Fill inner area
	memset(maze_grid, fill, 25 * 256);
	maze_size = size;

	// Fill borders
	for(int i=0; i<24; i++)
	{
		maze_grid[i * 256 + 0] = border;
		maze_grid[i * 256 + size - 1] = border;
	}

	for(int i=0; i<256; i++)
	{
		maze_grid[i] = border;
		maze_grid[i + 256 * 24] = border;
	}
}

// Add exit blocks and clear area in front
void maze_build_exit(void)
{
	for(char i=0; i<8; i++)
	{
		maze_grid[8 * 256 + i * 256 + maze_size - 1] = MF_EXIT;
		maze_grid[8 * 256 + i * 256 + maze_size - 2] = MF_EMPTY;
	}
	maze_grid[11 * 256 + 1] = MF_EMPTY;
	maze_grid[12 * 256 + 1] = MF_EMPTY;	
}

// Build a maze with mines
void maze_build_minefield(unsigned size)
{
	maze_build_border(size, MF_EMPTY, MF_PURPLE);

	// For each column
	for(unsigned i=1; i<size-1; i++)
	{
		// Place eight random blocks
		for(char j=0; j<8; j++)
		{
			char x = maze_randb(23) + 1;
			maze_grid[256 * x + i] = MF_RED + 4 * (j & 1);
		}

		// Place one mine
		char x = maze_randb(23) + 1;
		maze_grid[256 * x + i] = MF_MINE;
	}

	maze_build_exit();
}

void maze_build_cross(unsigned size)
{
	maze_build_border(size, MF_EMPTY, MF_PURPLE);

	for(unsigned i=2; i + 3<size-1; i+=3)
	{
		char	sx = (i & 1) * 2;

		for(char j=sx; j<23; j+=4)
		{
			maze_grid[256 * (j + 1) + i + 0] = MF_RED;
			maze_grid[256 * (j + 0) + i + 1] = MF_BLUE;
			maze_grid[256 * (j + 1) + i + 1] = MF_BLUE;
			maze_grid[256 * (j + 2) + i + 1] = MF_BLUE;
			maze_grid[256 * (j + 1) + i + 2] = MF_RED;
		}

		for(char j=0; j<2; j++)
		{
			char x = maze_randb(6) * 4 + 3 - sx;
			maze_grid[256 * x + i + 1] = MF_PURPLE;
		}
	}

	maze_build_exit();
}

void maze_build_labyrinth_3(unsigned size)
{
	maze_build_border(size, 0xff, 0xfe);

	int	p = 256 * 12 + size / 2;
	maze_grid[p] = 0xfc;

	for(;;)
	{
		char d = maze_rand() & 3;

		char i = 0;
		while (i < 4 && !maze_check(p, d))
		{
			d = (d + 1) & 3;
			i++;
		}

		if (i == 4)
		{
			p -= bdir[maze_grid[p]];
			if (maze_grid[p] == 0xfc)
			{
				maze_grid[p] = 0;

				for(int i=0; i<25; i++)
				{
					for(int j=0; j<size; j++)
					{
						if (maze_grid[i * 256 + j] > 0x80)
							maze_grid[i * 256 + j] = MF_RED + 4 * (1 & ((i >> 2) ^ (j >> 2)));
						else
						{
							maze_grid[i * 256 + j] = MF_EMPTY;
							if (i > 1)
								maze_grid[i * 256 + j - 256] = MF_EMPTY;
							if (j > 1)
							{
								maze_grid[i * 256 + j - 1] = MF_EMPTY;
								if (i > 1)
									maze_grid[i * 256 + j - 257] = MF_EMPTY;
							}
						}
					}
				}

				maze_build_exit();
				return;
			}
		}
		else
		{			
			p += bdir[d];
			maze_grid[p] = d;
		}
	}
}

// Build labyrinth with one wide path
void maze_build_labyrinth_1(unsigned size)
{
	maze_build_border(size, 0xff, 0xfe);

	// Start position of path
	int	p = 256 * 12 + size / 2;

	// Mark start field
	maze_grid[p] = 0xfc;

	for(;;)
	{
		// Random direction to look at first
		char d = maze_rand() & 3;

		char i = 0;

		// Rotation to look around if no path forward
		char dd = 1 + (maze_rand() & 2);

		// Find a path forward by looking in all four directions
		while (i < 4 && !maze_check_3(p, d))
		{
			d = (d + dd) & 3;
			i++;
		}

		if (i == 4)
		{
			// No path possible, backtrack			
			p -= bdir[maze_grid[p]];

			if (maze_grid[p] == 0xfc)
			{
				// Back at start location, clear it
				maze_grid[p] = 0;

				// Fill maze with alternating color blocks
				for(int i=0; i<25; i++)
				{
					for(int j=0; j<size; j++)
					{
						if (maze_grid[i * 256 + j] > 0x80)
							maze_grid[i * 256 + j] = MF_RED + 4 * (1 & ((i >> 2) ^ (j >> 2)));
						else
							maze_grid[i * 256 + j] = MF_EMPTY;
					}
				}

				maze_build_exit();
				return;
			}
		}
		else
		{			
			// Move along selected direction
			p += bdir[d];
			// Remember direction, that brought us here for back tracking
			maze_grid[p] = d;
		}
	}
}

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

void maze_build_path(const char * kns, char steps, char sel, char wall, unsigned size)
{
	maze_build_border(size, MF_EMPTY, MF_PURPLE);

	int k0 = kns[0];
	bool	t0 = true, t1 = true;

	for(unsigned i=1; i<size - 1; i++)
	{
		char j = i & (steps - 1);

		if (j == sel)
		{
			if (maze_rand() & 1)
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

		int kmin2 = 24 - kmax1;
		int kmax2 = 24 - kmin1;

		for(int k=1; k<24; k++)
		{
			if ((j < wall || t0) && k > kmin1 && k <= kmax1 || (j < wall || t1) && k >= kmin2 && k < kmax2)
				maze_grid[256 * k + i] = MF_EMPTY;
			else
				maze_grid[256 * k + i] = MF_RED + 4 * (i & 1);
		}

		k0 = k1;
	}

	maze_build_exit();
}

void maze_build_curves_1(unsigned size)
{
	static const char	kns[16] = {12, 12, 12, 11, 9, 7, 5, 3, 2, 2, 2, 3, 5, 7, 9, 11};
	maze_build_path(kns, 16, 5, 13, size);
}

void maze_build_curves_2(unsigned size)
{
	static const char	kns[16] = {12, 8, 5, 3, 2, 3, 5, 8, 12, 15, 18, 20, 21, 20, 18, 15};
	maze_build_path(kns, 16, 1, 0, size);
}


void maze_build_gates(unsigned size)
{
	maze_build_border(size, MF_EMPTY, MF_PURPLE);

	signed char	x = 6;
	for(int i=1; i + 2 < size; i+=3)
	{

		for(int k=1; k<24; k++)
		{
			if (k < x + 5 || k >= x + 7)
			{
				maze_grid[256 * k + i + 0] = MF_RED + 4 * (k & 1);
			}
			else
			{
				maze_grid[256 * k + i + 0] = MF_EMPTY;
			}

			if (k < x || k >= x + 12)
			{
				maze_grid[256 * k + i + 1] = MF_RED + 4 * (k & 1);
				maze_grid[256 * k + i + 2] = MF_RED + 4 * (k & 1);
			}
			else
			{
				maze_grid[256 * k + i + 1] = MF_EMPTY;
				maze_grid[256 * k + i + 2] = MF_EMPTY;
			}

		}

		x = x - 5 + maze_randb(10);
		if (x < 1) x = 6;
		if (x > 12) x = 7;
	}

	maze_build_exit();	
}

void maze_build_doors(unsigned size)
{
	maze_build_border(size, MF_EMPTY, MF_PURPLE);

	for(int i=4; i + 4 < size; i+=4)
	{
		for(int k=1; k<24; k++)
		{
			maze_grid[256 * k + i + 0] = MF_RED + 4 * (k & 1);
		}
	}

	for(int i=4; i+4 < 25; i+=4)
	{
		for(int k=1; k+1<size; k++)
		{
			maze_grid[256 * i + k + 0] = MF_RED + 4 * (k & 1);
		}		
	}

	for(int i=0; i < size + 4; i+=4)
	{
		for(int k=0; k<25; k+=4)
		{
			maze_grid[256 * k + i + 0] = 
				(i == 0 || i > size || k == 24) ? 0xfe : 0xff;
		}
	}

	int	p = 256 * 12 + 4;
	maze_grid[p] = 0xfc;

	for(;;)
	{
		char d = maze_rand() & 3;

		char i = 0;
		while (i < 4 && !maze_check_3by4(p, d))
		{
			d = (d + 1) & 3;
			i++;
		}

		if (i == 4)
		{
			p -= bdir4[maze_grid[p]];
			if (maze_grid[p] == 0xfc)
			{
				for(int i=0; i < size; i+=4)
				{
					for(int k=0; k<25; k+=4)
						maze_grid[256 * k + i + 0] = MF_RED;
				}

				maze_build_exit();
				return;
			}
		}
		else
		{			
			maze_grid[p + 510 + bdir2[d]] = MF_EMPTY;
			p += bdir4[d];
			maze_grid[p] = d;
		}
	}

	maze_build_exit();	
}


void maze_build(const MazeInfo * info)
{
	// Set seed for maze random generator
	maze_seed = info->seed;

	// Build maze with requested generator
	switch(info->gen)
	{
		case MGEN_LABYRINTH_1:
			maze_build_labyrinth_1(info->size + 2);
			break;
		case MGEN_LABYRINTH_3:
			maze_build_labyrinth_3(info->size + 2);
			break;
		case MGEN_DOORS:
			maze_build_doors(info->size + 2);
			break;
		case MGEN_MINEFIELD:
			maze_build_minefield(info->size + 2);
			break;
		case MGEN_GATES:
			maze_build_gates(info->size + 2);
			break;
		case MGEN_CURVES_1:
			maze_build_curves_1(info->size + 2);
			break;
		case MGEN_CURVES_2:
			maze_build_curves_2(info->size + 2);
			break;
		case MGEN_CROSS:
			maze_build_cross(info->size + 2);
			break;
	}

	// Extract colors
	vic.color_back1 = info->colors >> 4;
	vic.color_back2 = info->colors & 0x0f;
}
