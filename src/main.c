#include <stdlib.h>
#include <stdio.h>
#include "SDL.h"
#include "vectormath/vectormath_aos.h"
#include "apis.h"


#define SCRN_WIDTH (640)
#define SCRN_HEIGHT (480)

static inline void swapi(int a, int b) {
  int t = a;
  a = b;
  b = t;
}

uint8_t* main_buffer;
int main_bfr_pitch;

void plot_pixel(uint32_t x, uint32_t y, uint32_t c) {
  if (x > SCRN_WIDTH || y > SCRN_HEIGHT) return;
  uint32_t* dst = (uint32_t*)(main_buffer + (main_bfr_pitch * y) + (x * 4));
  *dst = c;
}

void plot_line_low(int x0, int y0, int x1, int y1, uint32_t c) {
  int dx = x1 - x0;
  int dy = y1 - y0;
  int yi = 1;
  if (dy < 0) {
    yi = -1;
    dy = -dy;
  } 
  int D = 2*dy - dx;
  int y = y0;
  for (int x=x0; x < x1; ++x) {
    plot_pixel(x, y, c);
    if (D > 0) {
      y = y + yi;
      D = D - 2*dx;
    }
    D = D + 2*dy;
  }
}

void plot_line_high(int x0, int y0, int x1, int y1, uint32_t c) {
  int dx = x1 - x0;
  int dy = y1 - y0;
  int xi = 1;
  if (dx < 0) {
    xi = -1;
    dx = -dx;
  }
  int D = 2*dx - dy;
  int x = x0;
  for (int y=y0; y<y1; ++y) {
    plot_pixel(x, y, c);
    if (D > 0) {
      x = x + xi;
      D = D - 2*dy;
    }
    D = D + 2*dx;
  }
}

void plot_line(int x0, int y0, int x1, int y1, uint32_t c) {
  if (abs(y1 - y0) < abs(x1 - x0)) {
    if (x0 > x1) {
      plot_line_low(x1, y1, x0, y0, c);
    } else { 
      plot_line_low(x0, y0, x1, y1, c);
    }
  } else {
    if (y0 > y1) {
      plot_line_high(x1, y1, x0, y0, c);
    } else {
      plot_line_high(x0, y0, x1, y1, c);
    }
  }
}

int main(int argc, char** argv) {
  if (SDL_Init(SDL_INIT_VIDEO)!=0) {
    fprintf(stderr, "Error calling SDL_Init");        
  }

  obj_model_t model;
  FILE* f = fopen("./../data/head.obj", "rb");
  if (f) {
    obj_initialise(&model);
    obj_loadFrom(f, &model);
    fclose(f);
  }

  SDL_Window* window = SDL_CreateWindow("SoftwareRaster", 0, 0, SCRN_WIDTH, SCRN_HEIGHT, SDL_WINDOW_SHOWN);    
  SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);
  SDL_Texture* main_surface = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, SCRN_WIDTH, SCRN_HEIGHT);  
  SDL_Event evt;

  for (;;) {
    while (SDL_PollEvent(&evt)) {
      if (evt.type == SDL_QUIT)
        return 0;
    }
    
    uint8_t *bptr;
    SDL_RenderClear(renderer);
    SDL_LockTexture(main_surface, NULL, (void**)&main_buffer, &main_bfr_pitch);
    for (uint32_t y=0; y<SCRN_HEIGHT; ++y) {
      bptr = ((uint8_t*)main_buffer) + main_bfr_pitch*y;
      for (uint32_t x=0; x<SCRN_WIDTH; ++x) {
        *bptr = 255; ++bptr;
        *bptr = 255; ++bptr;
        *bptr = 255; ++bptr;
        *bptr = 255; ++bptr;
      }
    }
    for (uint32_t f=0, fn=model.nFaces; f < fn; ++f) {
      obj_face_t* pf = &model.faces[f];
      for (uint32_t v=0; v < 3; ++v) {
        VmathVector3 v0 = model.verts[pf->f[v]];
        VmathVector3 v1 = model.verts[pf->f[(v+1)%3]];
        int x0 = (v0.x+1.f)*(SCRN_WIDTH/2.f);
        int y0 = (v0.y+1.f)*(SCRN_HEIGHT/2.f);
        int x1 = (v1.x+1.f)*(SCRN_WIDTH/2.f);
        int y1 = (v1.y+1.f)*(SCRN_HEIGHT/2.f);
        plot_line(x0, SCRN_HEIGHT-y0, x1, SCRN_HEIGHT-y1, 0x00000000);
      }
    }

    SDL_UnlockTexture(main_surface);
    main_buffer = NULL;
    SDL_RenderCopy(renderer, main_surface, NULL, NULL);
    SDL_RenderPresent(renderer);
  }
  return 0;
}

