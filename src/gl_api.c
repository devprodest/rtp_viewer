
#include "gl_api.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <stdio.h>

PFNGLCREATESHADERPROC glCreateShader                       = nullptr;
PFNGLSHADERSOURCEPROC glShaderSource                       = nullptr;
PFNGLCOMPILESHADERPROC glCompileShader                     = nullptr;
PFNGLGETSHADERIVPROC glGetShaderiv                         = nullptr;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog               = nullptr;
PFNGLDELETESHADERPROC glDeleteShader                       = nullptr;
PFNGLCREATEPROGRAMPROC glCreateProgram                     = nullptr;
PFNGLATTACHSHADERPROC glAttachShader                       = nullptr;
PFNGLLINKPROGRAMPROC glLinkProgram                         = nullptr;
PFNGLGETPROGRAMIVPROC glGetProgramiv                       = nullptr;
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog             = nullptr;
PFNGLDELETEPROGRAMPROC glDeleteProgram                     = nullptr;
PFNGLGENVERTEXARRAYSPROC glGenVertexArrays                 = nullptr;
PFNGLGENBUFFERSPROC glGenBuffers                           = nullptr;
PFNGLBINDVERTEXARRAYPROC glBindVertexArray                 = nullptr;
PFNGLBINDBUFFERPROC glBindBuffer                           = nullptr;
PFNGLBUFFERDATAPROC glBufferData                           = nullptr;
PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer         = nullptr;
PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray = nullptr;
PFNGLUSEPROGRAMPROC glUseProgram                           = nullptr;
PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation           = nullptr;
PFNGLUNIFORM1IPROC glUniform1i                             = nullptr;
PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays           = nullptr;
PFNGLACTIVETEXTUREPROC glActiveTexture                     = nullptr;
PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers                 = nullptr;
PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer                 = nullptr;
PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D       = nullptr;
PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus   = nullptr;
PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers           = nullptr;

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
    LOAD(glGenFramebuffers);
    LOAD(glBindFramebuffer);
    LOAD(glFramebufferTexture2D);
    LOAD(glCheckFramebufferStatus);
    LOAD(glDeleteFramebuffers);

    return 0;
}
