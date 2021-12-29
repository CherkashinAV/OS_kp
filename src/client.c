#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>
#include "Message.h"
#include "game.h"

#define MAX_PATH_NAME_SIZE 128
#define MAX_CMD_SIZE 	   64 
#define MAX_GAME_NAME_SIZE 32

char *skip_separator(char *str){
	if(str == NULL)
		return NULL;
	while(*str != 0){
		if(*str == ' ' || *str == '\t'){
			str++;
		}
		else 
			break;
	}
	return str;
}

char *search_separator(char *str){
	if(str == NULL)
		return NULL;
	while(*str != 0){
		if(*str != ' ' && *str != '\t'){
			str++;
		}
		else 
			break;
	}
	return str;
}

char wait_request(player_t *p, int *n){
	int num = -1;
	char rep[32];
	write_msg(p->fd_w, "w\n", 2);
	read_str(p->fd_r, rep, 32);
	switch(rep[0]){
		case 'p':
		case 't':
		case 'r':
		case 'w':
		case 'l':
			num = atoi(rep + 1);
			break; 
		default:
			rep[0] = 'r';
			break;
	}
	if (n != NULL)
		*n = num;
	return rep[0];
}

void erase(int n){
	while(n-- > 0){
		printf("\b \b");
	}
	fflush(stdout);
}

int wait_for_turn(game_t * game){
	player_t * p = game->players[0];
	int num = -1;
	int len = 0;
	char w = ' ';
	do{
		sleep(1);
		w = wait_request(p, &num);
		erase(len);
		if(w == 'p')
			len = printf("%s", "Waiting for players...");
		else if(w == 't')
			len = printf("%s", "Waiting for your turn...");
		fflush(stdout);
	} while(w != 'r' && w != 'l' && w != 'w');
	erase(len);
	if(w == 'l' || w == 'w'){
		w == 'l' ? printf("You are looser! Hehe\n") : printf("You are the WINNER!!!\n");
		game->winner_idx = w == 'w' ? 0 : 1;
	}
	return num;
}

static void print_prompter(game_t *g){
	if(g == NULL){
		printf("> ");
		fflush(stdout);
		return;
	}
	if(g->winner_idx < 0){
		int n = wait_for_turn(g);
		g->must_players = n;
		g->players_count = n;
	}
	printf("%s%s%c ", g->name, g->winner_idx < 0 ? "" : g->winner_idx == 0 ? " winner" : " loser", active_game(g) ? '#' : '>');
	fflush(stdout);
}

static game_t *create_game_message(player_t *player, char *name, int max_players){
	int num;
	char msg[MAX_REQUEST_SIZE];
	char rep[MAX_REPLY_SIZE];
	int len = snprintf(msg, MAX_REQUEST_SIZE, "c%d*%s\n", max_players, name == NULL ? "" : name);
	write_msg(player->fd_w, msg, len);
	read_str(player->fd_r, rep, MAX_REPLY_SIZE);
	if(*rep == '!' || *rep == 0)
		return NULL;
	len = 0;
	game_t *game = new_game(rep, max_players, player);
	num = wait_for_turn(game);
	game->players_count = num;
	game->must_players = max_players;
	return game;
}

static game_t *join_message(player_t *player, char *name){
	char server_game_name[MAX_GAME_NAME_SIZE];
	char msg[MAX_REQUEST_SIZE];
	char rep[MAX_REPLY_SIZE];
	int server_max_players = -1;
	int server_active_players = -1;
	int len = snprintf(msg, MAX_REQUEST_SIZE, "j%s\n", name == NULL ? "" : name);
	write_msg(player->fd_w, msg, len);
	read_str(player->fd_r, rep, MAX_REPLY_SIZE);
	if(*rep == '!')
		return NULL;
	sscanf(rep, "%s %d %d", server_game_name, &server_max_players, &server_active_players);
	if(*server_game_name == 0)
		return NULL;
	printf("%s %d %d\n", server_game_name, server_max_players, server_active_players);
	if(server_game_name[0] == 0 || server_max_players < 1 || server_active_players < 1)
		return NULL;
	game_t *game = new_game(server_game_name, server_max_players, player);
	int num = wait_for_turn(game);
	game->players_count = num;
	game->must_players = num;
	return game;
}

