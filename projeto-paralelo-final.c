/*

  * Projeto Eternity II - Versão Sequencial
  *
  * Autores: João Pedro Correa Silva e João Pedro Sousa Bianchim
  * Feito com base nop codigo do professor Emilio Francesquini
  * Data: Outubro de 2023

  Versão final do projeto, paralelizada com MPI.
  Toda a lógica de otimização sequencial foi mantida e distribuída
  entre os processos trabalhadores.
*/
// Oi
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <mpi.h>

// Tags para a comunicação MPI
const int WORK_TAG = 1;
const int STOP_TAG = 2;
const int SOLUTION_TAG = 3;
const int NO_SOLUTION_TAG = 4;

// --- ESTRUTURAS DE DADOS ---
typedef struct {
  unsigned int colors[4];
  unsigned char rotation;
  unsigned int id;
  int used;
} tile;

typedef struct {
    tile **tiles;
    unsigned int count;
    unsigned int capacity;
} tile_list;

typedef struct {
  unsigned int size;
  unsigned int tile_count;
  unsigned int ncolors;
  tile ***board;
  tile *tiles;
  tile_list **color_buckets;
  tile_list *vertex_tiles;
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

// --- DECLARAÇÕES ANTECIPADAS ---
int play(game *game, unsigned int x, unsigned int y, unsigned int required_color, int *stop_flag);
int play_inversa(game *game, unsigned int x, unsigned int y, unsigned int required_color, int *stop_flag);
void add_tile_to_list(tile_list *list, tile *t);
void build_color_buckets(game *g);
void find_vertex_tiles(game *g);

// --- FUNÇÕES DE INICIALIZAÇÃO E LIMPEZA ---
game *initialize_from_data(unsigned int bsize, unsigned int ncolors, tile* tiles_data, int tile_count) {
    game *g = malloc(sizeof(game));
    assert(g != NULL);
    g->ncolors = ncolors;
    g->size = bsize;
    g->tile_count = tile_count;
    g->board = malloc(sizeof(tile**) * bsize);
    for(unsigned int i = 0; i < bsize; i++)
        g->board[i] = calloc(bsize, sizeof(tile*));
    
    g->tiles = malloc(g->tile_count * sizeof(tile));
    memcpy(g->tiles, tiles_data, g->tile_count * sizeof(tile));
    
    build_color_buckets(g);
    find_vertex_tiles(g);
    return g;
}

game *initialize (FILE *input) {
  unsigned int bsize;
  unsigned int ncolors_from_file;
  int r = fscanf (input, "%u", &bsize);
  assert (r == 1);
  r = fscanf (input, "%u", &ncolors_from_file);
  assert (r == 1);
  assert (ncolors_from_file < 256);

  game *g = malloc (sizeof(game));
  assert(g != NULL);
  g->ncolors = ncolors_from_file + 1;
  g->size = bsize;
  g->tile_count = bsize * bsize;
  g->board = malloc (sizeof (tile**) * bsize);
  for(unsigned int i = 0; i < bsize; i++)
    g->board[i] = calloc(bsize, sizeof(tile*));

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

  build_color_buckets(g);
  find_vertex_tiles(g); 
  return g;
}

void free_resources(game *game) {
  if (game == NULL) return;
  if (game->vertex_tiles) {
    if (game->vertex_tiles->tiles) free(game->vertex_tiles->tiles);
    free(game->vertex_tiles);
  }
  if (game->color_buckets) {
    for (unsigned int i = 0; i < game->ncolors; i++) {
      if (game->color_buckets[i]) {
        if (game->color_buckets[i]->tiles) free(game->color_buckets[i]->tiles);
        free(game->color_buckets[i]);
      }
    }
    free(game->color_buckets);
  }
  if (game->tiles) free(game->tiles);
  if (game->board) {
    for(unsigned int i = 0; i < game->size; i++)
      if(game->board[i]) free(game->board[i]);
    free(game->board);
  }
  free(game);
}

void reset_game_state(game *g) {
    for (unsigned int y = 0; y < g->size; y++) {
        for (unsigned int x = 0; x < g->size; x++) {
            g->board[y][x] = NULL;
        }
    }
    for (unsigned int i = 0; i < g->tile_count; i++) {
        g->tiles[i].used = 0;
    }
}

// --- LÓGICA DE VALIDAÇÃO E IMPRESSÃO ---
int valid_move (game *game, unsigned int x, unsigned int y, tile *tile) {
  if (x == 0 && W_COLOR(tile) != 0) return 0;
  if (y == 0 && N_COLOR(tile) != 0) return 0;
  if (x == game->size - 1 && E_COLOR(tile) != 0) return 0;
  if (y == game->size - 1 && S_COLOR(tile) != 0) return 0;
  if (x > 0 && game->board[y][x - 1] != NULL && E_COLOR(game->board[y][x - 1]) != W_COLOR(tile)) return 0;
  if (y > 0 && game->board[y - 1][x] != NULL && S_COLOR(game->board[y - 1][x]) != N_COLOR(tile)) return 0;
  if (x < game->size - 1 && game->board[y][x + 1] != NULL && W_COLOR(game->board[y][x + 1]) != E_COLOR(tile)) return 0;
  if (y < game->size - 1 && game->board[y + 1][x] != NULL && N_COLOR(game->board[y + 1][x]) != S_COLOR(tile)) return 0;
  return 1;
}

void print_solution (solution_tile* solution, unsigned int size) {
    int k = 0;
    for(unsigned int j = 0; j < size; j++) {
        for(unsigned int i = 0; i < size; i++) {
            printf("%u %u\n", solution[k].id, solution[k].rotation);
            k++;
        }
    }
}

// --- FUNÇÕES DE BUSCA RECURSIVA (para os trabalhadores) ---
int play (game *game, unsigned int x, unsigned int y, unsigned int required_color, int *stop_flag) {
  if (*stop_flag) return 0;
  static int check_counter = 0;
  if (++check_counter % 2000 == 0) {
      int message_present = 0;
      MPI_Iprobe(0, STOP_TAG, MPI_COMM_WORLD, &message_present, MPI_STATUS_IGNORE);
      if (message_present) {
          *stop_flag = 1;
          return 0;
      }
  }
  
  tile_list *candidate_list = game->color_buckets[required_color];
  for (unsigned int i = 0; i < candidate_list->count; i++) {
    tile *tile = candidate_list->tiles[i];
    if (tile->used) continue;
    
    tile->used = 1;
    for (int rot = 0; rot < 4; rot++) {
      tile->rotation = rot;
      if (valid_move(game, x, y, tile)) {
        game->board[y][x] = tile;
        unsigned int nx, ny;
        unsigned int next_required_color = 0;
        ny = nx = game->size;

        if (x < game->size - 1 && game->board[y][x + 1] == NULL && (y == 0 || game->board[y - 1][x] != NULL)) {
          nx = x + 1; ny = y;
          next_required_color = E_COLOR(tile);
        } else if (y < game->size - 1 && game->board[y + 1][x] == NULL) {
          nx = x; ny = y + 1;
          next_required_color = S_COLOR(tile);
        } else if (x > 0 && game->board[y][x - 1] == NULL) {
          nx = x - 1; ny = y;
          next_required_color = W_COLOR(tile);
        } else if (y > 0 && game->board[y - 1][x] == NULL) {
          nx = x; ny = y - 1;
          next_required_color = N_COLOR(tile);
        } else {
          ny = game->size;
        }

        if (ny == game->size || play(game, nx, ny, next_required_color, stop_flag)) {
          return 1;
        }
        game->board[y][x] = NULL;
      }
    }
    tile->used = 0;
  }
  return 0;
}

int play_inversa (game *game, unsigned int x, unsigned int y, unsigned int required_color, int *stop_flag) {
  if (*stop_flag) return 0;
  static int check_counter = 0;
  if (++check_counter % 2000 == 0) {
      int message_present = 0;
      MPI_Iprobe(0, STOP_TAG, MPI_COMM_WORLD, &message_present, MPI_STATUS_IGNORE);
      if (message_present) {
          *stop_flag = 1;
          return 0;
      }
  }
  
  tile_list *candidate_list = game->color_buckets[required_color];
  for (unsigned int i = 0; i < candidate_list->count; i++) {
    tile *tile = candidate_list->tiles[i];
    if (tile->used) continue;
    
    tile->used = 1;
    for (int rot = 0; rot < 4; rot++) {
      tile->rotation = rot;
      if (valid_move(game, x, y, tile)) {
        game->board[y][x] = tile;
        unsigned int nx, ny;
        unsigned int next_required_color = 0;
        ny = nx = game->size;

        if (y < game->size - 1 && game->board[y + 1][x] == NULL && (x == 0 || game->board[y][x - 1] != NULL)) {
          nx = x; ny = y + 1;
          next_required_color = S_COLOR(tile);
        } else if (x < game->size - 1 && game->board[y][x + 1] == NULL) {
          nx = x + 1; ny = y;
          next_required_color = E_COLOR(tile);
        } else if (y > 0 && game->board[y - 1][x] == NULL) {
          nx = x; ny = y - 1;
          next_required_color = N_COLOR(tile);
        } else if (x > 0 && game->board[y][x - 1] == NULL) {
          nx = x - 1; ny = y;
          next_required_color = W_COLOR(tile);
        } else {
          ny = game->size;
        }

        if (ny == game->size || play_inversa(game, nx, ny, next_required_color, stop_flag)) {
          return 1;
        }
        game->board[y][x] = NULL;
      }
    }
    tile->used = 0;
  }
  return 0;
}

int play_first(game *g, int vertex_choice, int *stop_flag) {
    tile *start_tile;
    unsigned int x = 0, y = 0;

    if (vertex_choice >= 0 && vertex_choice <= 3) {
        if ((unsigned int)vertex_choice >= g->vertex_tiles->count) return 0;
        start_tile = g->vertex_tiles->tiles[vertex_choice];
        unsigned int nx = 1, ny = 0;
        for (int rot = 0; rot < 4; rot++) {
            start_tile->rotation = rot;
            if (valid_move(g, x, y, start_tile)) {
                g->board[y][x] = start_tile;
                start_tile->used = 1;
                unsigned int next_required_color = E_COLOR(start_tile);
                if (play(g, nx, ny, next_required_color, stop_flag)) return 1;
                g->board[y][x] = NULL;
                start_tile->used = 0;
            }
        }
    } else if (vertex_choice >= 4 && vertex_choice <= 7) {
        int mapped_choice = vertex_choice - 4;
        if ((unsigned int)mapped_choice >= g->vertex_tiles->count) return 0;
        start_tile = g->vertex_tiles->tiles[mapped_choice];
        unsigned int nx = 0, ny = 1;
        for (int rot = 0; rot < 4; rot++) {
            start_tile->rotation = rot;
            if (valid_move(g, x, y, start_tile)) {
                g->board[y][x] = start_tile;
                start_tile->used = 1;
                unsigned int next_required_color = S_COLOR(start_tile);
                if (play_inversa(g, nx, ny, next_required_color, stop_flag)) return 1;
                g->board[y][x] = NULL;
                start_tile->used = 0;
            }
        }
    }
    return 0;
}

// --- LÓGICA DO MESTRE E DOS TRABALHADORES ---
void master_process(game *g, int mpi_size) {
    double start_time, end_time;
    int total_tasks = 8;
    int next_task = 0;
    int workers_finished = 0;
    int solution_found = 0;
    
    MPI_Barrier(MPI_COMM_WORLD);
    start_time = MPI_Wtime();

    // Distribui as tarefas iniciais para os trabalhadores
    for (int rank = 1; rank < mpi_size; rank++) {
        if (next_task < total_tasks) {
            MPI_Send(&next_task, 1, MPI_INT, rank, WORK_TAG, MPI_COMM_WORLD);
            next_task++;
        } else {
            MPI_Send(&next_task, 1, MPI_INT, rank, STOP_TAG, MPI_COMM_WORLD); // Nenhuma tarefa para este
        }
    }

    while (workers_finished < (mpi_size - 1)) {
        MPI_Status status;
        MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        
        if (status.MPI_TAG == SOLUTION_TAG) {
            const int num_tiles = g->size * g->size;
            solution_tile* final_solution = malloc(num_tiles * sizeof(solution_tile));
            MPI_Recv(final_solution, num_tiles * sizeof(solution_tile), MPI_BYTE, status.MPI_SOURCE, SOLUTION_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            
            if (!solution_found) {
                solution_found = 1;
                end_time = MPI_Wtime();
                printf("SOLUÇÃO ENCONTRADA (pelo trabalhador %d):\n", status.MPI_SOURCE);
                print_solution(final_solution, g->size);
                fprintf(stderr, "\nTempo de execução: %f segundos\n", end_time - start_time);
                
                // Manda parar todos os trabalhadores
                for (int rank = 1; rank < mpi_size; rank++) {
                    MPI_Send(&next_task, 1, MPI_INT, rank, STOP_TAG, MPI_COMM_WORLD);
                }
            }
            free(final_solution);
            workers_finished++;

        } else if (status.MPI_TAG == NO_SOLUTION_TAG) {
            int task_completed;
            MPI_Recv(&task_completed, 1, MPI_INT, status.MPI_SOURCE, NO_SOLUTION_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            if (solution_found) {
                // Se a solução já foi encontrada, apenas marca como finalizado
                workers_finished++;
            } else if (next_task < total_tasks) {
                // Se ainda há tarefas, envia a próxima
                MPI_Send(&next_task, 1, MPI_INT, status.MPI_SOURCE, WORK_TAG, MPI_COMM_WORLD);
                next_task++;
            } else {
                // Se não há mais tarefas, manda parar
                MPI_Send(&next_task, 1, MPI_INT, status.MPI_SOURCE, STOP_TAG, MPI_COMM_WORLD);
                workers_finished++;
            }
        }
    }

    if (!solution_found) {
        end_time = MPI_Wtime();
        printf("SOLUTION NOT FOUND\n");
        fprintf(stderr, "\nTempo de execução: %f segundos\n", end_time - start_time);
    }
}

void worker_process(game *g) {
    int stop_flag = 0;
    MPI_Barrier(MPI_COMM_WORLD);

    while(!stop_flag) {
        int task_id;
        MPI_Status status;
        MPI_Recv(&task_id, 1, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        if (status.MPI_TAG == STOP_TAG) {
            stop_flag = 1;
            continue;
        }

        reset_game_state(g);
        if (play_first(g, task_id, &stop_flag)) {
            const int num_tiles = g->size * g->size;
            solution_tile* solution_payload = malloc(num_tiles * sizeof(solution_tile));
            int k = 0;
            for (unsigned int j = 0; j < g->size; j++) {
                for (unsigned int i = 0; i < g->size; i++) {
                    tile* t = g->board[j][i];
                    solution_payload[k].id = t->id;
                    solution_payload[k].rotation = t->rotation;
                    k++;
                }
            }
            MPI_Send(solution_payload, num_tiles * sizeof(solution_tile), MPI_BYTE, 0, SOLUTION_TAG, MPI_COMM_WORLD);
            free(solution_payload);
            stop_flag = 1; // Encontrou, pode parar
        } else {
            MPI_Send(&task_id, 1, MPI_INT, 0, NO_SOLUTION_TAG, MPI_COMM_WORLD);
        }
    }
}


int main (int argc, char **argv) {
  int mpi_rank, mpi_size;
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

  if (mpi_size < 2) {
    if (mpi_rank == 0) fprintf(stderr, "Erro: Este programa requer pelo menos 2 processos.\n");
    MPI_Finalize();
    return 1;
  }
  
  game *g = NULL;
  
  if (mpi_rank == 0) {
      g = initialize(stdin);
      MPI_Bcast(&g->size, 1, MPI_UNSIGNED, 0, MPI_COMM_WORLD);
      MPI_Bcast(&g->ncolors, 1, MPI_UNSIGNED, 0, MPI_COMM_WORLD);
      MPI_Bcast(&g->tile_count, 1, MPI_UNSIGNED, 0, MPI_COMM_WORLD);
      MPI_Bcast(g->tiles, g->tile_count * sizeof(tile), MPI_BYTE, 0, MPI_COMM_WORLD);
      master_process(g, mpi_size);
  } else {
      unsigned int bsize, ncolors, tile_count;
      MPI_Bcast(&bsize, 1, MPI_UNSIGNED, 0, MPI_COMM_WORLD);
      MPI_Bcast(&ncolors, 1, MPI_UNSIGNED, 0, MPI_COMM_WORLD);
      MPI_Bcast(&tile_count, 1, MPI_UNSIGNED, 0, MPI_COMM_WORLD);
      tile *tiles_data = malloc(tile_count * sizeof(tile));
      MPI_Bcast(tiles_data, tile_count * sizeof(tile), MPI_BYTE, 0, MPI_COMM_WORLD);
      g = initialize_from_data(bsize, ncolors, tiles_data, tile_count);
      free(tiles_data);
      worker_process(g);
  }
  
  free_resources(g);
  MPI_Finalize();
  return 0;
}

// --- DEFINIÇÕES DAS FUNÇÕES AUXILIARES ---
void add_tile_to_list(tile_list *list, tile *t) {
  for (unsigned int i = 0; i < list->count; i++) {
    if (list->tiles[i]->id == t->id) return;
  }
  if (list->count >= list->capacity) {
    list->capacity = (list->capacity == 0) ? 4 : list->capacity * 2;
    list->tiles = realloc(list->tiles, list->capacity * sizeof(tile*));
    assert(list->tiles != NULL);
  }
  list->tiles[list->count++] = t;
}

void build_color_buckets(game *g) {
  g->color_buckets = calloc(g->ncolors, sizeof(tile_list*));
  assert(g->color_buckets != NULL);
  for (unsigned int i = 0; i < g->ncolors; i++) {
    g->color_buckets[i] = calloc(1, sizeof(tile_list));
    assert(g->color_buckets[i] != NULL);
  }
  for (unsigned int i = 0; i < g->tile_count; i++) {
    tile *current_tile = &g->tiles[i];
    for (int c = 0; c < 4; c++) {
      unsigned int color = current_tile->colors[c];
      if (color < g->ncolors) {
        add_tile_to_list(g->color_buckets[color], current_tile);
      }
    }
  }
}

void find_vertex_tiles(game *g) {
  g->vertex_tiles = calloc(1, sizeof(tile_list));
  assert(g->vertex_tiles != NULL);
  for (unsigned int i = 0; i < g->tile_count; i++) {
    tile *current_tile = &g->tiles[i];
    int zero_count = 0;
    for (int c = 0; c < 4; c++) {
      if (current_tile->colors[c] == 0) zero_count++;
    }
    if (zero_count == 2) {
      add_tile_to_list(g->vertex_tiles, current_tile);
    }
  }
}
