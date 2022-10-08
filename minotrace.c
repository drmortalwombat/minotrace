#include <c64/vic.h>
#include <c64/memmap.h>
#include <c64/joystick.h>
#include <c64/asm6502.h>
#include <c64/rasterirq.h>
#include <c64/sprites.h>
#include <c64/sid.h>
#include <audio/sidfx.h>
#include <oscar.h>
#include <string.h>
#include <conio.h>
#include <math.h>
#include <fixmath.h>
#include <stdlib.h>
#include <stdio.h>
#include "gamemusic.h"
#include "raycast.h"
#include "maze.h"
#include "display.h"

#pragma stacksize(1024)
#pragma heapsize(0)

const SIDFX	SIDFXBounce[1] = {{
	1000, 2048, 
	SID_CTRL_GATE | SID_CTRL_NOISE,
	SID_ATK_2 | SID_DKY_6,
	0xf0  | SID_DKY_750,
	-20, 0,
	4, 20,
	3
}};

const SIDFX	SIDFXExplosion[1] = {{
	3000, 1000, 
	SID_CTRL_GATE | SID_CTRL_NOISE,
	SID_ATK_2 | SID_DKY_6,
	0xf0  | SID_DKY_3000,
	-10, 0,
	8, 120
}};

const SIDFX	SIDFXBeepShort[1] = {{
	8000, 2048, 
	SID_CTRL_GATE | SID_CTRL_RECT,
	SID_ATK_2 | SID_DKY_6,
	0xf0  | SID_DKY_72,
	0, 0,
	16, 4,
	1
}};

const SIDFX	SIDFXBeepHurry[1] = {{
	8000, 2048, 
	SID_CTRL_GATE | SID_CTRL_RECT,
	SID_ATK_2 | SID_DKY_6,
	0xf0  | SID_DKY_72,
	0, 0,
	8, 4,
	2
}};

const SIDFX	SIDFXBeepLong[1] = {{
	12000, 2048, 
	SID_CTRL_GATE | SID_CTRL_RECT,
	SID_ATK_2 | SID_DKY_6,
	0xf0  | SID_DKY_750,
	0, 0,
	20, 32,
	1
}};


enum GameStates 
{
	GS_INIT,
	GS_INTRO,
	GS_BUILD,
	GS_REVEAL,
	GS_READY,
	GS_RACE,
	GS_TIMEOUT,
	GS_EXPLOSION,
	GS_FINISHED,
	GS_RETRY,
	GS_GAMEOVER,

	NUM_GAME_STATES
}	GameState;

int		GameTime;
char	GameLevel, GameSelect;

struct PlayerStruct
{
	int			ipx, ipy;
	char		w;
	int			vx, vy;
	signed char dw;
	int			acc;

}	Player;


void maze_draw(void)
{
	char	w = Player.w;

	int		co = costab[w], si = sintab[w];

	int	idx = co + si, idy = si - co;
	int	iddx = dsintab[w], iddy = dcostab[w];

	rcast_cast_rays(Player.ipx, Player.ipy, idx, idy, iddx, iddy);

	sindex ^= 0x80;
	rcast_draw_screen();
}

void maze_flip(void)
{
	display_flip();

	time_draw();
	compass_draw(Player.w);
}

void player_init(void)
{
	Player.ipx = 3 * 128;
	Player.ipy = 25 * 128;
	Player.w = 0;
	Player.vx = 0;
	Player.vy = 0;
	Player.dw = 0;
	Player.acc = 0;
}

void player_control(void)
{
	joy_poll(0);

	if (joyx[0])
	{
		if (Player.dw == 0)
			Player.dw = 4 * joyx[0];
		else
		{
			Player.dw += joyx[0];
			if (Player.dw > 8)
				Player.dw = 8;
			else if (Player.dw < -8)
				Player.dw = -8;
		}

		Player.w = (Player.w + ((Player.dw + 2) >> 2)) & 63;	
	}
	else
		Player.dw = 0;

	if (joyb[0])
		Player.acc = 128;
	else if (joyy[0] > 0)
		Player.acc = -32;
	else if (joyy[0] < 0)
		Player.acc = 32;
	else
		Player.acc = 0;	
}

