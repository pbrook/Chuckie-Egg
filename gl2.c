/* OpenGL 2.0 setup and rendering code.
   Copyright (C) 2011 Paul Brook
   This code is licenced under the GNU GPL v3 */
#include "chuckie.h"

#include <SDL.h>
#define GL_GLEXT_PROTOTYPES

#include "raster.h"
#include "model.h"

#define PROJECT_ORTHO
#define DEPTH 640.0f

static int fullscreen = 0;
static int scale = 2;

raster_hooks *raster = NULL;

static SDL_Surface *sdlscreen;

void die(const char *msg, ...)
{
  va_list va;
  va_start(va, msg);
  fprintf(stderr, "Error: ");
  vfprintf(stderr, msg, va);
  va_end(va);
  exit(1);
}

static Uint32 do_timer(Uint32 interval, void *param)
{
  SDL_Event event;
  event.type = SDL_USEREVENT;
  SDL_PushEvent(&event);
  return interval;
}

static const GLchar *vertex_code = " \
#version 110 \n\
attribute vec3 position;  \n\
uniform mat4 world; \n\
uniform mat4 proj; \n\
void main() \n\
{ \n\
    gl_Position = proj * world * vec4(position, 1.0); \n\
} \n\
";

static const GLchar *fragment_code = " \
#version 110 \n\
uniform vec3 color; \n\
void main() \n\
{ \n\
    gl_FragColor = vec4(color, 1.0f); \n\
} \n\
";

GLuint vertex_shader;
GLuint fragment_shader;
GLuint program;
GLuint attr_position;
GLuint param_color;
GLuint param_world;
GLuint param_proj;

static void show_info_log(GLuint object, PFNGLGETSHADERIVPROC glGet__iv,
    PFNGLGETSHADERINFOLOGPROC glGet__InfoLog)
{
    GLint log_length;
    char *log;

    glGet__iv(object, GL_INFO_LOG_LENGTH, &log_length);
    log = malloc(log_length);
    glGet__InfoLog(object, log_length, NULL, log);
    fprintf(stderr, "%s", log);
    free(log);
}

static GLuint make_shader(GLenum type, const GLchar *src)
{
    GLuint shader;
    GLint shader_ok;

    shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &shader_ok);
    if (!shader_ok) {
        fprintf(stderr, "Failed to compile shader:\n");
        show_info_log(shader, glGetShaderiv, glGetShaderInfoLog);
        glDeleteShader(shader);
        exit(1);
    }
    return shader;
}

typedef struct {
    struct {
	GLfloat r[4];
    } c[4];
} matrix;

static void m_identity(matrix *m)
{
    int i, j;
    for (i = 0; i < 4; i++)
	for (j = 0; j < 4; j++)
	    m->c[i].r[j] = (i == j) ? 1.0f : 0.0f;
}

static void m_translate(matrix *m, GLfloat x, GLfloat y, GLfloat z)
{
    m->c[3].r[0] += x;
    m->c[3].r[1] += y;
    m->c[3].r[2] += z;
}

static void m_scale(matrix *m, GLfloat x, GLfloat y, GLfloat z)
{
    int i;

    for (i = 0; i < 4; i++) {
	m->c[i].r[0] *= x;
	m->c[i].r[1] *= y;
	m->c[i].r[2] *= z;
    }
}

static void m_project(matrix *m, GLfloat w, GLfloat h, GLfloat n, GLfloat f)
{
    memset(m, 0, sizeof(*m));
#ifdef PROJECT_ORTHO
    m->c[0].r[0] = 1 / w;
    m->c[1].r[1] = 1 / h;
    m->c[2].r[2] = 2.0f / (f - n);
    m->c[3].r[2] = -(f + n) / (f - n);
    m->c[3].r[3] = 1.0f;
#else
    m->c[0].r[0] = n / w;
    m->c[1].r[1] = n / h;
    m->c[2].r[2] = (f + n) / (f - n);
    m->c[2].r[3] = 1.0f;
    m->c[3].r[2] = -2.0f * f * n / (f - n);
#endif
}

static matrix modelworld;
static matrix eye;
static matrix projection;

#if 0
typedef struct {
} *gltex;

