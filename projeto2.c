#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <mpi.h>
#include <string.h>
#include <unistd.h>

const int STOP_TAG = 99;
const int SOLUTION_TAG = 100;
const int NO_SOLUTION_TAG = 101;

typedef struct {
    unsigned int colors[4];
    unsigned char rotation;
    unsigned int id;
    int used;
} tile;

typedef struct {
    unsigned int size;
    unsigned int tile_count;
    tile ***board;
    tile *tiles;
} game;

typedef struct {
    unsigned int id;
    unsigned char rotation;
} solution_tile;

#define X_COLOR(t, s) (t->colors[(s + 4 - t->rotation) % 4])
#define N_COLOR(t) (X_COLOR(t, 0))
#define E_COLOR(t) (X_COLOR(t, 1))
#define S_COLOR(t) (X_COLOR(t, 2))
#define W_COLOR(t) (X_COLOR(t, 3))

// Global MPI variables
int mpi_rank, mpi_size;

game *initialize_from_data(unsigned int bsize, tile* tiles_data, int tile_count) {
    game *g = malloc(sizeof(game));
    g->size = bsize;
    g->board = malloc(sizeof(tile**) * bsize);
    for(int i = 0; i < bsize; i++)
        g->board[i] = calloc(bsize, sizeof(tile*));
    g->tile_count = tile_count;
    
    g->tiles = malloc(g->tile_count * sizeof(tile));
    memcpy(g->tiles, tiles_data, g->tile_count * sizeof(tile));
    
    return g;
}

game *initialize(FILE *input) {
    unsigned int bsize;
    unsigned int ncolors;
    int r = fscanf(input, "%u", &bsize);
    assert(r == 1);
    r = fscanf(input, "%u", &ncolors);
    assert(r == 1);
    assert(ncolors < 256);

    game *g = malloc(sizeof(game));
    g->size = bsize;
    g->board = malloc(sizeof(tile**) * bsize);
    for(int i = 0; i < bsize; i++)
        g->board[i] = calloc(bsize, sizeof(tile*));
    g->tile_count = bsize * bsize;

    g->tiles = malloc(g->tile_count * sizeof(tile));
    for (unsigned int i = 0; i < g->tile_count; i++) {
        g->tiles[i].rotation = 0;
        g->tiles[i].id = i;
        g->tiles[i].used = 0;
        for (int c = 0; c < 4; c++) {
            r = fscanf(input, "%u", &g->tiles[i].colors[c]);
            assert(r == 1);
        }
    }
    return g;
}

void free_resources(game *game) {
    free(game->tiles);
    for(unsigned int i = 0; i < game->size; i++)
        free(game->board[i]);
    free(game->board);
    free(game);
}

int valid_move(game *game, unsigned int x, unsigned int y, tile *tile) {
    if (x == 0 && W_COLOR(tile) != 0) return 0;
    if (y == 0 && N_COLOR(tile) != 0) return 0;
    if (x == game->size - 1 && E_COLOR(tile) != 0) return 0;
    if (y == game->size - 1 && S_COLOR(tile) != 0) return 0;
    if (x > 0 && game->board[x - 1][y] != NULL && E_COLOR(game->board[x - 1][y]) != W_COLOR(tile)) return 0;
    if (x < game->size - 1 && game->board[x + 1][y] != NULL && W_COLOR(game->board[x + 1][y]) != E_COLOR(tile)) return 0;
    if (y > 0 && game->board[x][y - 1] != NULL && S_COLOR(game->board[x][y - 1]) != N_COLOR(tile)) return 0;
    if (y < game->size - 1 && game->board[x][y + 1] != NULL && N_COLOR(game->board[x][y + 1]) != S_COLOR(tile)) return 0;
    return 1;
}

