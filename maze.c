#include "maze.h"
#include "display.h"
#include <c64/vic.h>
#include <math.h>

char 			maze_grid[256 * 24];
extern unsigned maze_size;


bool maze_inside(int ipx, int ipy)
{
	char	ix = ipx >> 8, iy = ipy >> 8;
	return maze_grid[iy * 256 + ix] != 0;
}


void maze_preview(void)
{
	vic.ctrl2 = VIC_CTRL2_MCM;
	for(int i=0; i<maze_size; i++)
	{
		vic_waitBottom();
		vic.ctrl2 = VIC_CTRL2_MCM + 4;
		display_scroll_left();

		#pragma unroll(full)
		for(char j=0; j<24; j++)
		{
			char p = maze_grid[256 * j + i];
			if (p)
				p += 0x11;
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
		display_scroll_left();

		#pragma unroll(full)
		for(char j=0; j<24; j++)
			Screen[40 * j + 39] = 0;

		vic_waitBottom();
		vic.ctrl2 = VIC_CTRL2_MCM;
		vic_waitTop();
	}
}



void maze_print(void)
{
	for(int i=0; i<256; i++)
	{
		for(int j=0; j<24; j++)
		{
			if (maze_grid[i + 256 * j] >= 0x10)
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
		maze_grid[p1] == 0xff &&
		maze_grid[p1 + d0] >= 0xfe &&
		maze_grid[p1 + d1] >= 0xfe &&
		maze_grid[p1 + 2 * d0] >= 0xfe &&
		maze_grid[p1 + 2 * d1] >= 0xfe &&
		maze_grid[p2] >= 0xfe &&
		maze_grid[p2 + d0] >= 0xfe &&
		maze_grid[p2 + d1] >= 0xfe &&
		maze_grid[p2 + 2 * d0] >= 0xfe &&
		maze_grid[p2 + 2 * d1] >= 0xfe &&
		maze_grid[p3] >= 0xfe &&
		maze_grid[p3 + d0] >= 0xfe &&
		maze_grid[p3 + d1] >= 0xfe;
}

bool maze_check_3(int p, char d)
{
	int p1 = p + bdir[d];
	int p2 = p1 + bdir[d];
	int p3 = p2 + bdir[d];

	int	d0 = bdir[(d + 1) & 3], d1 = bdir[(d + 3) & 3];

	return 
		maze_grid[p1] == 0xff &&
		maze_grid[p1 + d0] >= 0xfe &&
		maze_grid[p1 + d1] >= 0xfe &&
		maze_grid[p2] >= 0xfe &&
		maze_grid[p2 + d0] >= 0xfe &&
		maze_grid[p2 + d1] >= 0xfe;
}

void maze_build_1(unsigned size)
{
	memset(maze_grid, 0xff, 24 * 256);
	maze_size = size;

	for(int i=0; i<24; i++)
	{
		maze_grid[i * 256 + 0] = 0xfe;
		maze_grid[i * 256 + size - 1] = 0xfe;
	}

	for(int i=0; i<256; i++)
	{
		maze_grid[i] = 0xfe;
		maze_grid[i + 256 * 23] = 0xfe;
	}

	int	p = 256 * 12 + 1;
	maze_grid[p] = 0xfc;

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
			p -= bdir[maze_grid[p]];
			if (maze_grid[p] == 0xfc)
			{
				maze_grid[p] = 0;

				for(int i=0; i<24; i++)
				{
					for(int j=0; j<size; j++)
					{
						if (maze_grid[i * 256 + j] > 0x80)
							maze_grid[i * 256 + j] = 4 + 4 * (1 & ((i >> 2) ^ (j >> 2)));
						else
						{
							maze_grid[i * 256 + j] = 0;
							maze_grid[i * 256 + j - 256] = 0;
							if (j > 1)
							{
								maze_grid[i * 256 + j - 1] = 0;
								maze_grid[i * 256 + j - 257] = 0;
							}
						}
					}
				}

				for(char i=0; i<8; i++)
					maze_grid[8 * 256 + i * 256 + size - 1] = 20;

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

void maze_build_3(void)
{
	memset(maze_grid, 0xff, 24 * 256);

	for(int i=0; i<24; i++)
	{
		maze_grid[i * 256 + 0] = 0xfe;
		maze_grid[i * 256 + 255] = 0xfe;
	}

	for(int i=0; i<256; i++)
	{
		maze_grid[i] = 0xfe;
		maze_grid[i + 256 * 23] = 0xfe;
	}

	int	p = 256 * 12 + 1;
	maze_grid[p] = 0xfc;

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
			p -= bdir[maze_grid[p]];
			if (maze_grid[p] == 0xfc)
			{
				maze_grid[p] = 0;

				for(int i=0; i<24; i++)
				{
					for(int j=0; j<256; j++)
					{
						if (maze_grid[i * 256 + j] > 0x80)
							maze_grid[i * 256 + j] = 16 + 128 * (1 & ((i >> 2) ^ (j >> 2)));
						else
							maze_grid[i * 256 + j] = 0;
					}
				}

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

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

void maze_build_path(const char * kns, char steps, char sel, char wall, unsigned size)
{
	memset(maze_grid, 0xa0, 24 * 256);
	maze_size = size;

	for(int i=0; i<24; i++)
	{
		maze_grid[i * 256 + 0] = 0x20;
		maze_grid[i * 256 + size - 1] = 0x20;
	}

	int k0 = kns[0];
	bool	t0 = true, t1 = true;

	for(int i=0; i<40; i++)
		rand();

	for(unsigned i=1; i<size - 1; i++)
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
				maze_grid[256 * k + i] = 0;
			else
				maze_grid[256 * k + i] = 0x10 + 0x80 * (i & 1);
		}

		k0 = k1;
	}
}

void maze_build_2(unsigned size)
{
	char	kns[16];

	for(int i=0; i<16; i++)
		kns[i] = 7 + 5 * sin((i + 3) * (PI / 8));

	maze_build_path(kns, 16, 5, 13, size);
}

void maze_build_4(unsigned size)
{
	char	kns[32];

	for(int i=0; i<32; i++)
		kns[i] = 7 + 5 * sin((i + 3) * (PI / 16));

	maze_build_path(kns, 32, 5, 26, size);
}

void maze_build_5(unsigned size)
{
	char	kns[16];

	for(int i=0; i<16; i++)
		kns[i] = 12 - 10 * sin(i * (PI / 16));

	maze_build_path(kns, 16, 0, 0, size);
}


void maze_build_6(void)
{
	memset(maze_grid, 0xa0, 24 * 256);

	for(int i=0; i<24; i++)
	{
		maze_grid[i * 256 + 0] = 0x20;
		maze_grid[i * 256 + 255] = 0x20;
	}

	signed char	x = 6;
	for(int i=1; i<255; i+=3)
	{

		for(int k=1; k<23; k++)
		{
			if (k < x + 5 || k >= x + 7)
			{
				maze_grid[256 * k + i + 0] = 0x10 + 0x80 * (k & 1);
			}
			else
			{
				maze_grid[256 * k + i + 0] = 0;
			}

			if (k < x || k >= x + 12)
			{
				maze_grid[256 * k + i + 1] = 0x10 + 0x80 * (k & 1);
				maze_grid[256 * k + i + 2] = 0x10 + 0x80 * (k & 1);
			}
			else
			{
				maze_grid[256 * k + i + 1] = 0;
				maze_grid[256 * k + i + 2] = 0;
			}

		}

		x = x - 5 + rand() % 10;
		if (x < 1) x = 6;
		if (x > 12) x = 7;
	}
}