static gltex gltex_wall;
static gltex gltex_ladder;
static gltex gltex_egg;
static gltex gltex_grain;
static gltex gltex_lift;

static gltex gltex_duck[10];
static gltex gltex_big_duck_l1;
static gltex gltex_big_duck_l2;
static gltex gltex_big_duck_r1;
static gltex gltex_big_duck_r2;
static gltex gltex_cage_open;
static gltex gltex_cage_closed;

static gltex gltex_player_r[4];
static gltex gltex_player_l[4];
static gltex gltex_player_up[4];

static gltex gltex_score;
static gltex gltex_blank;
static gltex gltex_player;
static gltex gltex_level;
static gltex gltex_bonus;
static gltex gltex_time;
static gltex gltex_hat;
static gltex gltex_digit[10];
#endif

static GLuint make_buffer(GLenum target, const void *buffer_data,
			  GLsizei buffer_size) {
    GLuint buffer;

    glGenBuffers(1, &buffer);
    glBindBuffer(target, buffer);
    glBufferData(target, buffer_size, buffer_data, GL_STATIC_DRAW);
    return buffer;
}

static void LoadModel(sprite_model *s)
{
    s->vec_vbo = make_buffer(GL_ARRAY_BUFFER, s->vec,
				  sizeof(GLfloat) * s->vec_count * 3);
    s->ind_vbo = make_buffer(GL_ARRAY_BUFFER, s->ind,
				  sizeof(GLushort) * s->ind_count);
}
static void LoadTextures(void)
{
    LoadModel(&model_farmer);
    LoadModel(&model_duck);
    LoadModel(&model_ladder);
    LoadModel(&model_wall);
    LoadModel(&model_egg);
    LoadModel(&model_grain);
}

/* A = B * C */
static void m_mul(matrix *a, matrix *b, matrix *c)
{
    int i;
    int j;
    int k;
    GLfloat v;

    for (i = 0; i < 4; i++) {
	for (j = 0; j < 4; j++) {
	    v = 0.0f;
	    for (k = 0; k < 4; k++)
		v += b->c[k].r[j] * c->c[i].r[k];
	    a->c[i].r[j] = v;
	}
    }
}

/* Transform from model to world coordinates.  */
static void m_swizzle(matrix *m, int x, int y, int rot)
{
    memset(m, 0, sizeof(*m));
    switch (rot) {
    default:
	m->c[0].r[0] = 2.0f;
	m->c[1].r[2] = 2.0f;
	break;
    case 1:
	m->c[1].r[0] = -2.0f;
	m->c[0].r[2] = 2.0f;
	break;
    case 2:
	m->c[0].r[0] = -2.0f;
	m->c[1].r[2] = -2.0f;
	break;
    case 3:
	m->c[1].r[0] = 2.0f;
	m->c[0].r[2] = -2.0f;
	break;
    }
    m->c[2].r[1] = 2.0f;
    m->c[3].r[3] = 1.0f;

    m->c[3].r[0] = x * 2 - 160;
    m->c[3].r[1] = y - 120;
    m->c[3].r[2] = DEPTH;
}

static void RenderGroup(object_group *g)
{
    matrix prev = eye;
    matrix mat;
    object_group *child;

    m_translate(&eye, g->translate[0], g->translate[1], g->translate[2]);

    for (child = g->child; child; child = child->next) {
	RenderGroup(child);
    }

    if (g->ind_count) {
	glUniform3f(param_color, g->color[0], g->color[1], g->color[2]);
	m_mul(&mat, &modelworld, &eye);
	glUniformMatrix4fv(param_world, 1, GL_FALSE, &mat.c[0].r[0]);
	glDrawElements(GL_TRIANGLES, g->ind_count, GL_UNSIGNED_SHORT,
		       (void *)(g->ind_start * sizeof(GLushort)));
    }
    eye = prev;
}

static void RenderSprite(sprite_model *s, int x, int y, int rot)
{
    glUniformMatrix4fv(param_proj, 1, GL_FALSE, &projection.c[0].r[0]);
    glBindBuffer(GL_ARRAY_BUFFER, s->vec_vbo);
    glVertexAttribPointer(attr_position, 3, GL_FLOAT, GL_FALSE,
       	sizeof(GLfloat) * 3, (void *)0);
    glEnableVertexAttribArray(attr_position);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s->ind_vbo);
    m_identity(&eye);
    m_swizzle(&modelworld, x, y, rot);
    RenderGroup(s->group);
    glDisableVertexAttribArray(attr_position);
}

