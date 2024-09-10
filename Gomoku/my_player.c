#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <arpa/inet.h>
#include <time.h>
#include "comms.h"

#define EMPTY -1
#define BLACK 0
#define WHITE 1


#define MAX_MOVES 361

const char *PLAYER_NAME_LOG = "my_player1.log";

void run_master(int, char *[], int);
int initialise_master(int, char *[], int *, int *, FILE **);

void initialise_board(void);
void free_board(void);
void print_board(FILE *);
void reset_board(FILE *);

void run_worker(int);
void update_adjacent(int, int);

int random_strategy(int, FILE *);
int evaluate(int, int);
int minimax(int, int, int, int ,int, int);
void legal_moves(int *, int *);
void make_move(int, int);

int *board;

int BOARD_SIZE;

int main(int argc, char *argv[]) {
	int rank, size;
	
	if (argc != 6) {
		printf("Usage: %s <inetaddress> <port> <time_limit> <player_colour> <board_size>\n", argv[0]);
		return 1;
	}
	
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	/* each process initialises their own board */
	BOARD_SIZE = atoi(argv[5]);
	initialise_board();
	

	if (rank == 0) {
		run_master(argc, argv, size);
	} else {
		run_worker(rank);
	}

	free_board();

	MPI_Finalize();
  return 0;
}

/**
 * Runs the master process.
 * 
 * @param argc command line argument count
 * @param argv command line argument vector
*/
void run_master(int argc, char *argv[], int size) {
	int msg_type, time_limit, my_move, opp_move, running, count, NO_MORE_TASKS, max, TERMINATE, alpha;
	int tot_tasks, my_colour, send[3];
	FILE *fp;
	char *move; 
	int *search;
	int tracking[size];

	//Initial values and Constants
	NO_MORE_TASKS = -5;
	TERMINATE = -123;
	alpha = -10000;

	running = initialise_master(argc, argv, &time_limit, &my_colour, &fp);

	MPI_Bcast(&my_colour, 1, MPI_INT, 0, MPI_COMM_WORLD);

	search = (int *)malloc(BOARD_SIZE * BOARD_SIZE * sizeof(int));
	
    for (int i = 0; i < BOARD_SIZE * BOARD_SIZE; i++) {
		search[i] = -1;
    }
	
	while (running) {
		msg_type = receive_message(&opp_move);
		if (msg_type == GENERATE_MOVE) { /* referee is asking for a move */
			count = 0;

			for (int i = 0; i < BOARD_SIZE * BOARD_SIZE; i++) {
        		if (board[i] == 3) {
					search[count++] = i;
				}
    		}
			count--;
			tot_tasks = count+1;

			if ((search[0]) == -1) {
				my_move = (BOARD_SIZE*BOARD_SIZE)/2;
			} else {
					MPI_Bcast(&NO_MORE_TASKS, 1, MPI_INT, 0, MPI_COMM_WORLD);
					MPI_Bcast(board, BOARD_SIZE * BOARD_SIZE, MPI_INT, 0, MPI_COMM_WORLD);
					
					send[1] = alpha;
					if (count < 15) {
						send[2] = 4;
					} else if (count < 30) {
						send[2] = 3;
					} else {
						send[2] = 2;
					} 
					
					for (int i = 1; i < size; i++) {
						if (count < 0) break;
						send[0] = search[count];
						MPI_Send(&send, 3, MPI_INT, i, 0, MPI_COMM_WORLD);
						tracking[i] = search[count];
						my_move = search[count];
						count--;
    				}
					int results_received = 0;
					max = -10000;
					while (results_received < tot_tasks) { 
						int result;
        				MPI_Status status;

        				MPI_Recv(&result, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status); 
        				results_received++;
        				int worker_rank = status.MPI_SOURCE;
						
						if (result > max) {
							max = result;
							my_move = tracking[worker_rank];
						}

						if (count >= 0) {
							send[0] = search[count];
							send[1] = max;
							MPI_Send(&send, 3, MPI_INT, worker_rank, 0, MPI_COMM_WORLD);
							tracking[worker_rank] = search[count];
            				count--;
        				} else {
							send[0] = -5;
							MPI_Send(&send, 3, MPI_INT, worker_rank, 0, MPI_COMM_WORLD);
						}
					}
			}

			update_adjacent(my_move, my_colour);
			make_move(my_move, my_colour);

			/* convert move to char */
			move = malloc(sizeof(char) * 10);
			sprintf(move, "%d\n", my_move);
			send_move(move);
			free(move);

		} else if (msg_type == PLAY_MOVE) { /* referee is forwarding opponents move */
			fprintf(fp, "\nOpponent placing piece in column: %d, row %d\n", opp_move/BOARD_SIZE, opp_move%BOARD_SIZE);
			make_move(opp_move, (my_colour + 1) % 2);  
			update_adjacent(opp_move, (my_colour + 1) % 2);
		} else if (msg_type == GAME_TERMINATION) { /* reset the board */
			fprintf(fp, "Game terminated.\n");
			fflush(fp);
			running = 0;
			MPI_Bcast(&TERMINATE, 1, MPI_INT, 0, MPI_COMM_WORLD);
		} else if (msg_type == MATCH_RESET) { /* game is over */
			reset_board(fp);
			for (int i = 0; i < BOARD_SIZE * BOARD_SIZE; i++) {
				search[i] = -1;
    		}
		} else if (msg_type == UNKNOWN) {
			fprintf(fp, "Received unknown message type from referee.\n");
			fflush(fp);
			running = 0;
		}

		if (msg_type == GENERATE_MOVE || msg_type == PLAY_MOVE || msg_type == MATCH_RESET) print_board(fp);
	}
	
	free(search);
}

