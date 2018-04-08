#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <float.h>
#include "SDL.h"
#include "vectormath/vectormath_aos.h"
#include "apis.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#undef STB_IMAGE_IMPLEMENTATION

#define DRAW_LINES (0)
#define DRAW_TRIS (0)
#define MAT_MUL_TRIS (1)

#define DRAW_SINGLE_TRI (0)
#define DRAW_HEAD_MODEL (1)

#define SCRN_WIDTH (960)
#define SCRN_HEIGHT (720)

#define MAKE_U32_COLOR(r, g, b, a) (uint32_t)(((r&0xFF)  << 24) | ((g&0xFF) << 16) | ((b&0xFF) << 8) | ((a&0xFF) << 0))

static inline float minf(float a, float b) {
  return a < b ? a : b;
}

static inline float maxf(float a, float b) {
  return a > b ? a : b;
}

static inline float clampf(float min, float max, float v) {
  if (v < min) return min;
  if (v > max) return max;
  return v;
}

static inline int clampi(int min, int max, int v) {
  if (v < min) return min;
  if (v > max) return max;
  return v;
}

static inline float lerpf(float a, float b, float t) {
  return a + (b-a)*t;
}

static inline void swapi(int a, int b) {
  int t = a;
  a = b;
  b = t;
}

struct texture_slot_t {
  uint8_t* data;
  uint16_t width, height;
};
typedef struct texture_slot_t texture_slot_t;

uint8_t* main_buffer;
int main_bfr_pitch;
float* z_buffer;
texture_slot_t tex_slots;

void plot_pixel(uint32_t x, uint32_t y, uint32_t c) {
  if (x > SCRN_WIDTH || y > SCRN_HEIGHT) return;
  uint32_t* dst = (uint32_t*)(main_buffer + (main_bfr_pitch * y) + (x * 4));
  *dst = c;
}

