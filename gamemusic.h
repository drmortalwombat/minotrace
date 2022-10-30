#ifndef GAMEMUSIC_H
#define GAMEMUSIC_H

/*
00-Title-2:33
01-Tune1-2:24
02-Tune2-0:30
03-Tune3-1:00
04-Tune4-1:00
*/

enum Tune
{
	TUNE_MAIN,
	TUNE_GAME_1,
	TUNE_GAME_2,
	TUNE_GAME_3,
	TUNE_GAME_4,
	TUNE_RESTART
};

// Start playback on a tune in the SID file
void music_init(Tune tune);

// Interate music
void music_play(void);

// Disable or enable voice 3 in the SID player
void music_patch_voice3(bool enable);

#pragma compile("gamemusic.c")

#endif