int play_parallel_worker(game *game, unsigned int x, unsigned int y) {
    // Workers periodically check if master sent a global stop signal
    static int check_counter = 0;
    if (++check_counter % 1000 == 0) {
        int stop_signal_present = 0;
        MPI_Iprobe(0, STOP_TAG, MPI_COMM_WORLD, &stop_signal_present, MPI_STATUS_IGNORE);
        if (stop_signal_present) {
            // No need to receive, the master will send another one later.
            // Just stop our search.
            return 0;
        }
    }
    
    // Distribute work among workers (ranks 1 to mpi_size-1)
    int is_first_call = (x == 0 && y == 0);
    int start_tile = is_first_call ? (mpi_rank - 1) : 0;
    int step = is_first_call ? (mpi_size - 1) : 1;
    
    for (unsigned int i = start_tile; i < game->tile_count; i += step) {
        if (game->tiles[i].used) continue;
        
        tile *current_tile = &game->tiles[i];
        current_tile->used = 1;
        
        for (int rot = 0; rot < 4; rot++) {
            current_tile->rotation = rot;
            if (valid_move(game, x, y, current_tile)) {
                game->board[x][y] = current_tile;
                
                unsigned int nx, ny;
                // Spiral movement logic
                if (x < game->size - 1 && game->board[x + 1][y] == NULL && (y == 0 || game->board[x][y - 1] != NULL)) {
                    nx = x + 1; ny = y;
                } else if (y < game->size - 1 && game->board[x][y + 1] == NULL) {
                    nx = x; ny = y + 1;
                } else if (x > 0 && game->board[x - 1][y] == NULL) {
                    nx = x - 1; ny = y;
                } else if (y > 0 && game->board[x][y - 1] == NULL) {
                    nx = x; ny = y - 1;
                } else {
                    ny = game->size; // End of board
                }

                if (ny == game->size) {
                    // Solution found! Worker sends it to the master.
                    const int num_tiles = game->size * game->size;
                    solution_tile* solution_payload = malloc(num_tiles * sizeof(solution_tile));
                    
                    int k = 0;
                    for (unsigned int j = 0; j < game->size; j++) {
                        for (unsigned int i_board = 0; i_board < game->size; i_board++) {
                            tile* t = game->board[i_board][j];
                            solution_payload[k].id = t->id;
                            solution_payload[k].rotation = t->rotation;
                            k++;
                        }
                    }
                    
                    MPI_Send(solution_payload, num_tiles * sizeof(solution_tile), MPI_BYTE, 0, SOLUTION_TAG, MPI_COMM_WORLD);
                    free(solution_payload);
                    return 1; // Success
                } else if (play_parallel_worker(game, nx, ny)) {
                    return 1; // Propagate success
                }
                
                game->board[x][y] = NULL; // Backtrack
            }
        }
        current_tile->used = 0; // Backtrack
    }
    return 0; // No path found from this state
}

