#ifndef MODEL_H
#define MODEL_H

#include "config.h"
#include <SDL/SDL_opengl.h>

typedef struct object_group object_group;
struct object_group {
    object_group *next;
    object_group *child;
    GLint ind_count;
    GLint ind_start;
    GLfloat translate[3];
    GLfloat color[3];
    char mod;
};

typedef struct {
    GLushort *ind;
    GLfloat *vec;
    GLint ind_count;
    GLint vec_count;
    GLuint ind_vbo;
    GLuint vec_vbo;
    object_group *group;
} sprite_model;

extern sprite_model model_farmer;
extern sprite_model model_duck;
extern sprite_model model_ladder;
extern sprite_model model_wall;
extern sprite_model model_egg;
extern sprite_model model_grain;
extern sprite_model model_lift;

#endif
