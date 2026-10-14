#pragma once

#include "gl_api.h"

GLuint gl_utils_compile_shader(const char * source, GLenum type);
GLuint gl_utils_build_program(const char * fragment_source);
GLuint gl_utils_create_fullscreen_quad(void);