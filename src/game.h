#ifndef __GAME_H_
#define __GAME_H_

#include <time.h>
#include <stdbool.h>
#include <ctype.h>
#include <pthread.h>

#define MAX_GAME_NAME_SIZE 32
#define WIN_BULLS 		   4

typedef struct{
	int fd_r;
	int fd_w;
	int user_id;
	pthread_t tid;
} player_t;

typedef struct{
	int winner_idx;
	char name[MAX_GAME_NAME_SIZE];
	char mystery_num[WIN_BULLS + 1];
	int must_players;
	int players_count;
	int active_player;
	player_t *players[1];
} game_t;

static inline bool active_game(game_t *g){
	return g->players_count >= g->must_players;
}

game_t *new_game(char *name, int must_players, player_t *first_player);
bool add_player_to_game(game_t *g, player_t *p);
bool ok_number_str(char *str);
bool ok_number(int num);

void bulls_and_cows(game_t *g, int num, int *bulls, int *cows);

#endif