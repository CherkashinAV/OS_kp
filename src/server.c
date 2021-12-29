#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <semaphore.h>
#include <stdbool.h>
#include <pthread.h>
#include "game.h"
#include "Message.h"
#define MAX_PATH_NAME_SIZE 128
#define MAX_GAME_NAME_SIZE 32
#define MAX_GAMES_COUNT    10
#define MAX_PLAYERS_COUNT  32

static sem_t phore;

void server_init(){
	if (sem_init(&phore, 1, 1) < 0)
		perror("Semaphore ERROR!");
}

void lock(){
	sem_wait(&phore);
}

void unlock(){
	sem_post(&phore);
}

void to_string(char* buf, int x){
	char num[24];
	int i = 0;
	if (x < 0){
		num[i++] = '-';
		x = -x;
	}
	do{
		num[i++] = x % 10 + '0';
		x /= 10;
	} while (x > 0);
	while(--i >= 0)
		*buf++ = num[i];
	*buf = 0;
}

typedef struct{
	char path_sr[MAX_PATH_NAME_SIZE];
	char path_sw[MAX_PATH_NAME_SIZE];
} pipe_lines;

void pl_init(pipe_lines *pl, int num){
	strcpy(pl->path_sr, "/tmp/bulls_and_cows_sr");
	to_string(pl->path_sr + 22, num);
	strcpy(pl->path_sw, "/tmp/bulls_and_cows_sw");
	to_string(pl->path_sw + 22, num);
}

static game_t *games[MAX_GAMES_COUNT];
static player_t *players[MAX_PLAYERS_COUNT];
static int players_count = 0;
static int games_count = 0;

static int list_reply(char *rep){
	int len = 0;
	int i = 0;
	game_t **g = games;
	int l;
	lock();
	int gc = games_count;
	printf("Games_count %d\n", gc);
	if(gc == 0){
		len = sprintf(rep, "NO GAMES AVAILABLE\n");
		unlock();
		return len;
	}
	do{
		if(i++ >= MAX_GAMES_COUNT)
			break;
		if(*g == NULL){
			g++;
			continue;
		}
		l = sprintf(rep, "%s[%d\\%d]\t", (*g)->name, (*g)->must_players, (*g)->players_count);
		g++;
		rep += l;
		len += l;
	} while(--gc > 0);
	unlock();
	*(--rep) = '\n';
	return len;
}

static int print_reply(char *rep){
	game_t **g = games;
	lock();
	int gc = 0;
	int i = 0;
	printf("total games: %d\n", games_count);
	do{
		if(i++ >= MAX_GAMES_COUNT)
			break;
		if(*g == NULL){
			g++;
			continue;
		}
		printf("%s[%d\\%d] : '%s' %s active pl : %d\n\t[", (*g)->name, (*g)->must_players, (*g)->players_count, 
			(*g)->mystery_num, (*g)->winner_idx < 0 ? "" : ": completed", (*g)->active_player);
		for(int j = 0; j < (*g)->must_players; j++){
			if((*g)->players[j] == NULL)
				printf("NULL ");
			else
				printf("%d ", (*g)->players[j]->user_id);
		}
		printf("]\n");
		g++;
		gc++;
	} while(true);
	printf("games found: %d\n", gc);
	unlock();
	*rep = 0;
	return 0;
}

int new_game_serv(char *name, int must_players, player_t *first_player){
	game_t *g = NULL;
	static int ind = 0;
	int rvl = -1;
	int i;
	g = new_game(name, must_players, first_player);
	if(g == NULL)
		return -1;
	lock();
	for(i = 0; i < MAX_GAMES_COUNT; i++){
		if(games[(i + ind) % MAX_GAMES_COUNT] == NULL){
			rvl = (i + ind) % MAX_GAMES_COUNT;
			games[rvl] = g;
			games_count++;
			ind = (i + ind + 1) % MAX_GAMES_COUNT;
			break;
		}
	}
	unlock();
	return rvl;
}