void process_cmd(int fd_r, int fd_w){
	player_t player;
	player.fd_r = fd_r;
	player.fd_w = fd_w;
	game_t *game = NULL;
	char cmd[MAX_CMD_SIZE];
	char rep[MAX_REPLY_SIZE];
	char req[MAX_REQUEST_SIZE];
	for(;;){
		print_prompter(game);
		read_str(0, cmd, MAX_CMD_SIZE);
		if(strcmp(cmd,"exit") == 0 ){
			if(game != NULL){
GAME_IN_PROGRESS:
				puts("You can't leave this game! Play to over!\n");
				continue;
			}
			write_msg(fd_w, "e\n", 5);
			return;
		}
		if(strcmp(cmd, "ping") == 0){
			write_msg(fd_w, "ping\n", 5);
			read_str(fd_r, rep, MAX_REPLY_SIZE);
			continue;
		}
		if(strncmp(cmd, "create ", 7) == 0){
			char *name = NULL;
			int max_players = 1;
			if(game != NULL)
				goto GAME_IN_PROGRESS;
			char *p = cmd + 7;
			p = skip_separator(p);
			if(*p != 0){
				char *new_p = search_separator(p);
				max_players = atoi(p);
				if(*new_p != 0){
					p = skip_separator(new_p);
					if(*p != 0){
						name = p;
						p = search_separator(p);
						*p = 0;
					}
				}
			}
			if(max_players <= 0)
				max_players = 1;
			if(name != NULL && *name == '*'){
BAD_COMMAND:
				puts("Bad command!");
				continue;
			}
			game = create_game_message(&player, name, max_players);
			continue;
		}
		if(strncmp(cmd, "join", 4) == 0){
			if(game != NULL)
				goto GAME_IN_PROGRESS;
			char *name = NULL;
			char *p = cmd + 4;
			p = skip_separator(p);
			if(*p != 0){
				char *new_p = search_separator(p);
				*new_p = 0;
				name = p;
			}
			game = join_message(&player, name);
			continue;
		}
		if(isdigit(*cmd)){
			write_msg(fd_w, cmd, strlen(cmd));
			write_msg(fd_w, "\n", 1);
			read_str(fd_r, rep, MAX_REPLY_SIZE);
			if(*rep == '!'){
				goto BAD_COMMAND;
			}
			printf("bulls: %c, cows: %c%s\n", rep[0], rep[1], rep[0] == (WIN_BULLS + '0') ? "\nCongratulations!\n" : "");
			continue;
		}
		if(cmd[0] == 'l'){
			write_msg(fd_w, "l\n", 2);
			read_str(fd_r, rep, MAX_REPLY_SIZE);
			char *p = rep;
			while(*p != 0){
				if(*p == '\t')
					*p = '\n';
				p++;
			}
			printf("%s\n", rep);
			continue;
		}
		if(cmd[0] == 'p'){
			write_msg(fd_w, "p\n", 2);
			read_str(fd_r, rep, MAX_REPLY_SIZE);
			char *p = rep;
			while(*p != 0){
				if(*p == '\r')
					*p = '\n';
				p++;
			}
			printf("%s\n", rep);
			continue;
		}
		if(*cmd == 'q' && game != NULL && game->winner_idx >= 0){
			write_msg(fd_w, "q\n", 2);
			read_str(fd_r, rep, MAX_REPLY_SIZE);
			if(*rep == '!')
				goto BAD_COMMAND;
			free(game);
			game = NULL;
		}
	}
}

int main (){
	int fd_r = -1;
	int fd_w = -1;
	if((fd_r = open("/tmp/bulls_and_cows_sw0", O_RDONLY)) < 0){
		perror("Can't open pipe for reading");
		goto BALLOUT;
	}
	char pl_r[MAX_PATH_NAME_SIZE];
	char pl_w[MAX_PATH_NAME_SIZE];
	read_str(fd_r, pl_w, MAX_PATH_NAME_SIZE);
	read_str(fd_r, pl_r, MAX_PATH_NAME_SIZE);
	close(fd_r);
	if((fd_w = open(pl_w, O_WRONLY)) < 0){
		perror("Can't open pipe for writing");
		goto BALLOUT;
	}
	if((fd_r = open(pl_r, O_RDONLY)) < 0){
		perror("Can't open pipe for reading");
		goto BALLOUT;
	}
	process_cmd(fd_r, fd_w);
BALLOUT:
	if(fd_r >= 0)
		close(fd_r);
	if(fd_w >= 0)
		close(fd_w);
	return 0;
}