void player_move(void)
{
	int		co = costab[Player.w], si = sintab[Player.w];

	int	wx = lmul8f8s(Player.vx, co) + lmul8f8s(Player.vy, si);
	int	wy = lmul8f8s(Player.vy, co) - lmul8f8s(Player.vx, si);		

	wx = (wx * 15 + 8) >> 4;
	wy = (wy + 1) >> 1;

	wx += Player.acc;

	if (wx >= 2048)
		wx = 2048;
	else if (wx < -2048)
		wx = -2048;

	int	vx = lmul8f8s(wx, co) - lmul8f8s(wy, si);
	int	vy = lmul8f8s(wy, co) + lmul8f8s(wx, si);

	int	ipx = Player.ipx + ((vx + 8) >> 4);
	int	ipy = Player.ipy + ((vy + 8) >> 4);

	static const int wdist = 0x40;
	static const int bspeed = 0x100;

	bool	bounce = false;

	if (vx > 0 && maze_inside(ipx + wdist, ipy))
	{
		ipx = ((ipx + wdist) & 0xff00) - wdist;
		if (vx > bspeed)
			bounce = true;
		vx = -(vx >> 1);
	}
	else if (vx < 0 && maze_inside(ipx - wdist, ipy))
	{
		ipx = ((ipx - wdist) & 0xff00) + (0x100 + wdist);
		if (vx < -bspeed)
			bounce = true;
		vx = -vx >> 1;
	}

	if (vy > 0 && maze_inside(ipx, ipy + wdist))
	{
		ipy = ((ipy + wdist) & 0xff00) - wdist;
		if (vy > bspeed)
			bounce = true;
		vy = -(vy >> 1);
	}
	else if (vy < 0 && maze_inside(ipx, ipy - wdist))
	{
		ipy = ((ipy - wdist) & 0xff00) + (0x100 + wdist);
		if (vy < -bspeed)
			bounce = true;
		vy = -vy >> 1;
	}

	Player.ipx = ipx;
	Player.ipy = ipy;
	Player.vx = vx;
	Player.vy = vy;

	if (bounce)
	{
		sidfx_play(2, SIDFXBounce, 1);
	}

}

struct MazeInfo	Levels[] = 
{
	{
		MGEN_CURVES_1, 0xa321,
		34, (VCOL_GREEN << 4) | VCOL_PURPLE,
		TUNE_GAME_2, 20
	},

	{
		MGEN_CURVES_2, 0xa321,
		33, (VCOL_RED << 4) | VCOL_BLUE,
		TUNE_GAME_2, 20
	},

	{
		MGEN_GATES, 0xa321,
		33, (VCOL_YELLOW << 4) | VCOL_ORANGE,
		TUNE_GAME_3, 20
	},

	{
		MGEN_CURVES_1, 0xa321,
		66, (VCOL_LT_BLUE << 4) | VCOL_GREEN,
		TUNE_GAME_3, 35
	},

	{
		MGEN_DOORS, 0xa321,
		35, (VCOL_LT_GREEN << 4) | VCOL_MED_GREY,
		TUNE_GAME_4, 20
	},

	{
		MGEN_MINEFIELD, 0xa321,
		34, (VCOL_CYAN << 4) | VCOL_BLUE,
		TUNE_GAME_2, 20
	},

	{
		MGEN_CURVES_2, 0xa321,
		65, (VCOL_ORANGE << 4) | VCOL_LT_BLUE,
		TUNE_GAME_3, 35
	},

	{
		MGEN_GATES, 0x1781,
		66, (VCOL_PURPLE<< 4) | VCOL_YELLOW,
		TUNE_GAME_4, 30
	},

	{
		MGEN_LABYRINTH_3, 0xa321,
		34, (VCOL_DARK_GREY << 4) | VCOL_RED,
		TUNE_GAME_2, 20
	},

	{
		MGEN_LABYRINTH_1, 0xf921,
		20, (VCOL_YELLOW << 4) | VCOL_PURPLE,
		TUNE_GAME_3, 30
	},

	{
		MGEN_DOORS, 0x4521,
		55, (VCOL_CYAN << 4) | VCOL_ORANGE,
		TUNE_GAME_4, 30
	},

	{
		MGEN_CURVES_1, 0xa321,
		130, (VCOL_BLUE << 4) | VCOL_MED_GREY,
		TUNE_GAME_4, 45
	},

