/* OpenGL 2.0 setup and rendering code.
   Copyright (C) 2011 Paul Brook
   This code is licenced under the GNU GPL v3 */
#include "chuckie.h"

#include <math.h>
#include <SDL.h>
#define GL_GLEXT_PROTOTYPES

#include "raster.h"
#include "model.h"

#define DEPTH 1280.0f

static int fullscreen = 0;
static int scale = 2;
static int shaders = 1;
extern int ortho;

extern float rotate_x;
extern float rotate_y;

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
uniform mat4 proj; \n\
void main() \n\
{ \n\
    gl_Position = proj * vec4(position, 1.0); \n\
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

static void m_rotate_x(matrix *m, GLfloat a)
{
    memset(m, 0, sizeof(*m));
    m->c[0].r[0] = 1.0f;
    m->c[2].r[1] = sin(a);
    m->c[1].r[1] = cos(a);
    m->c[2].r[2] = cos(a);
    m->c[1].r[2] = -sin(a);
    m->c[3].r[3] = 1.0f;
}

static void m_rotate_y(matrix *m, GLfloat a)
{
    memset(m, 0, sizeof(*m));
    m->c[0].r[0] = cos(a);
    m->c[2].r[0] = -sin(a);
    m->c[1].r[1] = 1.0f;
    m->c[0].r[2] = sin(a);
    m->c[2].r[2] = cos(a);
    m->c[3].r[3] = 1.0f;
}

static void m_rotate_z(matrix *m, GLfloat a)
{
    memset(m, 0, sizeof(*m));
    m->c[0].r[0] = cos(a);
    m->c[1].r[0] = sin(a);
    m->c[2].r[2] = 1.0f;
    m->c[0].r[1] = -sin(a);
    m->c[1].r[1] = cos(a);
    m->c[3].r[3] = 1.0f;
}

static void m_project(matrix *m, GLfloat w, GLfloat h)
{
    GLfloat n, f;
    memset(m, 0, sizeof(*m));
    if (ortho) {
	n = -160;
	f = 160;
	m->c[0].r[0] = 1 / w;
	m->c[1].r[1] = 1 / h;
	m->c[2].r[2] = 2.0f / (f - n);
	m->c[3].r[2] = -(f + n) / (f - n);
	m->c[3].r[3] = 1.0f;
    } else{
	n = DEPTH - 160;
	f = DEPTH + 160;
	m->c[0].r[0] = n / w;
	m->c[1].r[1] = n / h;
	m->c[2].r[2] = (f + n) / (f - n);
	m->c[2].r[3] = 1.0f;
	m->c[3].r[2] = -2.0f * f * n / (f - n);
    }
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
    LoadModel(&model_lift);
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
    matrix w;
    matrix r;
    matrix tmp;

    memset(&w, 0, sizeof(w));

    switch (rot) {
    default:
	w.c[0].r[0] = 2.0f;
	w.c[1].r[2] = 2.0f;
	break;
    case 1:
	w.c[1].r[0] = -2.0f;
	w.c[0].r[2] = 2.0f;
	break;
    case 2:
	w.c[0].r[0] = -2.0f;
	w.c[1].r[2] = -2.0f;
	break;
    case 3:
	w.c[1].r[0] = 2.0f;
	w.c[0].r[2] = -2.0f;
	break;
    }
    w.c[2].r[1] = 2.0f;
    w.c[3].r[3] = 1.0f;

    w.c[3].r[0] = x * 2 - 160;
    w.c[3].r[1] = y - 120;
    m_rotate_y(&r, rotate_x);
    m_mul(&tmp, &r, &w);
    m_rotate_x(&r, rotate_y);
    m_mul(m, &r, &tmp);
    if (!ortho) {
	m->c[3].r[2] += DEPTH;
    }
}

static float sprite_angle1;
static float sprite_angle2;

static void RenderGroup(object_group *g)
{
    matrix prev = eye;
    matrix mat;
    matrix final;
    object_group *child;

    switch (g->mod) {
    case 'L':
	m_rotate_x(&mat, sprite_angle1);
	break;
    case 'R':
	m_rotate_x(&mat, -sprite_angle1);
	break;
    case 'B':
	m_rotate_x(&mat, sprite_angle2 / 2.0f);
	break;
    case 'N':
    case 'l':
	m_rotate_x(&mat, sprite_angle2);
	break;
    case 'r':
	m_rotate_x(&mat, -sprite_angle2);
	break;
    case 'U':
	m_rotate_x(&mat, fabsf(sprite_angle1) / 2.0f);
	break;
    case 'D':
	m_rotate_x(&mat, -fabsf(sprite_angle1) / 2.0f);
	break;
    default:
	m_identity(&mat);
	break;
    }

    m_translate(&mat, g->translate[0], g->translate[1], g->translate[2]);
    m_mul(&eye, &prev, &mat);


    for (child = g->child; child; child = child->next) {
	RenderGroup(child);
    }

    if (g->ind_count) {
	m_mul(&mat, &modelworld, &eye);
	m_mul(&final, &projection, &mat);
	if (shaders) {
	    glUniform3f(param_color, g->color[0], g->color[1], g->color[2]);
	    glUniformMatrix4fv(param_proj, 1, GL_FALSE, &final.c[0].r[0]);
	} else {
	    glLoadMatrixf(&final.c[0].r[0]);
	    glColor3f(g->color[0], g->color[1], g->color[2]);
	}
	glDrawElements(GL_TRIANGLES, g->ind_count, GL_UNSIGNED_SHORT,
		       (void *)(sizeof(GLushort) * g->ind_start));
    }
    eye = prev;
}

static void RenderSprite(sprite_model *s, int x, int y, int rot)
{
    glBindBuffer(GL_ARRAY_BUFFER, s->vec_vbo);
    if (shaders) {
	glUniformMatrix4fv(param_proj, 1, GL_FALSE, &projection.c[0].r[0]);
	glVertexAttribPointer(attr_position, 3, GL_FLOAT, GL_FALSE,
	    sizeof(GLfloat) * 3, (void *)0);
	glEnableVertexAttribArray(attr_position);
    } else {
	glVertexPointer(3, GL_FLOAT, sizeof(GLfloat) * 3, (void *)0);
	glEnableClientState(GL_VERTEX_ARRAY);
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s->ind_vbo);
    m_identity(&eye);
    m_swizzle(&modelworld, x, y, rot);
    RenderGroup(s->group);
    if (shaders) {
	glDisableVertexAttribArray(attr_position);
    }
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

    if (!ortho) {
	rotate_x -= 0.05;
	rotate_y += 0.005;
    } else {
	rotate_x = 0;
	rotate_y = 0;
    }

    if (skip_frame) {
	return;
    }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_project(&projection, 160, 120);

    if (shaders) {
	glUseProgram(program);
    }

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
    if (n == 1) {
	sprite_angle1 = M_PI / 4;
    } else if (n == 3) {
	sprite_angle1 = -M_PI / 4;
    } else {
	sprite_angle1 = 0.0f;
    }
    if (rot == 2) {
	sprite_angle2 = (M_PI / 2) - sprite_angle1;
    } else {
	sprite_angle2 = -sprite_angle1;
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
	    sprite_angle1 = 0.0f;
	    sprite_angle2 = 0.0f;
	    break;
	case DUCK_STEP:
	    sprite_angle1 = M_PI / 4;
	    if ((duck[n].x ^ duck[n].y) & 8) {
		sprite_angle1 = -sprite_angle1;
	    }
	    sprite_angle2 = 0.0f;
	    break;
	case DUCK_EAT2:
	case DUCK_EAT4:
	    sprite_angle1 = 0.0f;
	    sprite_angle2 = -M_PI / 4;
	    break;
	case DUCK_EAT3:
	    sprite_angle1 = 0.0f;
	    sprite_angle2 = -M_PI / 2;
	    break;
	default:
	    abort();
	}
	RenderSprite(&model_duck, x + 4, duck[n].y - 12, rot);
    }

    /* Lift.  */
    if (have_lift) {
	for (n = 0; n < 2; n++) {
	    RenderSprite(&model_lift, lift_x + 8, lift_y[n] + 4, 0);
	}
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
	else if (*p == 's')
	  shaders = 0;
	else if (*p == 'h') {
	    printf("Usage: chuckie -123456789fs\n");
	    exit(0);
	}
    }
}

static void InitGL(void)
{
    GLint program_ok;

    glEnable(GL_DEPTH_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glClearColor(0, 0, 0, 0);
    glEnable(GL_CULL_FACE);
    LoadTextures();

    if (shaders) {
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
	param_proj = glGetUniformLocation(program, "proj");
    }

    glViewport(0, 0, scale * 320, scale * 240);
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
