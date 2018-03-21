#include <stdlib.h>
#include <stdio.h>
#include "SDL.h"
#include "vectormath/vectormath_aos.h"
#include "apis.h"

#define DRAW_LINES (0)
#define DRAW_TRIS (1)

#define SCRN_WIDTH (960)
#define SCRN_HEIGHT (720)

#define MAKE_U32_COLOR(r, g, b, a) (uint32_t)(((r&0xFF)  << 24) | ((g&0xFF) << 16) | ((b&0xFF) << 8) | ((a&0xFF) << 0))

static inline float minf(float a, float b) {
  return a < b ? a : b;
}

static inline float maxf(float a, float b) {
  return a > b ? a : b;
}

static inline float clampf(float max, float min, float v) {
  return v < min ? min : v > max ? max : v;
}

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

vec3_t barycentric(vec3_t* tri, vec3_t p) {
  vec3_t a = {
    .x=tri[2].x - tri[0].x,
    .y=tri[1].x - tri[0].x,
    .z=tri[0].x - p.x,
  };
  vec3_t b = {
    .x=tri[2].y - tri[0].y,
    .y=tri[1].y - tri[0].y,
    .z=tri[0].y - p.y,
  };
  vec3_t u;
  vmathV3Cross(&u, &a, &b);
  if (fabs(u.y) < 1.f) return (vec3_t){.x=-1.f, .y=1.f, .z=1.f};
  return (vec3_t){.x=1.f-(u.x+u.y)/u.z, .y=u.y/u.z, .z=u.x/u.z};
}

void triangle(vec3_t* tri, uint32_t colour) {
  vec3_t bbmin, bbmax, limit;
  vmathV3MakeFromElems(&bbmin, SCRN_WIDTH, SCRN_HEIGHT, 1);
  vmathV3MakeFromElems(&bbmax, 0.f, 0.f, 0.f);
  vmathV3Copy(&limit, &bbmin);
  for (uint32_t i=0; i < 3; ++i) {
    for (uint32_t j=0; j < 2; ++j) {
      vmathV3SetElem(&bbmin, j, maxf(0.f, minf(vmathV3GetElem(&bbmin, j), vmathV3GetElem(&tri[i], j))));
      vmathV3SetElem(&bbmax, j, minf(vmathV3GetElem(&limit, j), maxf(vmathV3GetElem(&bbmax, j), vmathV3GetElem(&tri[i], j))));
    }
  }

  vec3_t pt;
  for(pt.x = bbmin.x; pt.x <= bbmax.x; ++pt.x) {
    for (pt.y = bbmin.y; pt.y <= bbmax.y; ++pt.y) {
      vec3_t scrn = barycentric(tri, pt);
      if (scrn.x < 0 || scrn.y < 0 || scrn.z < 0) continue;
      plot_pixel((uint32_t)pt.x, (uint32_t)pt.y, colour);
    }
  }
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
        *bptr = 0; ++bptr;
        *bptr = 255; ++bptr;
        *bptr = 255; ++bptr;
      }
    }
#if DRAW_LINES
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
#elif DRAW_TRIS
    /*
    vec3_t test[3] = {
      {0, 0, 0}, 
      {200, 100, 0},
      {100, 100, 0},
    };
    triangle(test, 0x00FF0000);
    */
    uint32_t colours[5] = {
      0x00FFFF00, 0x00000000, 0x00FF0000, 0x0000FF00, 0x000000FF
    };
    for (uint32_t f=0, fn=model.nFaces; f < fn; ++f) {
      obj_face_t* pf = &model.faces[f];
      vec3_t t[3] = {
        [0] = model.verts[pf->f[0]],
        [1] = model.verts[pf->f[1]],
        [2] = model.verts[pf->f[2]],
      };
      vec3_t a, b, n;
      vmathV3Sub(&a, &model.verts[pf->f[2]], &model.verts[pf->f[0]]);
      vmathV3Sub(&b, &model.verts[pf->f[1]], &model.verts[pf->f[0]]);
      vmathV3Cross(&n, &a, &b);
      vmathV3Normalize(&n, &n);
      for (uint32_t v=0; v < 3; ++v) {
        t[v].x = floorf((t[v].x+1.f)*(SCRN_WIDTH/2.f));
        t[v].y = floorf(SCRN_HEIGHT - ((t[v].y+1.f)*(SCRN_HEIGHT/2.f)));
      }

      mat3_t m;
      if (n.z < 0.f) {
        uint8_t dot = (uint8_t)roundf((1.f-n.z)*255.f);
        triangle(t, MAKE_U32_COLOR(dot, dot, dot, 255));
      }
    }
#endif

    SDL_UnlockTexture(main_surface);
    main_buffer = NULL;
    SDL_RenderCopy(renderer, main_surface, NULL, NULL);
    SDL_RenderPresent(renderer);
  }
  return 0;
}

