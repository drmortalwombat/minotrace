#include <c64/vic.h>
#include <c64/memmap.h>
#include <c64/joystick.h>
#include <c64/asm6502.h>
#include <c64/rasterirq.h>
#include <c64/sprites.h>
#include <c64/sid.h>
#include <c64/keyboard.h>
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

#pragma data(tables)

// Sound effect bouncing against wall
const SIDFX	SIDFXBounce[1] = {{
	1000, 2048, 
	SID_CTRL_GATE | SID_CTRL_NOISE,
	SID_ATK_2 | SID_DKY_6,
	0xf0  | SID_DKY_750,
	-20, 0,
	4, 20,
	3
}};

// Sound effect exploding mine
const SIDFX	SIDFXExplosion[1] = {{
	3000, 1000, 
	SID_CTRL_GATE | SID_CTRL_NOISE,
	SID_ATK_2 | SID_DKY_6,
	0xf0  | SID_DKY_3000,
	-10, 0,
	8, 120
}};

// Sound effect short count down beep
const SIDFX	SIDFXBeepShort[1] = {{
	8000, 2048, 
	SID_CTRL_GATE | SID_CTRL_RECT,
	SID_ATK_2 | SID_DKY_6,
	0xf0  | SID_DKY_72,
	0, 0,
	16, 4,
	1
}};

// Sound effect hurry up beep
const SIDFX	SIDFXBeepHurry[1] = {{
	8000, 2048, 
	SID_CTRL_GATE | SID_CTRL_RECT,
	SID_ATK_2 | SID_DKY_6,
	0xf0  | SID_DKY_72,
	0, 0,
	8, 4,
	2
}};

// Sound effect long start beep
const SIDFX	SIDFXBeepLong[1] = {{
	12000, 2048, 
	SID_CTRL_GATE | SID_CTRL_RECT,
	SID_ATK_2 | SID_DKY_6,
	0xf0  | SID_DKY_750,
	0, 0,
	20, 32,
	1
}};

#pragma data(data)

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
	GS_PAUSE,
	GS_COMPLETED,

	NUM_GAME_STATES
}	GameState;

int		GameTime;
char	GameLevel, GameSelect;
bool	GameDown;

struct PlayerStruct
{
	int			ipx, ipy;
	char		w;
	int			vx, vy;
	signed char dw;
	int			acc;

}	Player;


// Draw the maze at the current player position and direction
void maze_draw(void)
{
	char	w = Player.w;

	// View direction vector
	int		co = costab[w], si = sintab[w];

	// Assume a 45 degree view opening, adjust view vector
	// to point to left most column
	int	idx = co + si, idy = si - co;

	// Direction vector one column to the right
	int	iddx = dsintab[w], iddy = dcostab[w];

	// Cast all rays for the screen
	rcast_cast_rays(Player.ipx, Player.ipy, idx, idy, iddx, iddy);

	// Draw screen on backbuffer
	sindex ^= 0x80;
	rcast_draw_screen();
}

// Flip the double buffer
void maze_flip(void)
{
	// Change display start address on next vblank
	display_flip();

	// Draw time and compass sprites
	time_draw();
	compass_draw(Player.w);
}

void player_init(void)
{
	// Start position 1.5 tiles to the left, and 12.5 tiles down
	Player.ipx = 3 * 128;
	Player.ipy = 25 * 128;

	// Looking to the right
	Player.w = 0;

	// No velocity
	Player.vx = 0;
	Player.vy = 0;
	Player.dw = 0;
	Player.acc = 0;
}

// Check player joystick control
void player_control(void)
{
	// Poll joystick
	joy_poll(0);

	// Rotation based on left/right
	if (joyx[0])
	{
		// Was centered before than start big
		if (Player.dw == 0)
			Player.dw = 4 * joyx[0];
		else
		{
			// Increase rotation a little more			
			Player.dw += joyx[0];

			// Clamp angular velocity
			if (Player.dw > 8)
				Player.dw = 8;
			else if (Player.dw < -8)
				Player.dw = -8;
		}

		// Update player direction base on angular velocity
		Player.w = (Player.w + ((Player.dw + 2) >> 2)) & 63;	
	}
	else
		Player.dw = 0;

	// Calculate acceleration based on up/down direction

	if (joyb[0])
		Player.acc = 128;	// Fast forward
	else if (joyy[0] > 0)
		Player.acc = -32;	// Reverse
	else if (joyy[0] < 0)	
		Player.acc = 32;	// Forward
	else
		Player.acc = 0;		// Steady
}