/**
 * Runs the worker process.
 * 
 * @param rank rank of the worker process
*/
void run_worker(int rank) {
	
	int task, my_colour, terminate, alpha, receive[3], result;

	MPI_Bcast(&my_colour, 1, MPI_INT, 0, MPI_COMM_WORLD);
	int opp_colour = (my_colour + 1) % 2;
	
	while (1) {
		task = 0;
		MPI_Bcast(&terminate, 1, MPI_INT, 0, MPI_COMM_WORLD);
		if (terminate == -123) break;
		MPI_Bcast(board, BOARD_SIZE * BOARD_SIZE, MPI_INT, 0, MPI_COMM_WORLD);
		while (1) {
			MPI_Recv(&receive, 3, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			if (receive[0] == -5) break;  
    		
			result = evaluate(receive[0], my_colour);
			if (result < 3000) {
				update_adjacent(receive[0], my_colour);
				result += minimax(receive[2], 0, receive[1], 10000, opp_colour, result);
				remove_adjacent(receive[0]);				
			} else {
				result += 5000;
			}
        	MPI_Send(&result, 1, MPI_INT, 0, 0, MPI_COMM_WORLD); 
		}
		
	}
}


/**
 * Evaluates the board positions using minimax with alpha beta pruning.
 * 
 * @param depth - The depth to search
 * @param turn - Keeps track of which players turn it is
 * @param alpha
 * @param beta
 * @param color - color of the player which turn it is
 * @param cumulative - A cumulative evaluation of the board position
*/
int minimax(int depth, int turn, int alpha, int beta, int color, int cumulative) {
	int value, temp, total;
	int bestValue;
	value = 0;
 	bestValue = -10000;

	if (turn == 1) {
		for (int i = 0; i < BOARD_SIZE*BOARD_SIZE; i++) {
			if (board[i] == 3) {
				temp = evaluate(i, color);
				if (temp >= 3000) return 3000;

				total = cumulative + temp;
			
				if (depth > 0) {
					update_adjacent(i, color);
					value = minimax(depth-1, 0, alpha, beta, (color + 1) % 2, total);	
					remove_adjacent(i);
				} else {
					value = 0;
				}
				temp += value;
				total += value;
			
				if (alpha < total) alpha = total;
				if (bestValue < temp) bestValue = temp;		
				if (alpha >= beta) break;
			}
		}
	} else {
		bestValue = 10000;
		for (int i = 0; i < BOARD_SIZE*BOARD_SIZE; i++) {
			if (board[i] == 3) {
				temp = -evaluate(i, color);
				if (temp <= -3000) return -3000;

				total = cumulative + temp;

				if (depth > 0) {
					update_adjacent(i, color);
					value = minimax(depth-1, 1, alpha, beta, (color + 1) % 2, total);	
					remove_adjacent(i);
				} else {
					value = 0;
				}
				temp += value;
				total += value;
				
				if (beta > total) beta = total;
				if (bestValue > temp) bestValue = temp;
				if (alpha >= beta) break;		
			}
		}
	}
	return bestValue;
}

/**
 * Evaluation function for evaluating a move.
 * 
 * @param pos - the move to evaluate
 * @param my_colour - the color which is has to evaluate
*/
int evaluate(int pos, int my_colour) {
    int score = 0;
    int continuous;
    
	int k = BOARD_SIZE;
	int open_ends = 0;

    // Directions: horizontal, vertical, two diagonals
    int directions[4] = {1, k, k+1, k-1};

    for (int d = 0; d < 4; d++) {
        continuous = 1;  
		open_ends = 0;
		
        for (int i = 1; i < 5; i++) {
            if (pos + i * directions[d] >= k * k || pos % k + i >= k || pos / k + i >= k) break;  
            if (board[pos + i * directions[d]] != my_colour) {
				if (board[pos + i * directions[d]] == 3 || board[pos + i * directions[d]] == EMPTY) open_ends++;
				break;
			} 
            continuous++;
        }
		
        for (int i = 1; i < 5; i++) {
            if (pos - i * directions[d] < 0 || pos % k - i < 0 || pos / k - i < 0) break;  
            if (board[pos - i * directions[d]] != my_colour) {
				if (board[pos - i * directions[d]] == 3 || board[pos - i * directions[d]] == EMPTY) open_ends++;
				break;
			}
            continuous++;
        }
		
      
        if (continuous >= 5) {
            score += 3000; 
        } else if (continuous == 4) {
            if (open_ends == 2) {
                score += 500;  
            } else if (open_ends == 1) {
                score += 100;  
            }
        } else if (continuous == 3) {
            if (open_ends == 2) {
                score += 100;  
            } else if (open_ends == 1) {
                score += 10;  
            }
        } else if (continuous == 2) {
            if (open_ends == 2) {
                score += 10;  
            } else if (open_ends == 1) {
                score += 5;  
            }
        }
    }
    return score;
}


/**
 * Updating the adjacent positions on the board when a move is played
 * 
 * @param new_move
 * @param color 
 *
 * */
void update_adjacent(int new_move, int color) {
    int potential_adjacent[] = {
        new_move - BOARD_SIZE - 1, // Top-left
        new_move - 1,              // Left
        new_move + BOARD_SIZE - 1, // Bottom-left
        new_move + BOARD_SIZE + 1, // Bottom-right
		new_move - BOARD_SIZE + 1, // Top-right
        new_move + 1,              // Right
        new_move - BOARD_SIZE,     // Up
        new_move + BOARD_SIZE      // Down
    };

    board[new_move] = color; 

    for (int i = 0; i < 8; i++) {
        int pos = potential_adjacent[i];

        if (pos >= 0 && pos < BOARD_SIZE * BOARD_SIZE) {
            if (new_move % BOARD_SIZE == 0 && i <= 2) continue;
            if (new_move % BOARD_SIZE == BOARD_SIZE - 1 && i >= 3 && i <= 5) continue;
			if (board[pos] == BLACK) continue;
			if (board[pos] == WHITE) continue;
            board[pos] = 3; // Mark as adjacent
        }
    }
}

/**
 * Removing a move from the board.
 * This is used in minimax when hypothetical moves are evaluated
 * 
 * @param new_move - the position to set to empty
*/
void remove_adjacent(int new_move) {
    int potential_adjacent[] = {
        new_move - BOARD_SIZE - 1, // Top-left
        new_move - 1,              // Left
        new_move + BOARD_SIZE - 1, // Bottom-left
        new_move + BOARD_SIZE + 1, // Bottom-right
		new_move - BOARD_SIZE + 1, // Top-right
        new_move + 1,              // Right
        new_move - BOARD_SIZE,     // Up
        new_move + BOARD_SIZE      // Down
    };

    board[new_move] = 3;

    for (int i = 0; i < 8; i++) {
        int pos = potential_adjacent[i];

        
        if (pos >= 0 && pos < BOARD_SIZE * BOARD_SIZE) {
            // Avoid row wrapping from left to right
            if (new_move % BOARD_SIZE == 0 && i <= 2) continue;
            // Avoid row wrapping from right to left
            if (new_move % BOARD_SIZE == BOARD_SIZE - 1 && i >= 3 && i <= 5) continue;
			if (board[pos] == BLACK) continue;
			if (board[pos] == WHITE) continue;
			int d;
			for (d = 0; d < 8; d++) {
				if (board[potential_adjacent[d] - new_move + pos] == WHITE) break;
				if (board[potential_adjacent[d] + pos - new_move] == BLACK) break;
			}
			if (d == 8) board[pos] = EMPTY;
        }
    }
}


/**
 * Resets the board to the initial state.
 * 
 * @param fp pointer to the log file
*/
void reset_board(FILE *fp) {

	fprintf(fp, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
	fprintf(fp, "~~~~~~~~~~~~~ NEW MATCH ~~~~~~~~~~~~\n");
	fprintf(fp, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
	free_board();
	initialise_board();

	fprintf(fp, "New board state:\n");
}

/**
 * Runs a random strategy. Chooses a random legal move and applies it to the board, then 
 * returns the move in the form of an integer (0-361).
 * 
 * @param my_colour colour of the player
 * @param fp pointer to the log file
*/
int random_strategy(int my_colour, FILE *fp) {
	int number_of_moves = 0;
	int *moves = malloc(sizeof(int) * MAX_MOVES);

	legal_moves(moves, &number_of_moves);

	srand(time(NULL));
	int random_index = rand() % number_of_moves;

	int move = moves[random_index];

	make_move(move, my_colour);

	free(moves);

	fprintf(fp, "\nPlacing piece in column: %d, row: %d \n", move/BOARD_SIZE, move%BOARD_SIZE);
	fflush(fp);

	return move; 
}

/**
 * Applies the given move to the board.
 * 
 * @param move move to apply
 * @param my_colour colour of the player
*/
void make_move(int move, int colour) {
	board[move] = colour;
}

/**
 * Gets a list of legal moves for the current board, and stores them in the moves array followed by a -1.
 * Also stores the number of legal moves in the number_of_moves variable.
 * 
 * @param moves array to store the legal moves in
 * @param number_of_moves variable to store the number of legal moves in
*/
void legal_moves(int *moves, int *number_of_moves) {
	int i, j, k = 0;

	for (i = 0; i < BOARD_SIZE; i++) {
		for (j = 0; j < BOARD_SIZE; j++) {

			if (board[i * BOARD_SIZE + j] == EMPTY) {
				moves[k++] = i * BOARD_SIZE + j;
				(*number_of_moves)++;
			}

		}
	}

	moves[k] = -1;
}

/**
 * Initialises the board for the game.
 */
void initialise_board(void) {
	board = malloc(sizeof(int) * BOARD_SIZE * BOARD_SIZE);
	memset(board, EMPTY, sizeof(int) * BOARD_SIZE * BOARD_SIZE);
}

/**
 * Prints the board to the given file with improved aesthetics.
 * 
 * @param fp pointer to the file to print to
 */
void print_board(FILE *fp) {
	fprintf(fp, "	");

	for (int i = 0; i < BOARD_SIZE; i++) {
		if (i < 9) {
			fprintf(fp, "%d  ", i + 1);
		} else {
			fprintf(fp, "%d ", i + 1);
		}
	}
	fprintf(fp, "\n");

	fprintf(fp, "   +");
	for (int i = 0; i < BOARD_SIZE; i++) {
		fprintf(fp, "--+");
	}
	fprintf(fp, "\n");

	for (int i = 0; i < BOARD_SIZE; i++) {
		fprintf(fp, "%2d |", i + 1);
		for (int j = 0; j < BOARD_SIZE; j++) {
			char piece = '.';
			if (board[i * BOARD_SIZE + j] == BLACK) {
				piece = 'B';
			} else if (board[i * BOARD_SIZE + j] == WHITE) {
				piece = 'W';
			} 
			fprintf(fp, "%c  ", piece);
		}
		fprintf(fp, "|");
		fprintf(fp, "\n");
	}

	fprintf(fp, "   +");
	for (int i = 0; i < BOARD_SIZE; i++) {
		fprintf(fp, "--+");
	}
	fprintf(fp, "\n");

	fflush(fp);
}

/**
 * Frees the memory allocated for the board.
 */
void free_board(void) {
	free(board);
}

/**
 * Initialises the master process for communication with the IF wrapper and set up the log file.
 * @param argc command line argument count
 * @param argv command line argument vector
 * @param time_limit time limit for the game
 * @param my_colour colour of the player
 * @param fp pointer to the log file
 * @return 1 if initialisation was successful, 0 otherwise
 */
int initialise_master(int argc, char *argv[], int *time_limit, int *my_colour, FILE **fp) {
	unsigned long int ip = inet_addr(argv[1]);
	int port = atoi(argv[2]);
	*time_limit = atoi(argv[3]);
	*my_colour = atoi(argv[4]);

	printf("my colour is %d\n", *my_colour);

	/* open file for logging */
	*fp = fopen(PLAYER_NAME_LOG, "w");

	if (*fp == NULL) {
		printf("Could not open log file\n");
		return 0;
	}

	fprintf(*fp, "Initialising communication.\n");

	/* initialise comms to IF wrapper */
	if (!initialise_comms(ip, port)) {
		printf("Could not initialise comms\n");
		return 0;
	}

	fprintf(*fp, "Communication initialised \n");

	fprintf(*fp, "Let the game begin...\n");
	fprintf(*fp, "My name: %s\n", PLAYER_NAME_LOG);
	fprintf(*fp, "My colour: %d\n", *my_colour);
	fprintf(*fp, "Board size: %d\n", BOARD_SIZE);
	fprintf(*fp, "Time limit: %d\n", *time_limit);
	fprintf(*fp, "-----------------------------------\n");
	print_board(*fp);

	fflush(*fp);

  return 1;
}