void remove_game(int ind){
	if(ind >= MAX_GAMES_COUNT || ind < 0)
		return;
	if(games[ind] != NULL){
		games[ind] = NULL;
		games_count--;
	}
}

int add_player(player_t *p){
	static int ind = 0;
	int rvl = -1;
	int i;
	if(p == NULL)
		return -1;
	lock();
	for(i = 0; i < MAX_PLAYERS_COUNT; i++){
		if(players[(i + ind) % MAX_PLAYERS_COUNT] == NULL){
			rvl = (i + ind) % MAX_PLAYERS_COUNT;
			players[rvl] = p;
			players_count++;
			ind = (i + ind + 1) % MAX_PLAYERS_COUNT;
			break;
		}
	}
	unlock();
	p->user_id = rvl;
	return rvl;
}

void remove_player(int ind){
	if(ind >= MAX_PLAYERS_COUNT || ind < 0)
		return;
	lock();
	if(players[ind] != NULL){
		players[ind] = NULL;
		players_count--;
	}
	unlock();
}

static void *client_thread(void *arg){
	int game_ind = -1;
	game_t *game = NULL;
	player_t *player = (player_t *)arg;
	int my_ind = player->user_id;
	char req[MAX_REQUEST_SIZE];
	char rep[MAX_REPLY_SIZE];
	int fd_r = player->fd_r;
	int fd_w = player->fd_w;
	for(;;){
		read_str(fd_r, req, MAX_REQUEST_SIZE);
		if(*req == 'e'){
			printf("Exited\n");
			remove_player(my_ind);
			free(player);
			return NULL;
		}
		if(strcmp(req, "ping") == 0){
			write_msg(fd_w, "pong\n", 5);
			continue;
		}
		if(*req == 'c'){
			if(game != NULL){
FAIL:
				write_msg(fd_w, "!\n", 2);
				continue;
			}
			char name[MAX_GAME_NAME_SIZE];
			name[0] = 0;
			int max_players = -1;
			sscanf(req + 1, "%d*%s", &max_players, name);
			if(max_players <= 0)
				goto FAIL;
			game_ind = new_game_serv(name, max_players, player);
			if(game_ind == -1)
				goto FAIL;
			game = games[game_ind];
			write_msg(fd_w, game->name, strlen(game->name));
			write_msg(fd_w, "\n", 1);
			continue;
		}
		if(isdigit(*req)){
			int bulls = 0;
			int cows = 0;
			int num = atoi(req);
			printf("game = %s [%d\\%d] num: %d\n", game->name, game->must_players, game->players_count, num);
			if(game == NULL || !active_game(game))
				goto FAIL;
			if(game->players[game->active_player] != player)
				goto FAIL;
			if(!ok_number(num))
				goto FAIL;
			bulls_and_cows(game, num, &bulls, &cows);
			int len = sprintf(rep, "%d%d\n", bulls, cows);
			game->active_player = (game->active_player + 1) % game->must_players;
			write_msg(fd_w, rep, len);
			if (bulls >= WIN_BULLS){
				game->winner_idx = my_ind;
			}
			continue;
		}
		if(*req == 'l'){
			printf("OK\n");
			int list_len = list_reply(rep);
			printf("list reply: [%s]\n", rep);
			write_msg(fd_w, rep, list_len);
			continue;
		}
		if(*req == 'p'){
			int print_len = print_reply(rep);
			print_len += sprintf(rep + print_len, "\rplayer id: %d game ind: %d game: %s\n", my_ind, game_ind, game ? game->name : "NULL");
			write_msg(fd_w, rep, print_len);
			continue;
		}
		if(*req == 'j'){
			char *p = req + 1;
			if(game != NULL)
				goto FAIL;
			lock();
			for(int i = 0; i < MAX_GAMES_COUNT; i++){
				if(games[i] == NULL)
					continue;
				if(active_game(games[i]))
					continue;
				if(*p == 0 || strcmp(p, games[i]->name) == 0){
					games[i]->players[games[i]->players_count++] = player;
					game = games[i];
					game_ind = i;
					break;
				}
			}
			unlock();
			if(game == NULL)
				goto FAIL;
			int len = sprintf(rep, "%s %d %d\n", game->name, game->must_players, game->players_count);
			write_msg(fd_w, rep, len);
			continue;
		}
		if(*req == 'w'){
			int len;
			if(active_game(game)){
				if(game->winner_idx >= 0){
					if(game->winner_idx == my_ind)
						len = sprintf(rep, "w%d\n", game->must_players);
					else 
						len = sprintf(rep, "l%d\n", game->must_players);
				}
				else
					if(game->players[game->active_player] == player){
						len = sprintf(rep, "r%d\n", game->must_players);
					}
					else 
						len = sprintf(rep, "t%d\n", game->must_players);
			}
			else 
				len = sprintf(rep, "p%d\n", game->players_count);
			write_msg(fd_w, rep, len);
			continue;
		}
		if(*req == 'q'){
			if(game == NULL)
				goto FAIL;
			lock();
			for(int i = 0; i < game->must_players; i++){
				if(game->players[i] == player){
					game->players[i] = NULL;
					game->players_count--;
				}
			}
			if(game->players_count <= 0){
				if (game_ind < 0 || game_ind >= MAX_GAMES_COUNT)
					printf("game_ind = %d\n", game_ind);
				else if(games[game_ind] == NULL)
					printf("NULL ind\n");
				else
					printf("%d.Gamename: %s(%s) PC: %d\n", game_ind, games[game_ind]->name, game->name, game->players_count);
				remove_game(game_ind);
				free(game);
			}
			game_ind = -1;
			game = NULL;
			unlock();
			write_msg(fd_w, "ok\n", 3);
			continue;
		}
	}
}

