#pragma once

#include "vectormath/vectormath_aos.h"
#include <stdint.h>
#include <stdio.h>

typedef VmathVector3 vec3_t;

struct obj_face_t {
  uint32_t f[3];
};
typedef struct obj_face_t obj_face_t;

struct obj_model_t {
  vec3_t* verts;
  obj_face_t* faces;
  uint32_t nVerts, nFaces;
  uint32_t nRVerts, nRFaces;
};
typedef struct obj_model_t obj_model_t;

void obj_initialise(obj_model_t* obj);
int obj_loadFrom(FILE* file, obj_model_t* obj);

