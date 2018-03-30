#pragma once

#include "vectormath/vectormath_aos.h"
#include <stdint.h>
#include <stdio.h>

typedef VmathVector3 vec3_t;
typedef VmathMatrix3 mat3_t;
typedef VmathMatrix4 mat4_t;

struct obj_face_t {
  uint16_t f[3];
  uint16_t uv[3];
};
typedef struct obj_face_t obj_face_t;

struct obj_uv_t {
  float u,v;
};
typedef struct obj_uv_t obj_uv_t;

struct obj_model_t {
  vec3_t* verts;
  obj_uv_t* uvs;
  obj_face_t* faces;
  uint32_t nVerts, nUVs, nFaces;
  uint32_t nRVerts, nRUVs, nRFaces;
};
typedef struct obj_model_t obj_model_t;

void obj_initialise(obj_model_t* obj);
int obj_loadFrom(FILE* file, obj_model_t* obj);

