#include "gamemusic.h"
#include "display.h"
#include <c64/sid.h>


#pragma section( music, 0)

#pragma region( music, 0xa000, 0xb400, , , {music} )

#pragma data(music)

// Embed SID file
__export char music[] = {
	#embed 0x2000 0x7e "minotrace.sid" 
};

#pragma data(data)

// Delay playback interrupt 5/6 for NTSC
char		music_throttle;

void music_init(Tune tune)
{
	__asm
	{
		lda		tune
		jsr		$a000
	}
}

void music_play(void)
{
	// Check NTSC delay
	if (ntsc)
	{
		music_throttle++;
		if (music_throttle == 6)
		{
			music_throttle = 0;
			return;
		}
	}

	__asm
	{
		jsr		$a003
	}
}

void music_patch_voice3(bool enable)
{
	// Patch SID player for voice 3
	*(char *)0xa156 = enable ? 0x20 : 0x4c;
}

