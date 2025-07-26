#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

typedef struct {
  unsigned int colors[4];
  unsigned char rotation;
  unsigned int id;
  int used;
} tile;

typedef struct {
  unsigned int size;
  unsigned int tile_count; //==size^2
  tile ***board;
  tile *tiles;
} game;

#define X_COLOR(t, s) (t->colors[(s + 4 - t->rotation) % 4])
#define N_COLOR(t) (X_COLOR(t, 0))
#define E_COLOR(t) (X_COLOR(t, 1))
#define S_COLOR(t) (X_COLOR(t, 2))
#define W_COLOR(t) (X_COLOR(t, 3))

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
  g->size = bsize;
  g->board = malloc (sizeof (tile**) * bsize);
  for(int i = 0; i < bsize; i++)
    g->board[i] = calloc(bsize, sizeof(tile*));
  g->tile_count = bsize * bsize;

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

  return g;
}

void free_resources(game *game) {
  free(game->tiles);
  for(int i = 0; i < game->size; i++)
    free(game->board[i]);
  free(game->board);
  free(game);
}