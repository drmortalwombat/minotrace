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
};

void music_init(Tune tune);

void music_queue(Tune tune);

void music_play(void);

void music_toggle(void);

void music_patch_voice3(bool enable);

#pragma compile("gamemusic.c")

#endif