	{
		MGEN_CURVES_2, 0xa321,
		129, (VCOL_YELLOW << 4) | VCOL_RED,
		TUNE_GAME_1, 65
	},

	{
		MGEN_CURVES_1, 0xa321,
		226, (VCOL_GREEN << 4) | VCOL_BLUE,
		TUNE_GAME_1, 80
	},

	{
		MGEN_GATES, 0x9fb2,
		132, (VCOL_LT_BLUE << 4) | VCOL_MED_GREY,
		TUNE_GAME_3, 55
	},

	{
		MGEN_LABYRINTH_3, 0x2482,
		66, (VCOL_RED << 4) | VCOL_BLUE,
		TUNE_GAME_3, 30
	},

	{
		MGEN_LABYRINTH_1, 0xa321,
		34, (VCOL_GREEN << 4) | VCOL_PURPLE,
		TUNE_GAME_4, 60
	},

	{
		MGEN_MINEFIELD, 0x7951,
		34, (VCOL_GREEN << 4) | VCOL_RED,
		TUNE_GAME_3, 30
	},

	{
		MGEN_LABYRINTH_1, 0x2197,
		52, (VCOL_LT_GREEN<< 4) | VCOL_DARK_GREY,
		TUNE_GAME_4, 70
	},

	{
		MGEN_LABYRINTH_3, 0x9812,
		98, (VCOL_MED_GREY << 4) | VCOL_LT_BLUE,
		TUNE_GAME_2, 60
	},

	{
		MGEN_DOORS, 0x7491,
		105, (VCOL_GREEN << 4) | VCOL_PURPLE,
		TUNE_GAME_1, 60
	},

	{
		MGEN_GATES, 0xe8b1,
		252, (VCOL_YELLOW << 4) | VCOL_CYAN,
		TUNE_GAME_1, 105
	},

	{
		MGEN_MINEFIELD, 0xa952,
		100, (VCOL_DARK_GREY << 4) | VCOL_ORANGE,
		TUNE_GAME_1, 120
	},

	{
		MGEN_CURVES_2, 0xa321,
		225, (VCOL_GREEN << 4) | VCOL_PURPLE,
		TUNE_GAME_1, 90
	},

	{
		MGEN_LABYRINTH_3, 0xfe12,
		126, (VCOL_YELLOW << 4) | VCOL_CYAN,
		TUNE_GAME_1, 90
	},

	{
		MGEN_LABYRINTH_3, 0xfe12,
		254, (VCOL_ORANGE << 4) | VCOL_LT_GREEN,
		TUNE_GAME_1, 180
	},

	{
		MGEN_LABYRINTH_1, 0xcf1d,
		100, (VCOL_RED << 4) | VCOL_BLUE,
		TUNE_GAME_1, 240
	},
};



void game_advance(GameStates state)
{
	while (state != GameState)
	{
		GameState = state;

		switch (GameState)
		{
			case GS_INIT:
				break;
			case GS_INTRO:
				GameLevel = 0;
				state = GS_BUILD;
				break;
			case GS_BUILD:
				maze_build(Levels + GameLevel);
				music_patch_voice3(false);				
				music_init(Levels[GameLevel].tune);
				time_running = false;
				time_init(Levels[GameLevel].time);
				time_draw();
				state = GS_REVEAL;
				break;
			case GS_REVEAL:
				break;
			case GS_READY:
				player_init();
				GameTime = 0;
				break;
			case GS_RACE:
				time_running = true;
				break;
			case GS_EXPLOSION:
				sidfx_play(2, SIDFXExplosion, 10);
				maze_set(Player.ipx, Player.ipy, MF_EMPTY);
				display_explosion();
				GameTime = 0;
				break;
			case GS_TIMEOUT:
				GameTime = 0;
				Player.acc = 0;
				break;
			case GS_FINISHED:
				GameTime = 0;
				Player.acc = 0;
				break;
			case GS_RETRY:
				music_patch_voice3(true);
				music_init(TUNE_RESTART);
				GameTime = 0;
				GameSelect = 0;
				Player.acc = 0;
				break;			
			case GS_GAMEOVER:
				break;
		}

	}
}

bool 	game_beep;