#if 0
static void RenderDigit(int x, int y, int n)
{
    RenderSprite(gltex_digit[n], x, y);
}

static void AddSceneSprite(gltex t, GLfloat x, GLfloat y)
{
    RenderSprite(t, x, y);
}

static void RenderPlayerHUD(int player)
{
    int x = player * 0x22 + 0x1b;
    int n;

    for (n = 0; n < 6; n++) {
	RenderDigit(x + 1 + n * 5, 0xef,
		    all_player_data[player].score[n + 2]);
    }

    n = all_player_data[player].lives;
    if (n > 8)
      n = 8;
    while (n--) {
	RenderSprite(gltex_hat, x, 0xe7);
	x += 4;
    }
}

static int item_pos[32];
static int item_count;
#endif

void RenderBackground(void)
{
}

#if 0
static void RenderScene(void)
{
    int player;
    int x;
    int y;
    int n;
    int type;
    gltex tex;

    AddSceneSprite(gltex_score, 0, 0xf0);
    for (player = 0; player < num_players; player++) {
	x = player * 0x22 + 0x1b;
	AddSceneSprite(gltex_blank, x, 0xf0);
    }

    y = 0xe3;
    AddSceneSprite(gltex_player, 0, y + 1);
    AddSceneSprite(gltex_digit[current_player + 1], 0x1b, y);

    AddSceneSprite(gltex_level, 0x24, y + 1);
    n = current_level + 1;
    AddSceneSprite(gltex_digit[n % 10], 0x45, y);
    n /= 10;
    AddSceneSprite(gltex_digit[n % 10], 0x40, y);
    if (n > 10)
	AddSceneSprite(gltex_digit[n / 10], 0x3b, y);

    AddSceneSprite(gltex_bonus, 0x4e, y + 1);
    AddSceneSprite(gltex_digit[0], 0x75, y);
    AddSceneSprite(gltex_time, 0x7e, y + 1);

    item_count = 0;
    for (x = 0; x < 20; x++) {
	for (y = 0; y < 25; y++) {
	    type = levelmap[y * 20 + x];
	    if (type & TILE_LADDER) {
		tex = gltex_ladder;
	    } else if (type & TILE_WALL) {
		tex = gltex_wall;
	    } else if (type & (TILE_EGG | TILE_GRAIN)) {
		item_pos[item_count++] = y * 20 + x;
		continue;
	    } else {
		continue;
	    }
	    AddSceneSprite(tex, x << 3, (y << 3) | 7);
	}
    }

    if (have_big_duck) {
	tex = gltex_cage_open;
    } else {
	tex = gltex_cage_closed;
    }
    AddSceneSprite(tex, 0, 0xdc);
}
#endif

