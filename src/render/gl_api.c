
#include "gl_api.h"
#include "log/logger.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

PFNGLCREATESHADERPROC glCreateShader                         = nullptr;
PFNGLSHADERSOURCEPROC glShaderSource                         = nullptr;
PFNGLCOMPILESHADERPROC glCompileShader                       = nullptr;
PFNGLGETSHADERIVPROC glGetShaderiv                           = nullptr;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog                 = nullptr;
PFNGLDELETESHADERPROC glDeleteShader                         = nullptr;
PFNGLCREATEPROGRAMPROC glCreateProgram                       = nullptr;
PFNGLATTACHSHADERPROC glAttachShader                         = nullptr;
PFNGLLINKPROGRAMPROC glLinkProgram                           = nullptr;
PFNGLGETPROGRAMIVPROC glGetProgramiv                         = nullptr;
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog               = nullptr;
PFNGLDELETEPROGRAMPROC glDeleteProgram                       = nullptr;
PFNGLGENVERTEXARRAYSPROC glGenVertexArrays                   = nullptr;
PFNGLGENBUFFERSPROC glGenBuffers                             = nullptr;
PFNGLBINDVERTEXARRAYPROC glBindVertexArray                   = nullptr;
PFNGLBINDBUFFERPROC glBindBuffer                             = nullptr;
PFNGLBUFFERDATAPROC glBufferData                             = nullptr;
PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer           = nullptr;
PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray   = nullptr;
PFNGLUSEPROGRAMPROC glUseProgram                             = nullptr;
PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation             = nullptr;
PFNGLUNIFORM1IPROC glUniform1i                               = nullptr;
PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays             = nullptr;
PFNGLACTIVETEXTUREPROC glActiveTexture                       = nullptr;
PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers                   = nullptr;
PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer                   = nullptr;
PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D         = nullptr;
PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus     = nullptr;
PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers             = nullptr;
PFNGLDELETEBUFFERSPROC glDeleteBuffers                       = nullptr;
PFNGLUNIFORM2FPROC glUniform2f                               = nullptr;
PFNGLUNIFORM3FPROC glUniform3f                               = nullptr;
PFNGLBINDATTRIBLOCATIONPROC glBindAttribLocation             = nullptr;
PFNGLGETATTRIBLOCATIONPROC glGetAttribLocation               = nullptr;
PFNGLGENERATEMIPMAPPROC glGenerateMipmap                     = nullptr;
PFNGLGETUNIFORMBLOCKINDEXPROC glGetUniformBlockIndex         = nullptr;
PFNGLUNIFORMBLOCKBINDINGPROC glUniformBlockBinding           = nullptr;
PFNGLBINDBUFFERRANGEPROC glBindBufferRange                   = nullptr;
PFNGLUNIFORM1FPROC glUniform1f                               = nullptr;
PFNGLUNIFORM4FPROC glUniform4f                               = nullptr;
PFNGLUNIFORM2FVPROC glUniform2fv                             = nullptr;
PFNGLUNIFORM4FVPROC glUniform4fv                             = nullptr;
PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv                 = nullptr;
PFNGLBLENDFUNCSEPARATEPROC glBlendFuncSeparate               = nullptr;
PFNGLSTENCILOPSEPARATEPROC glStencilOpSeparate               = nullptr;
PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray = nullptr;

#define LOAD(name)                                                    \
    ({                                                                \
        GLFWglproc p = glfwGetProcAddress(#name);                     \
        if (!p)                                                       \
        {                                                             \
            log_error("Failed to load OpenGL function: %s\n", #name); \
            return -1;                                                \
        }                                                             \
        *(void **)(&name) = (void *)p;                                \
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
    LOAD(glDeleteBuffers);
    LOAD(glUniform2f);
    LOAD(glUniform3f);
    LOAD(glBindAttribLocation);
    LOAD(glGetAttribLocation);
    LOAD(glGenerateMipmap);
    LOAD(glGetUniformBlockIndex);
    LOAD(glUniformBlockBinding);
    LOAD(glBindBufferRange);
    LOAD(glUniform1f);
    LOAD(glUniform4f);
    LOAD(glUniform2fv);
    LOAD(glUniform4fv);
    LOAD(glUniformMatrix4fv);
    LOAD(glBlendFuncSeparate);
    LOAD(glStencilOpSeparate);
    LOAD(glDisableVertexAttribArray);

    return 0;
}
