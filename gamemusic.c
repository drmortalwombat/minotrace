#include "gamemusic.h"
#include <c64/sid.h>


#pragma section( music, 0)

#pragma region( music, 0xa000, 0xc000, , , {music} )

#pragma data(music)

__export char music[] = {
	#embed 0x2000 0x7e "MazeGame.sid" 
};

#pragma data(data)


void music_init(Tune tune)
{
	__asm
	{
		lda		tune
		jsr		$a000
	}
}

void music_queue(Tune tune)
{
}

void music_play(void)
{
	__asm
	{
		jsr		$a003
	}
}

void music_patch_voice3(bool enable)
{
	*(char *)0xa152 = enable ? 0x20 : 0x4c;
}

void music_toggle(void)
{
}
