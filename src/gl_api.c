
#include "gl_api.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <stdio.h>

PFNGLCREATESHADERPROC glCreateShader                       = NULL;
PFNGLSHADERSOURCEPROC glShaderSource                       = NULL;
PFNGLCOMPILESHADERPROC glCompileShader                     = NULL;
PFNGLGETSHADERIVPROC glGetShaderiv                         = NULL;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog               = NULL;
PFNGLDELETESHADERPROC glDeleteShader                       = NULL;
PFNGLCREATEPROGRAMPROC glCreateProgram                     = NULL;
PFNGLATTACHSHADERPROC glAttachShader                       = NULL;
PFNGLLINKPROGRAMPROC glLinkProgram                         = NULL;
PFNGLGETPROGRAMIVPROC glGetProgramiv                       = NULL;
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog             = NULL;
PFNGLDELETEPROGRAMPROC glDeleteProgram                     = NULL;
PFNGLGENVERTEXARRAYSPROC glGenVertexArrays                 = NULL;
PFNGLGENBUFFERSPROC glGenBuffers                           = NULL;
PFNGLBINDVERTEXARRAYPROC glBindVertexArray                 = NULL;
PFNGLBINDBUFFERPROC glBindBuffer                           = NULL;
PFNGLBUFFERDATAPROC glBufferData                           = NULL;
PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer         = NULL;
PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray = NULL;
PFNGLUSEPROGRAMPROC glUseProgram                           = NULL;
PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation           = NULL;
PFNGLUNIFORM1IPROC glUniform1i                             = NULL;
PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays           = NULL;
PFNGLACTIVETEXTUREPROC glActiveTexture                     = NULL;

#define LOAD(name)                                                          \
    ({                                                                      \
        GLFWglproc p = glfwGetProcAddress(#name);                           \
        if (!p)                                                             \
        {                                                                   \
            fprintf(stderr, "Failed to load OpenGL function: %s\n", #name); \
            return -1;                                                      \
        }                                                                   \
        *(void **)(&name) = (void *)p;                                      \
    })

int gl_api_load(void)
{
    LOAD(glCreateShader);
    LOAD(glShaderSource);
    LOAD(glCompileShader);
    LOAD(glGetShaderiv);
    LOAD(glGetShaderInfoLog);
    LOAD(glDeleteShader);
    LOAD(glCreateProgram);
    LOAD(glAttachShader);
    LOAD(glLinkProgram);
    LOAD(glGetProgramiv);
    LOAD(glGetProgramInfoLog);
    LOAD(glDeleteProgram);
    LOAD(glGenVertexArrays);
    LOAD(glGenBuffers);
    LOAD(glBindVertexArray);
    LOAD(glBindBuffer);
    LOAD(glBufferData);
    LOAD(glVertexAttribPointer);
    LOAD(glEnableVertexAttribArray);
    LOAD(glUseProgram);
    LOAD(glGetUniformLocation);
    LOAD(glUniform1i);
    LOAD(glDeleteVertexArrays);
    LOAD(glActiveTexture);
    return 0;
}
