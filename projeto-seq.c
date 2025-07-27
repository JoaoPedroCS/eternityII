/*
  * Projeto Eternity II - Versão Sequencial
  * Autores: João Pedro Correa Silva e João Pedro Sousa Bianchim
  * Feito com base nop codigo do professor Emilio Francesquini
  * Data: Julho de 2025
*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>

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
  unsigned int ncolors; // Adicionei o número de cores (+ 1, para a cor 0)
  tile ***board;
  tile *tiles;
  tile_list **color_buckets; // Array de listas por cor
  tile_list *tiles_vertice;   // Lista de peças de vértice (com 2 zeros)
} game;

#define X_COLOR(t, s) (t->colors[(s + 4 - t->rotation) % 4])
#define N_COLOR(t) (X_COLOR(t, 0))
#define E_COLOR(t) (X_COLOR(t, 1))
#define S_COLOR(t) (X_COLOR(t, 2))
#define W_COLOR(t) (X_COLOR(t, 3))

// Adiciona uma peça a uma lista passada deevitando repetir
void add_tile_to_list(tile_list *list, tile *t) {
  for (unsigned int i = 0; i < list->count; i++) {
    if (list->tiles[i]->id == t->id) {
      return;
    }
  }
  if (list->count >= list->capacity) {
    list->capacity = (list->capacity == 0) ? 4 : list->capacity * 2;
    list->tiles = realloc(list->tiles, list->capacity * sizeof(tile*));
    assert(list->tiles != NULL);
  }
  list->tiles[list->count++] = t;
}
// Popula a lista de lista com os tiles correspondentes
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
// Aqui vai ter todas tiles de canto(vertice) que tem 2 cor cinza, se tiver 2 add na lista
void find_vertex_tiles(game *g) {
  g->tiles_vertice = calloc(1, sizeof(tile_list));
  assert(g->tiles_vertice != NULL);

  for (unsigned int i = 0; i < g->tile_count; i++) {
    tile *current_tile = &g->tiles[i];
    int zero_count = 0;
    for (int c = 0; c < 4; c++) {
      if (current_tile->colors[c] == 0) {
        zero_count++;
      }
    }
    if (zero_count == 2) {
      add_tile_to_list(g-, current_tile);
    }
  }
}

game *initialize (FILE *input) {
  unsigned int bsize;
  unsigned int ncolors;
  int r = fscanf (input, "%u", &bsize);
  assert (r == 1);
  r = fscanf (input, "%u", &ncolors);
  assert (r == 1);
  assert (ncolors < 256);

  //creates an empty board
  game *g = malloc (sizeof(game));
  assert(g != NULL);
  g->ncolors = ncolors + 1;
  g->size = bsize;
  g->tile_count = bsize * bsize;
  g->board = malloc (sizeof (tile**) * bsize);
  for(unsigned int i = 0; i < bsize; i++)
    g->board[i] = calloc(bsize, sizeof(tile*));

  //loads tiles
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
  if (game->vertex_tiles) {
    if (game->vertex_tiles->tiles) {
      free(game->vertex_tiles->tiles);
    }
    free(game->vertex_tiles);
  }

  if (game->color_buckets) {
    for (unsigned int i = 0; i < game->ncolors; i++) {
      if (game->color_buckets[i]) {
        if (game->color_buckets[i]->tiles) {
          free(game->color_buckets[i]->tiles);
        }
        free(game->color_buckets[i]);
      }
    }
    free(game->color_buckets);
  }

  free(game->tiles);
  for(unsigned int i = 0; i < game->size; i++)
    free(game->board[i]);
  free(game->board);
  free(game);
}

int valid_move (game *game, unsigned int x, unsigned int y, tile *tile) {
  //The borders must be 0-colored
  if (x == 0 && W_COLOR(tile) != 0) return 0;
  if (y == 0 && N_COLOR(tile) != 0) return 0;
  if (x == game->size - 1 && E_COLOR(tile) != 0) return 0;
  if (y == game->size - 1 && S_COLOR(tile) != 0) return 0;

  //The tile must also be compatible with its existing neighbours
  if (x > 0 && game->board[y][x - 1] != NULL && E_COLOR(game->board[y][x - 1]) != W_COLOR(tile)) return 0;
  if (y > 0 && game->board[y - 1][x] != NULL && S_COLOR(game->board[y - 1][x]) != N_COLOR(tile)) return 0;
  if (x < game->size - 1 && game->board[y][x + 1] != NULL && W_COLOR(game->board[y][x + 1]) != E_COLOR(tile)) return 0;
  if (y < game->size - 1 && game->board[y + 1][x] != NULL && N_COLOR(game->board[y + 1][x]) != S_COLOR(tile)) return 0;

  return 1;
}

void print_solution (game *game) {
  for(unsigned int j = 0; j < game->size; j++)
    for(unsigned int i = 0; i < game->size; i++) {
      tile *t = game->board[j][i];
      printf("%u %u\n", t->id, t->rotation);
    }
}

// função play do professor modificada para seguir espiral
int play (game *game, unsigned int x, unsigned int y, unsigned int required_color) {
  
  // Usa a cor recebida para pegar a lista de candidatos
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

        // Determina a próxima posição E a próxima cor necessária
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

        if (ny == game->size || play(game, nx, ny, next_required_color)) {
          return 1;
        }
        game->board[y][x] = NULL;
      }
    }
    tile->used = 0;
  }

  return 0;
}

// --- NOVA FUNÇÃO PLAY INVERSA ---
int play_inversa (game *game, unsigned int x, unsigned int y, unsigned int required_color) {
  
  // Usa a cor recebida para pegar a lista de candidatos
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

        // Determina a próxima posição E a próxima cor necessária (Espiral Inversa: Baixo -> Direita -> Cima -> Esquerda)
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

        if (ny == game->size || play_inversa(game, nx, ny, next_required_color)) {
          return 1;
        }
        game->board[y][x] = NULL;
      }
    }
    tile->used = 0;
  }

  return 0;
}

// Tenta resolver o tabuleiro começando com uma peça de vértice específica,
// sempre na posição (0,0).
int play_first(game *g, int vertex_choice) {
    
    tile *start_tile;
    unsigned int x = 0, y = 0; // Posição inicial sempre (0,0)

    // Lógica para chamar a busca normal ou a inversa
    if (vertex_choice >= 0 && vertex_choice <= 3) {
        if ((unsigned int)vertex_choice >= g-->count) {
            fprintf(stderr, "Escolha de vértice (%d) inválida. Tente um número menor.\n", vertex_choice);
            return 0;
        }
        start_tile = g-->tiles[vertex_choice];
        
        unsigned int nx = 1, ny = 0; // Próxima posição para a espiral normal

        for (int rot = 0; rot < 4; rot++) {
            start_tile->rotation = rot;
            if (valid_move(g, x, y, start_tile)) {
                g->board[y][x] = start_tile;
                start_tile->used = 1;
                
                unsigned int next_required_color = E_COLOR(start_tile);
                if (play(g, nx, ny, next_required_color)) {
                    return 1;
                }
                
                g->board[y][x] = NULL;
                start_tile->used = 0;
            }
        }
    } else if (vertex_choice >= 4 && vertex_choice <= 7) {
        int mapped_choice = vertex_choice - 4;
        if ((unsigned int)mapped_choice >= g-->count) {
            fprintf(stderr, "Escolha de vértice (%d) inválida. Tente um número menor.\n", vertex_choice);
            return 0;
        }
        start_tile = g-->tiles[mapped_choice];

        unsigned int nx = 0, ny = 1; // Próxima posição para a espiral inversa

        for (int rot = 0; rot < 4; rot++) {
            start_tile->rotation = rot;
            if (valid_move(g, x, y, start_tile)) {
                g->board[y][x] = start_tile;
                start_tile->used = 1;
                
                unsigned int next_required_color = S_COLOR(start_tile);
                if (play_inversa(g, nx, ny, next_required_color)) {
                    return 1;
                }
                
                g->board[y][x] = NULL;
                start_tile->used = 0;
            }
        }
    } else {
        fprintf(stderr, "Escolha de vértice (%d) fora do intervalo válido (0-7).\n", vertex_choice);
        return 0;
    }

    return 0; // Nenhuma rotação da peça inicial levou a uma solução
}

int main (int argc, char **argv) {
  clock_t start_time, end_time;
  double cpu_time_used;
  start_time = clock();

  game *g = initialize(stdin);

  int initial_vertex_choice = 0; 
  if (play_first(g, initial_vertex_choice)) {
    printf("SOLUÇÃO ENCONTRADA (iniciando com a peça de vértice de índice %d):\n", initial_vertex_choice);
    print_solution(g);
  } else {
    printf("SOLUTION NOT FOUND (iniciando com a peça de vértice de índice %d)\n", initial_vertex_choice);
  }

  end_time = clock();
  cpu_time_used = ((double) (end_time - start_time)) / CLOCKS_PER_SEC;
  printf("Execution time: %f seconds\n", cpu_time_used);

  free_resources(g);
  return 0;
}