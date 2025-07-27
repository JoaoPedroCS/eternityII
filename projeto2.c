/*
  Seu código original, adaptado para execução paralela com MPI.
  A lógica de busca em espiral foi mantida e é executada pelos trabalhadores.
  A divisão de trabalho é feita na primeira peça do tabuleiro.
  CORREÇÃO: A função valid_move foi completada para garantir que todas as
  soluções encontradas sejam corretas.
*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <mpi.h>

// Tags para a comunicação MPI
const int STOP_TAG = 99;
const int SOLUTION_TAG = 100;
const int NO_SOLUTION_TAG = 101;

// Estruturas de dados
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

// Estrutura simplificada para enviar a solução via MPI
typedef struct {
    unsigned int id;
    unsigned char rotation;
} solution_tile;

// Macros para obter as cores
#define X_COLOR(t, s) (t->colors[(s + 4 - t->rotation) % 4])
#define N_COLOR(t) (X_COLOR(t, 0))
#define E_COLOR(t) (X_COLOR(t, 1))
#define S_COLOR(t) (X_COLOR(t, 2))
#define W_COLOR(t) (X_COLOR(t, 3))

// Variáveis globais do MPI
int mpi_rank, mpi_size;

// Função para os trabalhadores inicializarem o jogo a partir dos dados recebidos
game *initialize_from_data(unsigned int bsize, tile* tiles_data, int tile_count) {
    game *g = malloc(sizeof(game));
    g->size = bsize;
    g->board = malloc(sizeof(tile**) * bsize);
    for(unsigned int i = 0; i < bsize; i++)
        g->board[i] = calloc(bsize, sizeof(tile*));
    g->tile_count = tile_count;
    
    g->tiles = malloc(g->tile_count * sizeof(tile));
    memcpy(g->tiles, tiles_data, g->tile_count * sizeof(tile));
    
    return g;
}

// Função de inicialização original, usada apenas pelo mestre
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
    for(unsigned int i = 0; i < bsize; i++)
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
    if (game == NULL) return;
    if (game->tiles) free(game->tiles);
    if (game->board) {
        for(unsigned int i = 0; i < game->size; i++)
            if (game->board[i]) free(game->board[i]);
        free(game->board);
    }
    free(game);
}

// --- FUNÇÃO valid_move CORRIGIDA E COMPLETA ---
int valid_move(game *game, unsigned int x, unsigned int y, tile *tile) {
    // Verifica as bordas
    if (x == 0 && W_COLOR(tile) != 0) return 0;
    if (y == 0 && N_COLOR(tile) != 0) return 0;
    if (x == game->size - 1 && E_COLOR(tile) != 0) return 0;
    if (y == game->size - 1 && S_COLOR(tile) != 0) return 0;

    // Verifica os vizinhos (agora incluindo direita e baixo)
    if (x > 0 && game->board[x - 1][y] != NULL && E_COLOR(game->board[x - 1][y]) != W_COLOR(tile)) return 0;
    if (y > 0 && game->board[x][y - 1] != NULL && S_COLOR(game->board[x][y - 1]) != N_COLOR(tile)) return 0;
    if (x < game->size - 1 && game->board[x + 1][y] != NULL && W_COLOR(game->board[x + 1][y]) != E_COLOR(tile)) return 0;
    if (y < game->size - 1 && game->board[x][y + 1] != NULL && N_COLOR(game->board[x][y + 1]) != S_COLOR(tile)) return 0;
    
    return 1;
}

void print_solution(solution_tile* solution, unsigned int size) {
    int k = 0;
    for(unsigned int j = 0; j < size; j++) {
        for(unsigned int i = 0; i < size; i++) {
            printf("%u %u\n", solution[k].id, solution[k].rotation);
            k++;
        }
    }
}

// Função de busca recursiva para os trabalhadores
int play_worker(game *game, unsigned int x, unsigned int y, int *stop_flag) {
    if (*stop_flag) {
        return 0;
    }

    static int check_counter = 0;
    if (++check_counter % 2000 == 0) {
        int message_present = 0;
        MPI_Iprobe(0, STOP_TAG, MPI_COMM_WORLD, &message_present, MPI_STATUS_IGNORE);
        if (message_present) {
            *stop_flag = 1;
            return 0;
        }
    }

    int is_first_call = (x == 0 && y == 0);
    unsigned int start_tile = is_first_call ? (mpi_rank - 1) : 0;
    unsigned int step = is_first_call ? (mpi_size - 1) : 1;
    
    for (unsigned int i = start_tile; i < game->tile_count; i += step) {
        if (game->tiles[i].used) continue;
        
        tile *current_tile = &game->tiles[i];
        current_tile->used = 1;
        
        for (int rot = 0; rot < 4; rot++) {
            current_tile->rotation = rot;
            if (valid_move(game, x, y, current_tile)) {
                game->board[x][y] = current_tile;
                
                unsigned int nx, ny;
                if (x < game->size - 1 && game->board[x + 1][y] == NULL && (y == 0 || game->board[x][y - 1] != NULL)) {
                    nx = x + 1; ny = y;
                } else if (y < game->size - 1 && game->board[x][y + 1] == NULL) {
                    nx = x; ny = y + 1;
                } else if (x > 0 && game->board[x - 1][y] == NULL) {
                    nx = x - 1; ny = y;
                } else if (y > 0 && game->board[x][y - 1] == NULL) {
                    nx = x; ny = y - 1;
                } else {
                    ny = game->size;
                }

                if (ny == game->size) {
                    const int num_tiles = game->size * game->size;
                    solution_tile* solution_payload = malloc(num_tiles * sizeof(solution_tile));
                    
                    int k = 0;
                    for (unsigned int j_board = 0; j_board < game->size; j_board++) {
                        for (unsigned int i_board = 0; i_board < game->size; i_board++) {
                            tile* t = game->board[i_board][j_board];
                            solution_payload[k].id = t->id;
                            solution_payload[k].rotation = t->rotation;
                            k++;
                        }
                    }
                    
                    MPI_Send(solution_payload, num_tiles * sizeof(solution_tile), MPI_BYTE, 0, SOLUTION_TAG, MPI_COMM_WORLD);
                    free(solution_payload);
                    return 1;
                } 
                
                if (play_worker(game, nx, ny, stop_flag)) {
                    return 1;
                }
                
                game->board[x][y] = NULL;
            }
        }
        current_tile->used = 0;
    }
    return 0;
}

void master_coordinator(game *g) {
    printf("Mestre coordenando %d trabalhadores...\n", mpi_size - 1);
    
    int workers_finished = 0;
    int solution_found = 0;
    int winner_rank = -1;
    const int num_tiles = g->size * g->size;
    solution_tile* final_solution = malloc(num_tiles * sizeof(solution_tile));
    
    while (workers_finished < (mpi_size - 1)) {
        MPI_Status status;
        MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        
        if (status.MPI_TAG == SOLUTION_TAG) {
            MPI_Recv(final_solution, num_tiles * sizeof(solution_tile), MPI_BYTE, 
                     status.MPI_SOURCE, SOLUTION_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            
            if (!solution_found) {
                solution_found = 1;
                winner_rank = status.MPI_SOURCE;
                printf("SOLUÇÃO ENCONTRADA (pelo trabalhador %d):\n", winner_rank);
                print_solution(final_solution, g->size);

                for (int proc = 1; proc < mpi_size; proc++) {
                    if (proc != winner_rank) {
                        MPI_Send(NULL, 0, MPI_INT, proc, STOP_TAG, MPI_COMM_WORLD);
                    }
                }
            }
            workers_finished++;
        } 
        else if (status.MPI_TAG == NO_SOLUTION_TAG) {
            MPI_Recv(NULL, 0, MPI_INT, status.MPI_SOURCE, NO_SOLUTION_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            workers_finished++;
        }
    }
    
    for (int proc = 1; proc < mpi_size; proc++) {
        MPI_Send(NULL, 0, MPI_INT, proc, STOP_TAG, MPI_COMM_WORLD);
    }

    if (!solution_found) {
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
            fprintf(stderr, "Erro: Este programa requer pelo menos 2 processos (1 mestre + 1 trabalhador).\n");
        }
        MPI_Finalize();
        return 1;
    }
    
    game *g = NULL;
    
    if (mpi_rank == 0) {
        g = initialize(stdin);
        
        MPI_Bcast(&g->size, 1, MPI_UNSIGNED, 0, MPI_COMM_WORLD);
        MPI_Bcast(&g->tile_count, 1, MPI_UNSIGNED, 0, MPI_COMM_WORLD);
        MPI_Bcast(g->tiles, g->tile_count * sizeof(tile), MPI_BYTE, 0, MPI_COMM_WORLD);
        
        master_coordinator(g);
        
    } else {
        unsigned int bsize, tile_count;
        MPI_Bcast(&bsize, 1, MPI_UNSIGNED, 0, MPI_COMM_WORLD);
        MPI_Bcast(&tile_count, 1, MPI_UNSIGNED, 0, MPI_COMM_WORLD);
        
        tile *tiles_data = malloc(tile_count * sizeof(tile));
        MPI_Bcast(tiles_data, tile_count * sizeof(tile), MPI_BYTE, 0, MPI_COMM_WORLD);
        
        g = initialize_from_data(bsize, tiles_data, tile_count);
        free(tiles_data);
        
        int stop_flag = 0;
        if (!play_worker(g, 0, 0, &stop_flag)) {
            MPI_Send(NULL, 0, MPI_INT, 0, NO_SOLUTION_TAG, MPI_COMM_WORLD);
        }
        
        MPI_Recv(NULL, 0, MPI_INT, 0, STOP_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    
    free_resources(g);
    MPI_Finalize();
    return 0;
}