void master_coordinator(game *g) {
    printf("Master coordinating %d workers...\n", mpi_size - 1);
    
    int workers_finished = 0;
    int solution_received = 0;
    int winner_rank = -1; // New variable to store the winner's rank
    const int num_tiles = g->size * g->size;
    solution_tile* final_solution = malloc(num_tiles * sizeof(solution_tile));
    
    while (workers_finished < (mpi_size - 1)) {
        MPI_Status status;
        MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        
        if (status.MPI_TAG == SOLUTION_TAG) {
            MPI_Recv(final_solution, num_tiles * sizeof(solution_tile), MPI_BYTE, 
                     status.MPI_SOURCE, SOLUTION_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            
            if (!solution_received) {
                solution_received = 1;
                winner_rank = status.MPI_SOURCE; // Store the winner's rank
                
                printf("Solution found by worker (rank %d):\n", winner_rank);
                for (int i = 0; i < num_tiles; i++) {
                    printf("%u %u\n", final_solution[i].id, final_solution[i].rotation);
                }

                printf("Telling other workers to stop searching...\n");
                int stop_flag = 1;
                for (int proc = 1; proc < mpi_size; proc++) {
                    // Send the early stop signal to everyone EXCEPT the winner
                    if (proc != winner_rank) {
                        MPI_Send(&stop_flag, 1, MPI_INT, proc, STOP_TAG, MPI_COMM_WORLD);
                    }
                }
            }
            workers_finished++;
        } 
        else if (status.MPI_TAG == NO_SOLUTION_TAG) {
            int dummy;
            MPI_Recv(&dummy, 1, MPI_INT, status.MPI_SOURCE, NO_SOLUTION_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            workers_finished++;
            printf("Worker %d finished its search. (%d/%d finished)\n", 
                   status.MPI_SOURCE, workers_finished, mpi_size - 1);
        }
    }
    
    int stop_flag = 1;
    if (solution_received) {
        printf("Sending final stop signal to winning worker %d...\n", winner_rank);
        MPI_Send(&stop_flag, 1, MPI_INT, winner_rank, STOP_TAG, MPI_COMM_WORLD);
    } else {
        printf("No solution found. Sending final stop signal to all workers...\n");
        for (int proc = 1; proc < mpi_size; proc++) {
            MPI_Send(&stop_flag, 1, MPI_INT, proc, STOP_TAG, MPI_COMM_WORLD);
        }
        printf("SOLUTION NOT FOUND\n");
    }
    
    free(final_solution);
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

    if (mpi_size < 2) {
        if (mpi_rank == 0) {
            fprintf(stderr, "Error: This program requires at least 2 processes (1 master + 1 worker).\n");
            fprintf(stderr, "Usage: mpirun -np N %s (where N >= 2)\n", argv[0]);
        }
        MPI_Finalize();
        return 1;
    }
    
    game *g;
    
    if (mpi_rank == 0) {
        // Master initializes and distributes data
        if (isatty(fileno(stdin))) {
             fprintf(stderr, "Error: Please provide puzzle input via stdin.\n");
             fprintf(stderr, "Usage: mpirun -np N %s < input_file.txt\n", argv[0]);
             MPI_Abort(MPI_COMM_WORLD, 1);
        }
        
        g = initialize(stdin);
        
        // Broadcast data to all workers
        MPI_Bcast(&g->size, 1, MPI_UNSIGNED, 0, MPI_COMM_WORLD);
        MPI_Bcast(&g->tile_count, 1, MPI_UNSIGNED, 0, MPI_COMM_WORLD);
        MPI_Bcast(g->tiles, g->tile_count * sizeof(tile), MPI_BYTE, 0, MPI_COMM_WORLD);
        
        master_coordinator(g);
        
    } else {
        // Workers receive data and search
        unsigned int bsize, tile_count;
        MPI_Bcast(&bsize, 1, MPI_UNSIGNED, 0, MPI_COMM_WORLD);
        MPI_Bcast(&tile_count, 1, MPI_UNSIGNED, 0, MPI_COMM_WORLD);
        
        tile *tiles_data = malloc(tile_count * sizeof(tile));
        MPI_Bcast(tiles_data, tile_count * sizeof(tile), MPI_BYTE, 0, MPI_COMM_WORLD);
        
        g = initialize_from_data(bsize, tiles_data, tile_count);
        free(tiles_data);
        
        // Worker performs its search
        int found = play_parallel_worker(g, 0, 0);
        
        // If this worker did NOT find a solution, it must notify the master
        // that it has finished its portion of the work.
        if (!found) {
            int no_solution_from_this_worker = 0;
            MPI_Send(&no_solution_from_this_worker, 1, MPI_INT, 0, NO_SOLUTION_TAG, MPI_COMM_WORLD);
        }
        
        // ALL workers must now wait for the master's final instruction.
        // This is a blocking receive that waits for the master to signal that
        // the overall job is done (either by solution or exhaustion).
        int final_stop_signal;
        MPI_Recv(&final_stop_signal, 1, MPI_INT, 0, STOP_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
    
    // Final synchronization barrier to ensure all processes exit cleanly together.
    MPI_Barrier(MPI_COMM_WORLD);
    
    free_resources(g);
    MPI_Finalize();
    return 0;
}