void game_loop(void)
{
	switch (GameState)
	{
		case GS_INIT:
			break;
		case GS_INTRO:
			break;
		case GS_REVEAL:
			{
				char	t[3];
				t[0] = '0' + (GameLevel + 1) / 10;
				t[1] = '0' + (GameLevel + 1) % 10;
				t[2] = 0;
				display_put_bigtext(8, 4, s"lvl", BC_WHITE);
				display_put_bigtext(12, 14, t, BC_WHITE);
			}
			maze_preview();
			game_advance(GS_READY);
			break;
		case GS_READY:
			maze_draw();
			if (GameTime == 0 || GameTime == 25)
				sidfx_play(2, SIDFXBeepShort, 1);
			else if (GameTime == 50)
				sidfx_play(2, SIDFXBeepLong, 1);

			if (GameTime < 25)
				display_put_bigtext(0, 10, s"ready", BC_WHITE);
			else if (GameTime < 50)
				display_put_bigtext(8, 10, s"set", BC_WHITE);
			else if (GameTime < 60)
				display_put_bigtext(12, 10, s"go", BC_WHITE);
			else
				game_advance(GS_RACE);
			maze_flip();
			player_control();
			GameTime++;
			break;
		case GS_RACE:
			maze_draw();

			if (time_count < 50 * 10)
			{
				if (time_digits[3] == 4)
				{
					char	t[2];
					t[0] = time_digits[2] + '0';
					t[1] = 0;
					display_put_bigtext(16, 10, t, BC_WHITE);
					if (!game_beep)
					{
						sidfx_play(2, SIDFXBeepHurry, 1);
						game_beep = true;
					}
				}
				else
					game_beep = false;
			}

			maze_flip();
			player_control();
			player_move();
			if (time_count == 0)
				game_advance(GS_TIMEOUT);
			else
			{
				MazeFields	field = maze_field(Player.ipx, Player.ipy);
				if (field == MF_EXIT)
					game_advance(GS_FINISHED);
				else if (field == MF_MINE)
					game_advance(GS_EXPLOSION);
			}
			break;
		case GS_EXPLOSION:
			if (GameTime == 30)
			{
				joy_poll(0);
				if (!joyb[0])
					game_advance(GS_RETRY);				
			}
			else
			{
				maze_draw();

				display_put_bigtext(4, 4, s"boom", BC_WHITE);
				display_put_bigtext(0, 13, s"crash", BC_WHITE);

				maze_flip();
				Player.w = (Player.w + ((30 - GameTime) >> 2)) & 63;

				GameTime++;
			}
			break;
		case GS_TIMEOUT:
			if (GameTime == 30)
			{
				joy_poll(0);
				if (!joyb[0])
					game_advance(GS_RETRY);				
			}
			else
			{
				maze_draw();

				display_put_bigtext(4, 4, s"outa", BC_WHITE);
				display_put_bigtext(4, 13, s"time", BC_WHITE);

				maze_flip();
				player_move();

				GameTime++;
			}
			break;
		case GS_FINISHED:
			if (GameTime == 30)
			{
				GameLevel++;
				display_reset();
				game_advance(GS_BUILD);				
			}
			else
			{
				display_five_star(GameTime * 8);
				maze_flip();
				GameTime++;
			}
			break;
		case GS_RETRY:
			if (GameTime == 120 || joyb[0])
			{
				display_reset();
				game_advance(GS_BUILD);				
			}
			else
			{
				maze_draw();

				display_put_bigtext(0, 4, s"retry", GameSelect ? BC_BLACK : BC_WHITE);
				display_put_bigtext(4, 13, s"exit", GameSelect ? BC_WHITE : BC_BLACK);

				maze_flip();
				player_move();

				joy_poll(0);
				if (joyy[0] > 0)
					GameSelect = 1;
				else if (joyy[0] < 0)
					GameSelect = 0;

				GameTime++;
			}
			break;
		case GS_GAMEOVER:		
			break;
	}
}

int main(void)
{
#if 0
	maze_build_minefield(68);
	maze_print();
#else
	display_init();

	music_init(TUNE_MAIN);
	display_title();

	display_game();

    rcast_init_tables();
    rcast_init_fastcode();


	game_advance(GS_INTRO);

	for(;;)
	{
		game_loop();
	}

#endif
	return 0;
}