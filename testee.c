/*
  Emilio Francesquini
  e.francesquini@ufabc.edu.br
  2020.Q1
  CC-BY-SA 4.0

  Modificado para incluir e popular uma "lista de listas" (buckets)
  que categoriza as peças por cor e uma lista separada para peças de vértice.
  Adicionada a função play_first para iniciar a busca a partir de um canto.
  OTIMIZAÇÃO: A função play agora recebe a cor necessária como parâmetro.
*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>

// Estrutura para uma única peça
typedef struct {
  unsigned int colors[4];
  unsigned char rotation;
  unsigned int id;
  int used;
} tile;

// Estrutura para uma lista dinâmica de ponteiros para peças
typedef struct {
    tile **tiles;
    unsigned int count;
    unsigned int capacity;
} tile_list;

// Estrutura para o estado completo do jogo
typedef struct {
  unsigned int size;
  unsigned int tile_count;
  unsigned int ncolors; // Número de cores + 1 (para a cor 0)
  tile ***board;
  tile *tiles;
  tile_list **color_buckets; // Array de listas por cor
  tile_list *vertex_tiles;   // Lista de peças de vértice (com 2 zeros)
} game;

#define X_COLOR(t, s) (t->colors[(s + 4 - t->rotation) % 4])
#define N_COLOR(t) (X_COLOR(t, 0))
#define E_COLOR(t) (X_COLOR(t, 1))
#define S_COLOR(t) (X_COLOR(t, 2))
#define W_COLOR(t) (X_COLOR(t, 3))

// Declaração antecipada das funções de busca
int play(game *game, unsigned int x, unsigned int y, unsigned int required_color);
int play_inversa(game *game, unsigned int x, unsigned int y, unsigned int required_color);


void add_tile_to_list(tile_list *list, tile *t) {
  // se uma peça tem a mesma cor duas vezes, não a adicionamos duas vezes na mesma lista
  for (unsigned int i = 0; i < list->count; i++) {
    if (list->tiles[i]->id == t->id) {
      return;
    }
  }
  // Se a lista está cheia, dobra a capacidade
  if (list->count >= list->capacity) {
    list->capacity = (list->capacity == 0) ? 4 : list->capacity * 2;
    list->tiles = realloc(list->tiles, list->capacity * sizeof(tile*));
    assert(list->tiles != NULL);
  }
  list->tiles[list->count++] = t;
}

void build_color_buckets(game *g) {
  // Aloca o array de listas
  g->color_buckets = calloc(g->ncolors, sizeof(tile_list*));
  assert(g->color_buckets != NULL);

  // Inicializa cada lista individualmente
  for (unsigned int i = 0; i < g->ncolors; i++) {
    g->color_buckets[i] = calloc(1, sizeof(tile_list));
    assert(g->color_buckets[i] != NULL);
  }

  // Percorre todas as peças e as adiciona aos buckets corretos
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
  // Aloca e inicializa a lista de peças de vértice
  g->vertex_tiles = calloc(1, sizeof(tile_list));
  assert(g->vertex_tiles != NULL);

  // Percorre todas as peças do jogo
  for (unsigned int i = 0; i < g->tile_count; i++) {
    tile *current_tile = &g->tiles[i];
    int zero_count = 0;
    // Conta quantas vezes a cor 0 aparece na peça
    for (int c = 0; c < 4; c++) {
      if (current_tile->colors[c] == 0) {
        zero_count++;
      }
    }
    // Se a contagem for exatamente 2, é uma peça de vértice
    if (zero_count == 2) {
      add_tile_to_list(g->vertex_tiles, current_tile);
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

// --- NOVA FUNÇÃO AUXILIAR ---
// Limpa o tabuleiro e o estado das peças para uma nova tentativa
void reset_game_state(game *g) {
    // Limpa o tabuleiro
    for (unsigned int y = 0; y < g->size; y++) {
        for (unsigned int x = 0; x < g->size; x++) {
            g->board[y][x] = NULL;
        }
    }
    // Reseta o estado 'used' de todas as peças
    for (unsigned int i = 0; i < g->tile_count; i++) {
        g->tiles[i].used = 0;
    }
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

// --- FUNÇÃO PLAY OTIMIZADA ---
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

        // Determina a próxima posição E a próxima cor necessária (Espiral: Direita -> Baixo -> Esquerda -> Cima)
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

// --- FUNÇÃO PLAY INVERSA ---
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
        if ((unsigned int)vertex_choice >= g->vertex_tiles->count) {
            fprintf(stderr, "Escolha de vértice (%d) inválida. Tente um número menor.\n", vertex_choice);
            return 0;
        }
        start_tile = g->vertex_tiles->tiles[vertex_choice];
        
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
        if ((unsigned int)mapped_choice >= g->vertex_tiles->count) {
            fprintf(stderr, "Escolha de vértice (%d) inválida. Tente um número menor.\n", vertex_choice);
            return 0;
        }
        start_tile = g->vertex_tiles->tiles[mapped_choice];

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
  
  game *g = initialize(stdin);
  int solution_found_flag = 0;

  // Loop para testar todas as 8 estratégias de início
  for (int i = 0; i < 8; i++) {
      clock_t start_time, end_time;
      double cpu_time_used;
      
      fprintf(stderr, "\n--- TENTATIVA COM A ESTRATÉGIA: %d ---\n", i);
      start_time = clock();

      if (play_first(g, i)) {
          // Imprime a solução apenas na primeira vez que a encontra
          if (!solution_found_flag) {
             printf("PRIMEIRA SOLUÇÃO ENCONTRADA (com a estratégia de início %d):\n", i);
             //print_solution(g);
             solution_found_flag = 1;
          }
      } 
      
      end_time = clock();
      cpu_time_used = ((double) (end_time - start_time)) / CLOCKS_PER_SEC;
      fprintf(stderr, "Tempo para a tentativa %d: %f segundos\n", i, cpu_time_used);
      
      // Reseta o estado do jogo para a próxima iteração
      reset_game_state(g); 
  }

  if (!solution_found_flag) {
      printf("\nSOLUTION NOT FOUND (após testar todas as 8 opções)\n");
  }

  free_resources(g);
  return 0;
}
