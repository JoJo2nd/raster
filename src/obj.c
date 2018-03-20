
#include "apis.h"
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

void obj_initialise(obj_model_t* obj) {
  obj->nFaces = obj->nRFaces = 0;
  obj->nVerts = obj->nRVerts = 0;
}

void obj_release(obj_model_t* obj) {
  free(obj->verts);
  free(obj->faces);
  obj->nVerts = obj->nRVerts = 0;
  obj->nFaces = obj->nRFaces = 0;
}

enum parse_mode_t {
  parse_mode_vertex_1,
  parse_mode_vertex_2,
  parse_mode_vertex_3,
  parse_mode_face_1,
  parse_mode_face_2,
  parse_mode_face_3,
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
  obj->verts = malloc(sizeof(VmathVector3)*obj->nRVerts);
  obj->faces = malloc(sizeof(obj_face_t)*obj->nRFaces);

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
          sscanf(token_buffer, "%d/%d/%d", &v, &n, &t);
          // Obj files indices 1 base (not zero based!)
          obj->faces[obj->nFaces-1].f[parse_mode-parse_mode_face_1] = v-1;
          parse_mode++;
          if (parse_mode > parse_mode_face_3) {
            //printf("f %d %d %d\n", obj->faces[obj->nFaces-1].f[0], obj->faces[obj->nFaces-1].f[1], obj->faces[obj->nFaces-1].f[2]);
            parse_mode = parse_mode_none;
          }
        } break;
      }
    }
    r = getNextToken(token_buffer, sizeof token_buffer, file);
  }

  return 0;
}
