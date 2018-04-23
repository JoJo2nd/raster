
#include "apis.h"
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

void obj_initialise(obj_model_t* obj) {
  memset(obj, 0, sizeof(obj_model_t));
}

void obj_release(obj_model_t* obj) {
  free(obj->verts);
  free(obj->faces);
  free(obj->uvs);
  memset(obj, 0, sizeof(obj_model_t));
}

enum parse_mode_t {
  parse_mode_vertex_1,
  parse_mode_vertex_2,
  parse_mode_vertex_3,
  parse_mode_face_1,
  parse_mode_face_2,
  parse_mode_face_3,
  parse_mode_uv_1,
  parse_mode_uv_2,
  parse_mode_none,
};
typedef enum parse_mode_t parse_mode_t;

static int skipWhitespace(FILE* f) {
  int c;
  for (;;) {
    c = fgetc(f);
    if (!isspace(c)) {
      ungetc(c, f);
      return 0;
    }
    if (c == EOF) return EOF;
  } 
  return 0;
}

static int skipLine(FILE* f) {
  int c;
  for (;;) {
    c = fgetc(f);
    if (c == '\r') {
      // check of \r\n style line endings
      c = fgetc(f);
      if (c != '\n') ungetc(c, f);
      return 0;
    }
    if (c == '\n') return 0;
    if (c == EOF) return EOF;
  }
  return 0;
}

static int getNextToken(char* buf, uint32_t len, FILE* f) {
  uint32_t i=0;
  len -= 1;
  while ( i < len ) {
    int c = fgetc(f);
    if (c == EOF) break;
    else if (isspace(c)) break;
    else if (c == '#') {
      if (skipLine(f) == EOF) break;
    } else {
      buf[i++] = (char)c;
    }
  }
  buf[i]=0;
  return feof(f) ? EOF : i; 
}

int obj_loadFrom(FILE* file, obj_model_t* obj) {
  char token_buffer[1024];
  parse_mode_t parse_mode = parse_mode_none;
  obj->nRVerts = 1024;
  obj->nRFaces = obj->nRVerts*3;
  obj->nRUVs = obj->nRVerts*3;
  obj->verts = malloc(sizeof(VmathVector3)*obj->nRVerts);
  obj->faces = malloc(sizeof(obj_face_t)*obj->nRFaces);
  obj->uvs = malloc(sizeof(obj_uv_t)*obj->nRUVs);

  int r = getNextToken(token_buffer, sizeof token_buffer, file);
  while (r != EOF) {
    if (r != 0) { //empty token
      switch (parse_mode) {
        case parse_mode_none: {
          if (strcmp("v", token_buffer) == 0) {
            obj->nVerts++;
            if (obj->nVerts >= obj->nRVerts) {
              obj->nRVerts *= 2;
              obj->verts=realloc(obj->verts, sizeof(VmathVector3)*obj->nRVerts);
            }
            parse_mode = parse_mode_vertex_1;
          } else if (strcmp("f", token_buffer) == 0) {
            obj->nFaces++;
            if (obj->nFaces >= obj->nRFaces) {
              obj->nRFaces *= 2;
              obj->faces = realloc(obj->faces, sizeof(obj_face_t)*obj->nRFaces);
            }
            parse_mode = parse_mode_face_1;
          } else if (strcmp("vt", token_buffer) == 0) {
            obj->nUVs++;
            if (obj->nUVs >= obj->nRUVs) {
              obj->nRUVs *= 2;
              obj->uvs = realloc(obj->uvs, sizeof(obj_uv_t)*obj->nRUVs);
            }
            parse_mode = parse_mode_uv_1;
          }
        } break;
        case parse_mode_vertex_1:
        case parse_mode_vertex_2:
        case parse_mode_vertex_3: {
          double d = atof(token_buffer);
          if (parse_mode == parse_mode_vertex_1)
            obj->verts[obj->nVerts-1].x = d;
          if (parse_mode == parse_mode_vertex_2)
            obj->verts[obj->nVerts-1].y = d;
          if (parse_mode == parse_mode_vertex_3){            
            obj->verts[obj->nVerts-1].z = d;
            //printf("v %f %f %f\n", obj->verts[obj->nVerts-1].x, obj->verts[obj->nVerts-1].y, obj->verts[obj->nVerts-1].z);
          }
          parse_mode++;
          if (parse_mode > parse_mode_vertex_3)
            parse_mode = parse_mode_none;
        } break;
        case parse_mode_face_1:
        case parse_mode_face_2:
        case parse_mode_face_3: {
          int32_t v, n, t;
          sscanf(token_buffer, "%d/%d/%d", &v, &t, &n);
          // Obj files indices 1 base (not zero based!)
          obj->faces[obj->nFaces-1].f[parse_mode-parse_mode_face_1] = (uint16_t)v-1;
          obj->faces[obj->nFaces-1].uv[parse_mode-parse_mode_face_1] = (uint16_t)(t-1);
          parse_mode++;
          if (parse_mode > parse_mode_face_3) {
            uint16_t v1 = obj->faces[obj->nFaces-1].f[0],
            v2 = obj->faces[obj->nFaces-1].f[1],
            v3 = obj->faces[obj->nFaces-1].f[2],
            t1 = obj->faces[obj->nFaces-1].uv[0],
            t2 = obj->faces[obj->nFaces-1].uv[1],
            t3 = obj->faces[obj->nFaces-1].uv[2];
            printf("face (%f %f %f):(%f %f), (%f %f %f):(%f %f), (%f %f %f):(%f %f)\n", 
              obj->verts[v1].x, obj->verts[v1].y, obj->verts[v1].z, obj->uvs[t1].u, obj->uvs[t1].v,
              obj->verts[v2].x, obj->verts[v2].y, obj->verts[v2].z, obj->uvs[t2].u, obj->uvs[t2].v,
              obj->verts[v3].x, obj->verts[v3].y, obj->verts[v3].z, obj->uvs[t3].u, obj->uvs[t3].v);
            parse_mode = parse_mode_none;
          }
        } break;
        case parse_mode_uv_1:
        case parse_mode_uv_2: {
          double d = atof(token_buffer);
          if (parse_mode == parse_mode_uv_1) 
            obj->uvs[obj->nUVs-1].u = d;
          else if (parse_mode == parse_mode_uv_2) 
            obj->uvs[obj->nUVs-1].v = 1.f-d;
          parse_mode++;
          if (parse_mode > parse_mode_uv_2) {
            parse_mode = parse_mode_none; 
          }
        } break;
      }
    }
    r = getNextToken(token_buffer, sizeof token_buffer, file);
  }

  return 0;
}