void client_exchange(int fd_r, int fd_w){
	player_t *player = malloc(sizeof(player_t));
	player->fd_r = fd_r;
	player->fd_w = fd_w;
	int idx = add_player(player);
	printf("IDX: %d\n", idx);
	pthread_create(&player->tid, NULL, &client_thread, player);
	printf("THREAD STARTED\n");
}

void server_thread_start(pipe_lines pl){
	int fd_r = -1;
	int fd_w = -1;
	if(mknod(pl.path_sw, S_IFIFO|S_IWUSR|S_IWOTH|S_IRUSR|S_IROTH, 0) < 0){
		perror("mknod ERROR!");
		return;
	}
	if(mknod(pl.path_sr, S_IFIFO|S_IWUSR|S_IWOTH|S_IRUSR|S_IROTH, 0) < 0){
		perror("mknod ERROR!");
		return;
	}
	printf("nodes created\n");
	if((fd_r = open(pl.path_sr, O_RDONLY)) < 0){
		perror("Can't open pipe for reading");
		return;
	}
	if((fd_w = open(pl.path_sw, O_WRONLY)) < 0){
		perror("Can't open pipe for reading");
		return;
	}
	printf("game %s %s\n", pl.path_sw, pl.path_sr);
	client_exchange(fd_r, fd_w);
}

int main(){
	server_init();
	int pipe_id = 1;
	pipe_lines connection_pl;
	pl_init(&connection_pl, 0);
	if(mknod(connection_pl.path_sw, S_IFIFO|S_IWUSR|S_IWOTH|S_IRUSR|S_IROTH, 0) < 0)
			perror("mknod ERROR!");
	for(;;){
		int fd_w;
		if((fd_w = open(connection_pl.path_sw, O_WRONLY)) < 0)
			perror("Can't open pipe for writing");
		pipe_lines client_pl;
		pl_init(&client_pl, pipe_id);
		write(fd_w, client_pl.path_sr, strlen(client_pl.path_sr));
		write(fd_w, "\n", 1);
		write(fd_w, client_pl.path_sw, strlen(client_pl.path_sw));
		printf("%s\n", client_pl.path_sr);
		write(fd_w, "\n", 1);
		server_thread_start(client_pl);
		sleep(1);
		close(fd_w);
		pipe_id++;
	}
	remove(connection_pl.path_sw);
	return 0;
}