void RenderFrame(void)
{
    int x;
    int y;
    int n;
    int type;
    int rot;
    sprite_model *model;

    if (skip_frame) {
	return;
    }
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(program);

    /* Egg/Grain.  */
    for (x = 0; x < 20; x++) {
	for (y = 0; y < 25; y++) {
	    type = levelmap[y * 20 + x];
	    if (type & TILE_LADDER) {
		model = &model_ladder;
	    } else if (type & TILE_WALL) {
		model = &model_wall;
	    } else if (type & TILE_EGG) {
		model = &model_egg;
	    } else if (type & TILE_GRAIN) {
		model = &model_grain;
	    } else {
		continue;
	    }
	    RenderSprite(model, (x << 3) + 4, (y << 3) + 7, 0);
	}
    }

    /* Player.  */
    if (player_face == 0) {
	rot = 2;
	n = (player_y >> 1) & 3;
    } else {
	if (player_face < 0)
	  rot = 3;
	else
	  rot = 1;
	n = (player_x >> 1) & 3;
    }
    if (player_mode != PLAYER_CLIMB) {
	if (move_x == 0)
	  n = 0;
    } else {
	if (move_y == 0)
	  n = 0;
    }
    RenderSprite(&model_farmer, player_x + 4, player_y - 8, rot);

    /* Ducks.  */
    for (n = 0; n < num_ducks; n++) {
	int dir = duck[n].dir;
	x = duck[n].x;
	if (dir == DIR_R) {
	    rot = 1;
	} else if (dir == DIR_L) {
	    rot = 3;
	} else {
	    rot = 2;
	}
	switch (duck[n].mode) {
	case DUCK_BORED:
	case DUCK_STEP:
	    break;
	default:
	    rot = 0;
	    break;
	}
	RenderSprite(&model_duck, x + 4, duck[n].y - 12, rot);
    }

#if  0
    RenderScene();

    /* HUD  */
    for (n = 0; n < num_players; n++) {
	RenderPlayerHUD(n);
    }

    y = 0xe3;

    RenderDigit(0x66, y, bonus[0]);
    RenderDigit(0x6b, y, bonus[1]);
    RenderDigit(0x70, y, bonus[2]);

    RenderDigit(0x91, y, timer_ticks[0]);
    RenderDigit(0x96, y, timer_ticks[1]);
    RenderDigit(0x9b, y, timer_ticks[2]);


    /* Lift.  */
    if (have_lift) {
	for (n = 0; n < 2; n++) {
	    RenderSprite(gltex_lift, lift_x, lift_y[n]);
	}
    }

    /* Big duck.  */
    if (big_duck_dir) {
	tex = big_duck_frame ? gltex_big_duck_l2 : gltex_big_duck_l1;
    } else {
	tex = big_duck_frame ? gltex_big_duck_r2 : gltex_big_duck_r1;
    }
    RenderSprite(tex, big_duck_x, big_duck_y);
#endif

    SDL_GL_SwapBuffers();
    if (glGetError() != GL_NO_ERROR) {
	die("Error rendering frame\n");
    }
}

static void parse_args(int argc, const char *argv[])
{
    const char *p;
    int i;

    for (i = 1; i < argc; i++) {
	p = argv[i];
	if (*p == '-')
	  p++;
	if (*p >= '1' && *p <= '9')
	  scale = *p - '0';
	else if (*p == 'f')
	  fullscreen = 1;
	else if (*p == 'h') {
	    printf("Usage: chuckie -123456789f\n");
	    exit(0);
	}
    }
}

static void InitGL(void)
{
    GLint program_ok;

    //glewInit();
    glDisable(GL_DEPTH_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glClearColor(0, 0, 0, 0);
    LoadTextures();

    vertex_shader = make_shader(GL_VERTEX_SHADER, vertex_code);
    fragment_shader = make_shader(GL_FRAGMENT_SHADER, fragment_code);

    program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &program_ok);
    if (!program_ok) {
        fprintf(stderr, "Failed to link shader program:\n");
        show_info_log(program, glGetProgramiv, glGetProgramInfoLog);
        glDeleteProgram(program);
        exit(1);
    }
    attr_position = glGetAttribLocation(program, "position");
    param_color = glGetUniformLocation(program, "color");
    param_world = glGetUniformLocation(program, "world");
    param_proj = glGetUniformLocation(program, "proj");

    glViewport(0, 0, scale * 320, scale * 240);
    m_project(&projection, 160, 120, DEPTH - 50.0f, DEPTH + 50.0f);
    if (glGetError() != GL_NO_ERROR) {
	die("Error initializing OpenGL\n");
    }
}

int main(int argc, const char *argv[])
{
  parse_args (argc, argv);
  int flags;

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == -1) {
      die("SDL_Init: %s\n", SDL_GetError());
  }

  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  flags = SDL_OPENGL;
  if (fullscreen)
    flags |= SDL_FULLSCREEN;

  sdlscreen = SDL_SetVideoMode(scale * 320, scale * 240, 32, flags);
  if (!sdlscreen) {
      SDL_Quit();
      fprintf(stderr, "SDL_SetVideoMode failed\n");
      return 0;
  }

  SDL_ShowCursor(SDL_DISABLE);

  init_sound();

  InitGL();

  SDL_AddTimer(30, do_timer, NULL);

  run_game();

  return 0;
}