// Advance player speed and position
void player_move(void)
{
	// Player direction
	int		co = costab[Player.w], si = sintab[Player.w];

	// Split current velocity into components parallel (wy) and perpendicular (wx) 
	// to player direction
	int	wx = lmul8f8s(Player.vx, co) + lmul8f8s(Player.vy, si);
	int	wy = lmul8f8s(Player.vy, co) - lmul8f8s(Player.vx, si);		

	// Small friction in forward direction
	wx = (wx * 15 + 8) >> 4;

	// Apply heavy friction perpendicular (wheel like)
	wy = (wy + 1) >> 1;

	// Apply acceleration in forward direction
	wx += Player.acc;

	// Clamp speed
	if (wx >= 2048)
		wx = 2048;
	else if (wx < -2048)
		wx = -2048;

	// Rotate vector back into global coordinate system
	int	vx = lmul8f8s(wx, co) - lmul8f8s(wy, si);
	int	vy = lmul8f8s(wy, co) + lmul8f8s(wx, si);

	// Update player position by speed
	int	ipx = Player.ipx + ((vx + 8) >> 4);
	int	ipy = Player.ipy + ((vy + 8) >> 4);

	static const int wdist = 0x40;		// Minimum wall distance is a quarter block
	static const int bspeed = 0x100;

	// Collision check

	bool	bounce = false;
	
	// Colliding in positive x direction	
	if (vx > 0 && maze_inside(ipx + wdist, ipy))
	{
		// Get back into empty block
		ipx = ((ipx + wdist) & 0xff00) - wdist;
		if (vx > bspeed)
			bounce = true;

		// Reflect velocity on wall
		vx = -(vx >> 1);
	}
	else if (vx < 0 && maze_inside(ipx - wdist, ipy))
	{
		ipx = ((ipx - wdist) & 0xff00) + (0x100 + wdist);
		if (vx < -bspeed)
			bounce = true;
		vx = -vx >> 1;
	}

	// Colliding in positive y direction	
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

	// Play bounce sound if player collided fast
	if (bounce)
	{
		sidfx_play(2, SIDFXBounce, 1);
	}

}

#pragma data(tables)

// Table of levels
struct MazeInfo	Levels[27] = 
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
//		10, (VCOL_RED << 4) | VCOL_BLUE,
		100, (VCOL_RED << 4) | VCOL_BLUE,
		TUNE_GAME_1, 240
	},

	// 27
};

#pragma data(data)

