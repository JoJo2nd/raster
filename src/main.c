#include <stdlib.h>
#include <stdio.h>
#include "SDL.h"
#include "vectormath/vectormath_aos.h"

int main(int argc, char** argv) {
  VmathVector3 v3;
  v3.x= 0.f;

  if (SDL_Init(SDL_INIT_VIDEO)!=0) {
    fprintf(stderr, "Error calling SDL_Init");        
  }

  SDL_Window* window = SDL_CreateWindow("SoftwareRaster", 0, 0, 640, 480, SDL_WINDOW_SHOWN);    
  SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);
  SDL_Texture* main_surface = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, 640, 480);  
  SDL_Event evt;

  for (;;) {
    while (SDL_PollEvent(&evt)) {
      if (evt.type == SDL_QUIT)
        return 0;
    }

    void* buffer;
    uint8_t *bptr;
    int bfr_pitch;
    SDL_RenderClear(renderer);
    SDL_LockTexture(main_surface, NULL, &buffer, &bfr_pitch);
    for (uint32_t y=0; y<480; ++y) {
      bptr = ((uint8_t*)buffer) + bfr_pitch*y;
      for (uint32_t x=0; x<640; ++x) {
        *bptr = 255; ++bptr;
        *bptr = 0; ++bptr;
        *bptr = 0; ++bptr;
        *bptr = 255; ++bptr;
      }
    }
    SDL_UnlockTexture(main_surface);
    SDL_RenderCopy(renderer, main_surface, NULL, NULL);
    SDL_RenderPresent(renderer);
  }

	return 0;
}

