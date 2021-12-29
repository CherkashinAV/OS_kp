#include "game.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

static void generate_mystery_num(char *num){
	int digit;
	static bool first = true;
	if(first){
		srand(time(NULL));
		first = false;
	}
	for (int i = 0; i < WIN_BULLS; i++){
		do{
			digit = (rand() % 10) + '0';
			for (int j = 0; j < i; j++){
				if(num[j] == digit){
					digit = 0;
					break;
				}
			}
		} while(digit == 0);
		num[i] = digit;
	}
}

game_t *new_game(char *name, int must_players, player_t *first_player){
	static int game_number = 0;
	game_t *g = malloc(sizeof(game_t) + sizeof(player_t *) * (must_players - 1));
	if (g == NULL){
		perror("malloc ERROR!");
		return NULL;
	}
	if(name == NULL || strcmp(name, "") == 0)
		sprintf(g->name, "*game%d", game_number++);
	else
		strcpy(g->name, name);
	g->active_player = 0;
	g->must_players = must_players;
	g->players[0] = first_player;
	g->players_count = 1;
	g->winner_idx = -1;
	generate_mystery_num(g->mystery_num);
	return g;
}

bool add_player_to_game(game_t *g, player_t *p){
	if(g == NULL || p == NULL)
		return false;
	if(active_game(g))
		return false;
	g->players[g->players_count++] = p;
	return true;
}

void bulls_and_cows(game_t *g, int num, int *bulls, int *cows){
	char digit;
	int b = 0;
	int c = 0;
	if(g == NULL)
		goto BALLOUT;
	for(int i = (WIN_BULLS - 1); i >= 0; i--){
		digit = (num % 10) + '0';
		num /= 10;
		for(int j = 0; j < WIN_BULLS; j++){
			if(g->mystery_num[j] == digit){
				c++;
				break;
			}
		}
		if(digit == g->mystery_num[i])
			b++;
	}
BALLOUT:
	printf("mystery: %s b: %d c %d\n", g->mystery_num, b, c);
	if(bulls != NULL)
		*bulls = b;
	if(cows != NULL)
		*cows = c - b;
}

bool ok_number_str(char *str){
	bool in_use[10];
	int i;
	for(i = 0; i < 10; i++)
		in_use[i] = false;
	i = 0;
	while(*str != 0){
		if(!isdigit(*str))
			return false;
		if (in_use[*str - '0'] == true)
			return false;
		in_use[*str - '0'] = true;
		i++;
		str++;
		if(i > 4)
			return false;
	}
	return true;
}

bool ok_number(int num){
	char buf[16];
	if (snprintf(buf, sizeof(buf), "%04d", num) != 4){
		printf("IN COUNT\n");
		return false;
	}
	return ok_number_str(buf);
}

#ifdef BC_TEST
int main(){
	game_t *g = new_game("abobus", 1, NULL);
	printf("%s\n", g->name);
	strcpy(g->mystery_num, "1234");
	int bulls;
	int cows;
	bulls_and_cows(g, 1234, &bulls, &cows);
	printf("%s: %d %d %d must be 4 0\n", g->mystery_num, 1234, bulls, cows);
	bulls_and_cows(g, 1324, &bulls, &cows);
	printf("%s: %d %d %d must be 2 2\n", g->mystery_num, 1324, bulls, cows);
	bulls_and_cows(g, 2504, &bulls, &cows);
	printf("%s: %d %d %d must be 1 1\n", g->mystery_num, 2504, bulls, cows);
	free(g);
	return 0;
}
#endif //BC_TEST