// Advance game state machine
void game_advance(GameStates state)
{
	// Avoid recursion with a loop
	while (state != GameState)
	{
		GameState = state;

		switch (GameState)
		{
			case GS_INIT:
				break;
			case GS_INTRO:
				// Enable all three voices for music
				music_patch_voice3(false);				
				music_init(TUNE_MAIN);

				// Display title screen
				display_title();
				
				// Prepare game play
				display_game();
    			rcast_init_tables();
    			rcast_init_fastcode();

				GameLevel = 0;
				state = GS_BUILD;
				break;
			case GS_BUILD:
				// Build next level
				maze_build(Levels + GameLevel);
				music_patch_voice3(false);				
				music_init(Levels[GameLevel].tune);

				// Time is not yet running
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
				// Start the countdown timer
				time_running = true;
				break;
			case GS_EXPLOSION:
				// Mine explosion
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
			case GS_PAUSE:
				GameSelect = 0;
				time_running = false;
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

			case GS_COMPLETED:
				music_patch_voice3(false);				
				music_init(TUNE_MAIN);

				display_completed();
				
				display_game();
    			rcast_init_tables();
    			rcast_init_fastcode();

				GameLevel = 0;
				state = GS_BUILD;
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
			// Put level number on screen
			{
				char	t[3];
				t[0] = '0' + (GameLevel + 1) / 10;
				t[1] = '0' + (GameLevel + 1) % 10;
				t[2] = 0;
				display_put_bigtext(8, 4, s"lvl", BC_WHITE);
				display_put_bigtext(12, 14, t, BC_WHITE);
			}
			// Preview maze
			maze_preview();
			game_advance(GS_READY);
			break;
		case GS_READY:
			// Draw first person view
			maze_draw();

			// Play beep sound every seconc
			if (GameTime == 0 || GameTime == 25)
				sidfx_play(2, SIDFXBeepShort, 1);
			else if (GameTime == 50)
				sidfx_play(2, SIDFXBeepLong, 1);

			// Draw overlay text
			if (GameTime < 25)
				display_put_bigtext(0, 10, s"ready", BC_WHITE);
			else if (GameTime < 50)
				display_put_bigtext(8, 10, s"set", BC_WHITE);
			else if (GameTime < 60)
				display_put_bigtext(12, 10, s"go", BC_WHITE);
			else
				game_advance(GS_RACE);

			// Flip double buffer
			maze_flip();

			// Let player rotate, but not move
			player_control();
			GameTime++;
			break;
		case GS_RACE:
			// Draw first person view
			maze_draw();

			// Show countdown of last 10 seconds
			if (time_count <= 10)
			{
				if (time_digits[3] >= 4)
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

			// Flip double buffer
			maze_flip();

			// Player joystick control
			player_control();

			// Player advance
			player_move();

			// Check pause or timeout
			keyb_poll();
			if (keyb_key == KSCAN_QUAL_DOWN + KSCAN_STOP)
				game_advance(GS_PAUSE);
			else if (time_count == 0)
				game_advance(GS_TIMEOUT);
			else
			{
				// Check collision with mine or exit
				MazeFields	field = maze_field(Player.ipx, Player.ipy);
				if (field == MF_EXIT)
					game_advance(GS_FINISHED);
				else if (field == MF_MINE)
					game_advance(GS_EXPLOSION);
			}
			break;

		case GS_PAUSE:
			// Pause menu
			if (joyb[0])
			{
				if (GameSelect == 1)
				{
					display_reset();
					game_advance(GS_BUILD);	
				}
				else if (GameSelect == 2)
				{
					display_reset();
					game_advance(GS_INTRO);				
				}
				else
				{
					game_advance(GS_RACE);				
				}
			}
			else
			{
				maze_draw();

				// Show menu text
				display_put_bigtext(4,  1, s"cont", GameSelect == 0 ? BC_BOX_RED : BC_BOX_BLACK);
				display_put_bigtext(0,  9, s"retry", GameSelect == 1 ? BC_BOX_RED : BC_BOX_BLACK);
				display_put_bigtext(4, 17, s"exit", GameSelect == 2 ? BC_BOX_RED : BC_BOX_BLACK);

				maze_flip();

				joy_poll(0);

				if (joyy[0] == 0)
					GameDown = false;
				else if (!GameDown)
				{
					if (joyy[0] > 0 && GameSelect < 2)
					{
						GameSelect ++;
						GameDown = true;
					}
					else if (joyy[0] < 0 && GameSelect > 0)
					{
						GameSelect --;
						GameDown = true;
					}
				}
			}

			break;
		case GS_EXPLOSION:
			// Rotate player after explosion for one second

			if (GameTime == 30)
			{
				// Only advance if button is not pressed
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

				// Rotate
				Player.w = (Player.w + ((30 - GameTime) >> 2)) & 63;

				GameTime++;
			}
			break;
		case GS_TIMEOUT:
			// Show time out message for one second

			if (GameTime == 30)
			{
				// Only advance if button is not pressed
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
			// Level completed

			if (GameTime == 30)
			{
				GameLevel++;
				display_reset();
				if (GameLevel == 27)
					game_advance(GS_COMPLETED);
				else
					game_advance(GS_BUILD);				
			}
			else
			{
				// Show star transition

				display_five_star(GameTime * 8);
				maze_flip();
				GameTime++;
			}
			break;
		case GS_RETRY:
			// Show retry menu for 4 seconds
		
			if (GameTime == 120 || joyb[0])
			{
				display_reset();
				if (GameSelect)
					game_advance(GS_INTRO);				
				else
					game_advance(GS_BUILD);				
			}
			else
			{
				maze_draw();

				display_put_bigtext(0, 4, s"retry", GameSelect == 0 ? BC_BOX_RED : BC_BOX_BLACK);
				display_put_bigtext(4, 13, s"exit", GameSelect == 1 ? BC_BOX_RED : BC_BOX_BLACK);

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

	game_advance(GS_INTRO);

	for(;;)
	{
		game_loop();
	}

#endif
	return 0;
}