int z_test(uint32_t x, uint32_t y, float z) {
  z =  z * .5f + .5f;
  if (x > SCRN_WIDTH || y > SCRN_HEIGHT) return 0;
  uint32_t idx = (y*SCRN_WIDTH) + x;
  if (z > z_buffer[idx]) {
    return 0;
  }
  z_buffer[idx] = z;
  return 1;
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

vec3_t barycentric2(vec3_t* tri, vec3_t p) {
  vec3_t *a = tri, *b = tri+1, *c = tri+2;
  vec3_t v0, v1, v2;
  vmathV3Sub(&v0, b, a);
  vmathV3Sub(&v1, c, a);
  vmathV3Sub(&v2, &p, a);
  float d00 = vmathV3Dot(&v0, &v0);
  float d01 = vmathV3Dot(&v0, &v1);
  float d11 = vmathV3Dot(&v1, &v1);
  float d20 = vmathV3Dot(&v2, &v0);
  float d21 = vmathV3Dot(&v2, &v1);
  float denom = d00 * d11 - d01 * d01;
  vec3_t r;
  r.y = (d11*d20 - d01*d21) / denom;
  r.z = (d00*d21 - d01*d20) / denom;
  r.x = 1.f - r.y - r.z;
  return r;
}

vec3_t barycentricV4(vec4_t* tri, vec3_t p) {
  vec3_t a = {tri[0].x, tri[0].y, tri[0].z};
  vec3_t b = {tri[1].x, tri[1].y, tri[1].z};
  vec3_t c = {tri[2].x, tri[2].y, tri[2].z};
  vec3_t v0, v1, v2;
  vmathV3Sub(&v0, &b, &a);
  vmathV3Sub(&v1, &c, &a);
  vmathV3Sub(&v2, &p, &a);
  float d00 = vmathV3Dot(&v0, &v0);
  float d01 = vmathV3Dot(&v0, &v1);
  float d11 = vmathV3Dot(&v1, &v1);
  float d20 = vmathV3Dot(&v2, &v0);
  float d21 = vmathV3Dot(&v2, &v1);
  float denom = d00 * d11 - d01 * d01;
  vec3_t r;
  r.y = (d11*d20 - d01*d21) / denom;
  r.z = (d00*d21 - d01*d20) / denom;
  r.x = 1.f - r.y - r.z;
  return r;
}

uint32_t sample_colour(float u, float v) {
  int x = clampi(0, tex_slots.width, (int)round(u*(float)tex_slots.width));
  int y = clampi(0, tex_slots.height, (int)round(v*(float)tex_slots.height));
  uint8_t* t = tex_slots.data + (y*tex_slots.width*3) + (x*3);
  return MAKE_U32_COLOR(t[0], t[1], t[2], 0xFF);
  //return MAKE_U32_COLOR((uint8_t)(u*255), (uint8_t)(v*255), 0x0, 0xFF);
}

/*
  clips a triangle in clip space. return the number of vertices written to clipped_tri (always a multiple of 3)
*/
uint32_t clip_tri(vec4_t* tri, vec4_t* clipped_tri) {
  if (tri[0].w <= 0.f && tri[1].w <= 0.f && tri[2].w <= 0.f)
    return 0;
  // STUB currently. Doesn't handle any but behind the camera.
  clipped_tri[0] = tri[0];
  clipped_tri[1] = tri[1];
  clipped_tri[2] = tri[2];
  return 3;
}

/*
  cull_tri_* -> return 1 if triangle should be culled. tri is expected to be 
  in normalised device coords. 
*/
int cull_tri_cw(vec4_t* tri) {
  float d = (tri[1].x - tri[0].x) * (tri[2].y - tri[0].y) - 
            (tri[1].y - tri[0].y) * (tri[2].x - tri[0].x);
  return d < 0.f;
}

int cull_tri_ccw(vec4_t* tri) {
  float d = (tri[1].x - tri[0].x) * (tri[2].y - tri[0].y) - 
            (tri[1].y - tri[0].y) * (tri[2].x - tri[0].x);
  return d > 0.f;
}

void triangle(vec3_t* tri, obj_uv_t* uvs, uint32_t colour) {
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
  float u, v;
  for(pt.x = bbmin.x; pt.x <= bbmax.x; ++pt.x) {
    for (pt.y = bbmin.y; pt.y <= bbmax.y; ++pt.y) {
      vec3_t scrn = barycentric2(tri, pt);
      if (scrn.x < 0 || scrn.y < 0 || scrn.z < 0) continue;
      //if (!(scrn.y >= 0 && scrn.z >= 0.f && (scrn.y + scrn.z) <= 1.f)) continue;
      pt.z = tri[0].z*scrn.x;
      pt.z += tri[1].z*scrn.y;
      pt.z += tri[2].z*scrn.z;

      u = uvs[0].u*scrn.x + uvs[1].u*scrn.y + uvs[2].u*scrn.z;
      v = uvs[0].v*scrn.x + uvs[1].v*scrn.y + uvs[2].v*scrn.z;

      /*if (z_test((uint32_t)pt.x, (uint32_t)pt.y, pt.z))*/ {
        colour = sample_colour(u, v);
        plot_pixel((uint32_t)pt.x, (uint32_t)pt.y, colour);
      }
    }
  }
}

void triangle_1(vec3_t* tri, uint32_t colour) {
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
  float u, v;
  for(pt.x = bbmin.x; pt.x <= bbmax.x; ++pt.x) {
    for (pt.y = bbmin.y; pt.y <= bbmax.y; ++pt.y) {
      vec3_t scrn = barycentric2(tri, pt);
      if (scrn.x < 0 || scrn.y < 0 || scrn.z < 0) continue;
      //if (!(scrn.y >= 0 && scrn.z >= 0.f && (scrn.y + scrn.z) <= 1.f)) continue;
      pt.z = tri[0].z*scrn.x;
      pt.z += tri[1].z*scrn.y;
      pt.z += tri[2].z*scrn.z;

      if (z_test((uint32_t)pt.x, (uint32_t)pt.y, pt.z)) {
        plot_pixel((uint32_t)pt.x, (uint32_t)pt.y, colour);
      }
    }
  }
}

void triangle_2(vec4_t* tri, obj_uv_t* uvs, uint32_t colour) {
  vec3_t bbmin, bbmax, limit;
  vmathV3MakeFromElems(&bbmin, SCRN_WIDTH, SCRN_HEIGHT, 1);
  vmathV3MakeFromElems(&bbmax, 0.f, 0.f, 0.f);
  vmathV3Copy(&limit, &bbmin);
  for (uint32_t i=0; i < 3; ++i) {
    for (uint32_t j=0; j < 2; ++j) {
      vmathV3SetElem(&bbmin, j, maxf(0.f, minf(vmathV3GetElem(&bbmin, j), vmathV4GetElem(&tri[i], j))));
      vmathV3SetElem(&bbmax, j, minf(vmathV3GetElem(&limit, j), maxf(vmathV3GetElem(&bbmax, j), vmathV4GetElem(&tri[i], j))));
    }
  }

  vec3_t pt;
  float u, v, z;
  for(pt.x = bbmin.x; pt.x <= bbmax.x; ++pt.x) {
    for (pt.y = bbmin.y; pt.y <= bbmax.y; ++pt.y) {
      vec3_t bary = barycentricV4(tri, pt);
      if (bary.x < 0 || bary.y < 0 || bary.z < 0) continue;

      // interpolate depth
      z = tri[0].z*bary.x + tri[1].z*bary.y + tri[2].z*bary.z;

      if (z_test((uint32_t)pt.x, (uint32_t)pt.y, z)) {
        // Compute perspective correct interpolation coefficients.
        vec3_t persp_tmp = { bary.x/tri[0].w, bary.y/tri[1].w, bary.z/tri[2].w }, persp;
        // Perspective interpolation of texture coordinates (and normal).
        vmathV3ScalarMul(&persp, &persp_tmp, 1.0 / (persp_tmp.x + persp_tmp.y + persp_tmp.z));

        // Non-perspective correct interpolation
        //vmathV3Copy(&persp, &bary);
                
        u = uvs[0].u*persp.x + uvs[1].u*persp.y + uvs[2].u*persp.z;
        v = uvs[0].v*persp.x + uvs[1].v*persp.y + uvs[2].v*persp.z;

        colour = sample_colour(u, v);
        plot_pixel((uint32_t)pt.x, (uint32_t)pt.y, colour);
      }
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
  //FILE* f = fopen("./../data/cube.obj", "rb");
  if (f) {
    obj_initialise(&model);
    obj_loadFrom(f, &model);
    fclose(f);
  }

  texture_slot_t adiffuse;
  {
    int w, h;
    adiffuse.data = stbi_load("./../data/ahead_diffuse.tga", &w, &h, NULL, 3);
    adiffuse.width = (uint16_t)w;
    adiffuse.height = (uint16_t)h;
  }
  tex_slots = adiffuse;

  SDL_Window* window = SDL_CreateWindow("SoftwareRaster", 0, 0, SCRN_WIDTH, SCRN_HEIGHT, SDL_WINDOW_SHOWN);    
  SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);
  SDL_Texture* main_surface = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, SCRN_WIDTH, SCRN_HEIGHT);  
  SDL_Event evt;

  z_buffer = malloc(SCRN_WIDTH*SCRN_HEIGHT*sizeof(float));
  main_buffer = malloc(SCRN_WIDTH*SCRN_HEIGHT*4);
  main_bfr_pitch = SCRN_WIDTH*4;

  vec3_t head_centre = {.0f, .0f, -3.f};
  mat4_t world, view, proj, worldview, worldviewproj;
  /*
    Transforms are as such (NOTE This example uses a Right handed coordinate space):
    Original    | Model Xform   | View Xfrom        | Projection Xform                |         Perspective divide                     | VP Scale & Bias
    Model space -> World space -> View/camera space -> Clip (Homogeneous) coordinates / Perspective divide -> Normalised Device Coords -> Viewport space
  */
  vmathM4MakeTranslation(&world, &head_centre);
  vmathM4MakeIdentity(&view);
  //vmathM4MakeFrustum(&proj, -SCRN_WIDTH/2, SCRN_WIDTH/2, -SCRN_HEIGHT/2, SCRN_HEIGHT/2, 0.1f, 10000.f);
  //vmathM4MakeFrustum(&proj, -50.f, 50.f, -50.f, 50.f, 0.001f, 10000.f);
  vmathM4MakePerspective(&proj, 3.1415f/4, 16.f/9.f, .1f, 100000.f);

  for (;;) {
    while (SDL_PollEvent(&evt)) {
      if (evt.type == SDL_QUIT)
        return 0;
    }

    static float head_rot = 0;
    mat4_t pivot, tmp;
    vmathM4MakeRotationY(&world, head_rot);
    vmathM4SetTranslation(&world, &head_centre);
    //vmathM4Mul(&world, &tmp, &pivot);
    head_rot += (1.f/180.f) * 2.f;

    vmathM4Mul(&worldview, &view, &world);
    vmathM4Mul(&worldviewproj, &proj, &worldview);
    
    uint8_t *bptr;

    for (uint32_t y=0; y<SCRN_HEIGHT; ++y) {
      bptr = ((uint8_t*)main_buffer) + main_bfr_pitch*y;
      for (uint32_t x=0; x<SCRN_WIDTH; ++x) {
        *bptr = 255; ++bptr;
        *bptr = 255; ++bptr;
        *bptr = 0; ++bptr;
        *bptr = 255; ++bptr;
      }
    }
    for (uint32_t i=0, n=SCRN_WIDTH*SCRN_HEIGHT; i<n; ++i) {
      z_buffer[i] = 1.0f;
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
      obj_uv_t uvs[3] = {
        [0] = model.uvs[pf->uv[0]],
        [1] = model.uvs[pf->uv[1]],
        [2] = model.uvs[pf->uv[2]],
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
        triangle(t, uvs, MAKE_U32_COLOR(dot, dot, dot, 255));
      }
    }
#elif MAT_MUL_TRIS
    if (DRAW_SINGLE_TRI)
    {
      vec4_t v[3] = {
        {-10.f,  0.f, 0.f, 1.f},
        {  0.f, 10.f, 0.f, 1.f},
        { 10.f,  0.f, 0.f, 1.f},
      };
      vec4_t t[3];
      vec3_t pt[3];

      // xform in to clip space
      vmathM4MulV4(t+0, &worldviewproj, &v[0]);
      vmathM4MulV4(t+1, &worldviewproj, &v[1]);
      vmathM4MulV4(t+2, &worldviewproj, &v[2]);
      // perform the perspective divide
      for (uint32_t v=0; v<3; ++v) {
        pt[v].x = t[v].x / t[v].w;
        pt[v].y = t[v].y / t[v].w;
        pt[v].z = t[v].z / t[v].w;
      }
      // triangle expects viewport coords not Normalised Device Coordinates.
      for (uint32_t v=0; v < 3; ++v) {
        pt[v].x = roundf((pt[v].x+1.f)*(SCRN_WIDTH/2.f));
        pt[v].y = roundf(SCRN_HEIGHT - ((pt[v].y+1.f)*(SCRN_HEIGHT/2.f)));
      }

      triangle_1(pt, MAKE_U32_COLOR(0xFF, 0x00, 0xFF, 0xFF));
    }

    if (DRAW_HEAD_MODEL)
    {
      for (uint32_t f=0, fn=model.nFaces; f < fn; ++f) {
        obj_face_t* pf = &model.faces[f];
        vec4_t v[3] = {
          {model.verts[pf->f[0]].x, model.verts[pf->f[0]].y, model.verts[pf->f[0]].z, 1.f},
          {model.verts[pf->f[1]].x, model.verts[pf->f[1]].y, model.verts[pf->f[1]].z, 1.f},
          {model.verts[pf->f[2]].x, model.verts[pf->f[2]].y, model.verts[pf->f[2]].z, 1.f},
        };
        vec4_t t[3], clipt[6];
        vec4_t pt[3];
        obj_uv_t uvs[3] = {
          [0] = model.uvs[pf->uv[0]],
          [1] = model.uvs[pf->uv[1]],
          [2] = model.uvs[pf->uv[2]],
        };

        // xform in to clip space
        vmathM4MulV4(t+0, &worldviewproj, v+0);
        vmathM4MulV4(t+1, &worldviewproj, v+1);
        vmathM4MulV4(t+2, &worldviewproj, v+2);
        // clip to the view frustum 
        uint32_t ctn = clip_tri(t, clipt);
        for (int ct=0; ct < ctn; ct+=3) {
          // perform the perspective divide
          for (uint32_t v=0; v<3; ++v) {
            // x & y -> Normalised Device Coordinates.
            // z -> -1 to 1 depth (NOTE: differs from DirectX standard, which is 0 to 1). We scale & bias in the z test to 0 to 1
            // z TODO: Reversed depth 1->0 and discard -1 to 1 standard
            // w -> perspective divide denom. Needed for correct interpolation
            pt[v].x = clipt[ct+v].x / clipt[ct+v].w;
            pt[v].y = clipt[ct+v].y / clipt[ct+v].w;
            pt[v].z = clipt[ct+v].z / clipt[ct+v].w;
            pt[v].w = clipt[ct+v].w;
          }
          // cull the triangle before the viewport xform
          if (cull_tri_cw(pt))
            continue;

          // triangle expects viewport coords not Normalised Device Coordinates.
          for (uint32_t v=0; v < 3; ++v) {
            pt[v].x = roundf((pt[v].x+1.f)*(SCRN_WIDTH/2.f));
            pt[v].y = roundf(SCRN_HEIGHT - ((pt[v].y+1.f)*(SCRN_HEIGHT/2.f)));
          }

          triangle_2(pt, uvs, MAKE_U32_COLOR(0xFF, 0xFF, 0xFF, 0xFF));
        }
      }
    }
#endif

    SDL_RenderClear(renderer);
    void* dest_main_buffer;
    int dest_main_bfr_pitch;
    SDL_LockTexture(main_surface, NULL, (void**)&dest_main_buffer, &dest_main_bfr_pitch);
    for (uint32_t i=0; i < SCRN_HEIGHT; ++i) {
      memcpy(dest_main_buffer + (i*dest_main_bfr_pitch), main_buffer + (i*main_bfr_pitch), main_bfr_pitch);
    }
    SDL_UnlockTexture(main_surface);
    
    SDL_RenderCopy(renderer, main_surface, NULL, NULL);
    SDL_RenderPresent(renderer);
  }
  return 0;
}

