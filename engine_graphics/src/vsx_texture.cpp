/**
* Project: VSXu: Realtime modular visual programming engine.
*
* This file is part of Vovoid VSXu.
*
* @author Jonatan Wallmander, Robert Wenzel, Vovoid Media Technologies AB Copyright (C) 2003-2013
* @see The GNU Lesser General Public License (LGPL)
*
* VSXu Engine is free software; you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
* for more details.
*
* You should have received a copy of the GNU Lesser General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/


#include <vsxfst.h>
#include <vsx_gl_global.h>
#include <vsx_texture.h>
#ifndef VSX_TEXTURE_NO_GLPNG
  #include <vsxg.h>
  #include <stdlib.h>
  #ifdef _WIN32
    #include <pthread.h>
  #else
    #include <pthread.h>
  #endif
#endif

#include <vsx_gl_state.h>

#ifdef VSXU_EXE
  std::map<vsx_string, vsx_texture_info> vsx_texture::t_glist;
#else
  void* vsx_texture::t_glist;
#endif

vsx_texture::vsx_texture()
{
  gl_state = 0x0;
  pti_l = 0;
  valid = false;
  is_multisample = false;
  valid_fbo = false;
  locked = false;
  transform_obj = new vsx_transform_neutral;
  original_transform_obj = 1;
  depth_buffer_local = true;
}

vsx_texture::vsx_texture(int id, int type)
{
  gl_state = 0x0;
  pti_l = 0;
  texture_info.ogl_id = id;
  texture_info.ogl_type = type;
  transform_obj = new vsx_transform_neutral;
  is_multisample = false;
  valid = true;
  valid_fbo = false;
  locked = false;
  #ifndef VSXU_OPENGL_ES
    glewInit();
  #endif
  depth_buffer_local = true;
}


void vsx_texture::set_gl_state(void* n)
{
  gl_state = n;
}

void vsx_texture::init_opengl_texture_1d()
{
  GLuint tex_id;
  glGenTextures(1, &tex_id);
  texture_info.ogl_id = tex_id;
  texture_info.ogl_type = GL_TEXTURE_1D;
}


void vsx_texture::init_opengl_texture_2d()
{
  GLuint tex_id;
  glGenTextures(1, &tex_id);
  texture_info.ogl_id = tex_id;
  texture_info.ogl_type = GL_TEXTURE_2D;
}


bool vsx_texture::has_buffer_support()
{
  bool fbo = GLEW_EXT_framebuffer_object;
  bool blit = GLEW_EXT_framebuffer_blit;
  if (!fbo) printf("vsx_texture Notice: EXT_framebuffer_object support is MISSING! This will mean limited functionality.\n\n");
  if (!blit) printf("vsx_texture Notice: EXT_framebuffer_blit support is MISSING! This will mean limited functionality.\n\n");
  return fbo && blit;
}


GLuint vsx_texture::get_depth_buffer_handle()
{
  return depth_buffer_handle;
}


void init_render_buffer()
{
  uint fboID, ColorBufferID, DepthBufferID;
    int samples;
    char ErrorMessage[1024];
    //We need to find out what the maximum supported samples is
    glGetIntegerv(GL_MAX_SAMPLES_EXT, &samples);

    //----------------------
    //If for example, 16 is returned, then you can attempt to make a FBO with samples=0 to 16
    //0 means no multisample. This is like using glFramebufferRenderbufferEXT instead of glRenderbufferStorageMultisampleEXT
    //You can attempt to make sample from 1 to 16, but some of them might fail
    //Now, let's make a FBO
    glGenFramebuffersEXT(1, &fboID);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fboID);



    //----------------------
    //Now make a multisample color buffer
    glGenRenderbuffersEXT(1, &ColorBufferID);
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, ColorBufferID);



    //samples=4, format=GL_RGBA8, width=256, height=256
    glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, 4, GL_RGBA8, 256, 256);



    //----------------------
    //Make a depth multisample depth buffer
    //You must give it the same samples as the color RB, same width and height as well
    //else you will either get a GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_EXT or some other error
    glGenRenderbuffersEXT(1, &DepthBufferID);
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, DepthBufferID);


    //samples=4, format=GL_DEPTH_COMPONENT24, width=256, height=256
    glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, 4, GL_DEPTH_COMPONENT24, 256, 256);


    //----------------------
    //It's time to attach the RBs to the FBO
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, ColorBufferID);
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, DepthBufferID);
}

void vsx_texture::init_feedback_buffer(
  int width,
  int height,
  bool float_texture,
  bool alpha,
  bool enable_multisample
)
{
  if (!gl_state) { vsx_printf("vsx_texture::init_feedback_buffer: vsx_texture gl_state not set!\n"); return; }
  locked = false;
  prev_buf = 0;
  #ifndef VSXU_OPENGL_ES
    glewInit();
  #endif
  int i_width = width;
  int i_height = height;

  if ( !has_buffer_support() )
  {
    printf("vsx_texture error: No FBO available!\n");
    return;
  }

  // set the buffer type (for deinit and capturing)
  frame_buffer_type = VSX_TEXTURE_BUFFER_TYPE_FEEDBACK_PBUFFER;

  int prev_buf_l;
  GLuint tex_id;
  prev_buf_l = ((vsx_gl_state*)gl_state)->framebuffer_bind_get();

  // color buffer
  glGenRenderbuffersEXT(1, &color_buffer_handle);

  glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, color_buffer_handle);

  if(enable_multisample && GLEW_EXT_framebuffer_multisample)
  {
    if (float_texture)
    {
      glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, 4, alpha?GL_RGBA16F_ARB:GL_RGB16F_ARB, width, height);
    }
    else
    {
      glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, 4, alpha?GL_RGBA8:GL_RGB8, width, height);
    }
  }
  else
  {
    if (float_texture)
    {
      glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, alpha?GL_RGBA16F_ARB:GL_RGB16F_ARB, width, height);
    }
    else
    {
      glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, alpha?GL_RGBA8:GL_RGB8, width, height);
    }
  }

  glGenRenderbuffersEXT(1, &depth_buffer_handle);
  glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, depth_buffer_handle);
  if(enable_multisample && GLEW_EXT_framebuffer_multisample)
  {
    glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, 4, GL_DEPTH_COMPONENT, width, height);
  }
  else
  {
    glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT, width, height);
  }

  // create fbo for multi sampled content and attach depth and color buffers to it
  glGenFramebuffersEXT(1, &frame_buffer_handle);
  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, frame_buffer_handle);
  glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, color_buffer_handle);
  glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, depth_buffer_handle);

  // create texture
  glGenTextures(1, &tex_id);
  glBindTexture(GL_TEXTURE_2D, tex_id);
  if (float_texture)
  {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F_ARB, i_width, i_height, 0, GL_RGBA, GL_FLOAT, NULL);
  }
  else
  {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, i_width, i_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  }
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL,0);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  // set your texture parameters here if required ...

  // create final fbo and attach texture to it
  glGenFramebuffersEXT(1, &frame_buffer_object_handle);
  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, frame_buffer_object_handle);
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, tex_id, 0);

  // TODO: create and attach depth buffer also


  texture_info.ogl_id = tex_id;
  texture_info.ogl_type = GL_TEXTURE_2D;
  texture_info.size_x = width;
  texture_info.size_y = height;

  ((vsx_gl_state*)gl_state)->framebuffer_bind(prev_buf_l);

  valid = true; // valid for binding
  valid_fbo = true; // valid for capturing
}


void vsx_texture::reinit_feedback_buffer
(
  int width,
  int height,
  bool float_texture,
  bool alpha,
  bool multisample
)
{
  deinit_buffer();
  init_feedback_buffer
  (
    width,
    height,
    float_texture,
    alpha,
    multisample
  );
}

// init an offscreen color buffer, no depth
VSX_TEXTURE_DLLIMPORT void vsx_texture::init_color_buffer
(
  int width, // width in pixels
  int height, // height in pixels
  bool float_texture, // use floating point channels (8-bit is default)
  bool alpha, // support alpha channel or not
  bool multisample // enable anti-aliasing
)
{
  VSX_UNUSED(multisample);
  if (!gl_state) { vsx_printf("vsx_texture::init_color_buffer: vsx_texture gl_state not set!\n"); return; }
  locked = false;
  prev_buf = 0;
  #ifndef VSXU_OPENGL_ES
    glewInit();
  #endif
  int i_width = width;
  int i_height = height;

  if ( !has_buffer_support() )
  {
    printf("vsx_texture error: No FBO available!\n");
    return;
  }

  // set the buffer type (for deinit and capturing)
  frame_buffer_type = VSX_TEXTURE_BUFFER_TYPE_COLOR;

  // save the previous FBO (stack behaviour)
  int prev_buf_l;
  prev_buf_l = ((vsx_gl_state*)gl_state)->framebuffer_bind_get();
//  glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, (GLint *)&prev_buf_l);

  GLuint texture_storage_type;

  if (float_texture)
  {
    texture_storage_type = alpha?GL_RGBA16F_ARB:GL_RGB16F_ARB;
  }
  else
  {
    texture_storage_type = alpha?GL_RGBA8:GL_RGB8;
  }

  //RGBA8 2D texture, 24 bit depth texture, 256x256
  glGenTextures(1, &color_buffer_handle);
  glBindTexture(GL_TEXTURE_2D, color_buffer_handle);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  //NULL means reserve texture memory, but texels are undefined
  glTexImage2D(GL_TEXTURE_2D, 0, texture_storage_type, i_width, i_height, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);

  //-------------------------
  glGenFramebuffersEXT(1, &frame_buffer_handle);
  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, frame_buffer_handle);
  //Attach 2D texture to this FBO
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, color_buffer_handle, 0/*mipmap level*/);
  //-------------------------
  //Does the GPU support current FBO configuration?
  GLenum status;
  status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
  switch(status)
  {
     case GL_FRAMEBUFFER_COMPLETE_EXT:
      texture_info.ogl_id = color_buffer_handle;
      texture_info.ogl_type = GL_TEXTURE_2D;
      texture_info.size_x = width;
      texture_info.size_y = height;
            valid = true; // valid for binding
      valid_fbo = true; // valid for capturing
      break;
    default:
    break;
  }
  ((vsx_gl_state*)gl_state)->framebuffer_bind(prev_buf_l);
}

// run in stop/start or when changing resolution
VSX_TEXTURE_DLLIMPORT void vsx_texture::reinit_color_buffer
(
  int width, // width in pixels
  int height, // height in pixels
  bool float_texture, // use floating point channels (8-bit is default)
  bool alpha, // support alpha channel or not
  bool multisample // enable anti-aliasing
)
{
  deinit_buffer();
  init_color_buffer
  (
    width,
    height,
    float_texture,
    alpha,
    multisample
  );

}

// init an offscreen color + depth buffer (as textures)
// See http://www.opengl.org/wiki/Framebuffer_Object_Examples
VSX_TEXTURE_DLLIMPORT void vsx_texture::init_color_depth_buffer
(
  int width, // width in pixels
  int height, // height in pixels
  bool float_texture, // use floating point channels (8-bit is default)
  bool alpha, // support alpha channel or not
  bool multisample, // enable anti-aliasing
  GLuint existing_depth_texture_id
)
{
  if (!gl_state) { vsx_printf("vsx_texture::init_color_depth_buffer: vsx_texture gl_state not set!\n"); return; }
  locked = false;
  prev_buf = 0;
  #ifndef VSXU_OPENGL_ES
    glewInit();
  #endif
  int i_width = width;
  int i_height = height;

  if ( !has_buffer_support() )
  {
    printf("vsx_texture error: No FBO available!\n");
    return;
  }

  // set the buffer type (for deinit and capturing)
  frame_buffer_type = VSX_TEXTURE_BUFFER_TYPE_COLOR_DEPTH;


  // save the previous FBO (stack behaviour)
  int prev_buf_l;
  prev_buf_l = ((vsx_gl_state*)gl_state)->framebuffer_bind_get();
  //GLuint tex_id;
//  glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, (GLint *)&prev_buf_l);

  GLuint texture_storage_type;

  if (float_texture)
  {
    texture_storage_type = alpha?GL_RGBA16F_ARB:GL_RGB16F_ARB;
  }
  else
  {
    texture_storage_type = alpha?GL_RGBA8:GL_RGB8;
  }

  GLuint texture_type;
//  texture_type = GL_TEXTURE_2D;
  texture_type = GL_TEXTURE_2D_MULTISAMPLE;




  // Generate Color Texture
  //RGBA8 2D texture, 24 bit depth texture, 256x256
  glGenTextures(1, &color_buffer_handle);
  glBindTexture(GL_TEXTURE_2D, color_buffer_handle);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  //NULL means reserve texture memory, but texels are undefined
  glTexImage2D(GL_TEXTURE_2D, 0, texture_storage_type, i_width, i_height, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);







  // Generate Depth Texture
  if (existing_depth_texture_id != 0)
  {
    depth_buffer_handle = existing_depth_texture_id;
    depth_buffer_local = false;
  } else
  {
    glGenTextures(1, &depth_buffer_handle);
    glBindTexture(GL_TEXTURE_2D, depth_buffer_handle);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_TEXTURE_MODE, GL_INTENSITY);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    //NULL means reserve texture memory, but texels are undefined
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, i_width, i_height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL);
    depth_buffer_local = true;
  }






  // GENERATE FBO
  //-------------------------
  glGenFramebuffersEXT(1, &frame_buffer_handle);
  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, frame_buffer_handle);



//  if (multisample)
//  {

//    // CREATE RENDER BUFFERS


//    //----------------------
//    //Now make a multisample color buffer
//    glGenRenderbuffersEXT(1, &render_buffer_color_handle);
//    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, render_buffer_color_handle);



//    //samples=4, format=GL_RGBA8, width=256, height=256
//    glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, 4, GL_RGBA8, i_width, i_height);



//    //----------------------
//    //Make a depth multisample depth buffer
//    //You must give it the same samples as the color RB, same width and height as well
//    //else you will either get a GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_EXT or some other error
//    glGenRenderbuffersEXT(1, &render_buffer_depth_handle);
//    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, render_buffer_depth_handle);


//    //samples=4, format=GL_DEPTH_COMPONENT24, width=256, height=256
//    glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, 4, GL_DEPTH_COMPONENT24, i_width, i_height);


//    //----------------------
//    //It's time to attach the RBs to the FBO
//    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, render_buffer_color_handle);
//    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, render_buffer_depth_handle);

//  }


  // Attach textures to the FBO


  //Attach 2D texture to this FBO
  glFramebufferTexture2DEXT(
                              GL_FRAMEBUFFER_EXT,
                              GL_COLOR_ATTACHMENT0_EXT,
                              GL_TEXTURE_2D,
                              color_buffer_handle,
                              0/*mipmap level*/
                           );
  //-------------------------
  //Attach depth texture to FBO
  glFramebufferTexture2DEXT(
        GL_FRAMEBUFFER_EXT,
        GL_DEPTH_ATTACHMENT_EXT,
        GL_TEXTURE_2D,
        depth_buffer_handle,
        0/*mipmap level*/
  );
  //-------------------------



  //Does the GPU support current FBO configuration?
  GLenum status;
  status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
  switch(status)
  {
     case GL_FRAMEBUFFER_COMPLETE_EXT:
      texture_info.ogl_id = color_buffer_handle;
      texture_info.ogl_type = GL_TEXTURE_2D;
      texture_info.size_x = width;
      texture_info.size_y = height;
//      glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, prev_buf_l);
      valid = true; // valid for binding
      valid_fbo = true; // valid for capturing
      break;
    default:
    break;
  }
  ((vsx_gl_state*)gl_state)->framebuffer_bind(prev_buf_l);
}



//http://www.opengl.org/wiki/GL_EXT_framebuffer_multisample


// run in stop/start or when changing resolution
VSX_TEXTURE_DLLIMPORT void vsx_texture::reinit_color_depth_buffer
(
  int width,
  int height,
  bool float_texture,
  bool alpha,
  bool multisample,
  GLuint existing_depth_texture_id
)
{
  deinit_buffer();
  init_color_depth_buffer
  (
    width,
    height,
    float_texture,
    alpha,
    multisample,
    existing_depth_texture_id
  );
}

void vsx_texture::deinit_buffer()
{
  if (!valid_fbo) return;
  if (!gl_state) { vsx_printf("vsx_texture::deinit_buffer: vsx_texture gl_state not set!\n"); return; }
  if (frame_buffer_type == VSX_TEXTURE_BUFFER_TYPE_FEEDBACK_PBUFFER)
  {
    #ifndef VSXU_OPENGL_ES
      glDeleteRenderbuffersEXT(1,&color_buffer_handle);
      glDeleteRenderbuffersEXT(1,&depth_buffer_handle);
      glDeleteTextures(1,&texture_info.ogl_id);
      glDeleteFramebuffersEXT(1, &frame_buffer_handle);
      glDeleteFramebuffersEXT(1, &frame_buffer_object_handle);
    #endif
    return;
  }
  if (frame_buffer_type == VSX_TEXTURE_BUFFER_TYPE_COLOR)
  {
    //Delete resources
    glDeleteTextures(1, &color_buffer_handle);
    depth_buffer_handle = 0;
    depth_buffer_local = 0;
    //Bind 0, which means render to back buffer, as a result, fb is unbound
    glDeleteFramebuffersEXT(1, &frame_buffer_handle);
    return;
  }
  if (frame_buffer_type == VSX_TEXTURE_BUFFER_TYPE_COLOR_DEPTH)
  {
    //Delete resources
    glDeleteTextures(1, &color_buffer_handle);
    if (depth_buffer_local)
    {
      glDeleteTextures(1, &depth_buffer_handle);
    }
    depth_buffer_handle = 0;
    depth_buffer_local = 0;
    //Bind 0, which means render to back buffer, as a result, fb is unbound
    if ( ((vsx_gl_state*)gl_state)->framebuffer_bind_get() == frame_buffer_handle )
      ((vsx_gl_state*)gl_state)->framebuffer_bind(0);
    glDeleteFramebuffersEXT(1, &frame_buffer_handle);
    return;
  }
}


void vsx_texture::begin_capture_to_buffer()
{
  if (!gl_state) { vsx_printf("vsx_texture::begin_capture_to_buffer: vsx_texture gl_state not set!\n"); return; }
  if (!valid_fbo) return;
  if (locked) return;
  #ifndef VSXU_OPENGL_ES
    prev_buf = ((vsx_gl_state*)gl_state)->framebuffer_bind_get();
    glPushAttrib(GL_ALL_ATTRIB_BITS );
  #endif
    ((vsx_gl_state*)gl_state)->matrix_get_v( VSX_GL_PROJECTION_MATRIX, buffer_save_matrix[0].m );
    ((vsx_gl_state*)gl_state)->matrix_get_v( VSX_GL_MODELVIEW_MATRIX, buffer_save_matrix[1].m );
    ((vsx_gl_state*)gl_state)->matrix_get_v( VSX_GL_TEXTURE_MATRIX, buffer_save_matrix[2].m );


    ((vsx_gl_state*)gl_state)->matrix_mode( VSX_GL_PROJECTION_MATRIX );
    ((vsx_gl_state*)gl_state)->matrix_load_identity();
    ((vsx_gl_state*)gl_state)->matrix_mode( VSX_GL_MODELVIEW_MATRIX );
    ((vsx_gl_state*)gl_state)->matrix_load_identity();
    ((vsx_gl_state*)gl_state)->matrix_mode( VSX_GL_TEXTURE_MATRIX );
    ((vsx_gl_state*)gl_state)->matrix_load_identity();

  glEnable(GL_BLEND);

  #ifdef VSXU_OPENGL_ES_1_0
    glBindTexture(GL_TEXTURE_2D,0);
    glBindFramebufferOES(GL_FRAMEBUFFER_OES, framebuffer_id);
  #endif
  #ifndef VSXU_OPENGL_ES
    ((vsx_gl_state*)gl_state)->framebuffer_bind(frame_buffer_handle);
  #endif

  glViewport(0,0,(int)texture_info.size_x, (int)texture_info.size_y);

  locked = true;
}

void vsx_texture::end_capture_to_buffer()
{
  if (!gl_state) { vsx_printf("vsx_texture::end_capture_to_buffer: vsx_texture gl_state not set!\n"); return; }
  if (!valid_fbo) return;
  if (locked)
  {

    if (frame_buffer_type == VSX_TEXTURE_BUFFER_TYPE_FEEDBACK_PBUFFER)
    {
    #ifndef VSXU_OPENGL_ES_2_0
      glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, multisample_render_buffer_color_handle);
      glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, frame_buffer_object_handle);
      glBlitFramebufferEXT(0, 0, (GLint)texture_info.size_x-1, (GLint)texture_info.size_y-1, 0, 0, (GLint)texture_info.size_x-1, (GLint)texture_info.size_y-1, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    #endif
    }
    #ifdef VSXU_OPENGL_ES_1_0
      glBindFramebufferOES(GL_FRAMEBUFFER_OES, prev_buf);
    #endif
    #ifndef VSXU_OPENGL_ES
      ((vsx_gl_state*)gl_state)->framebuffer_bind(prev_buf);
//      glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, prev_buf);
    #endif
    ((vsx_gl_state*)gl_state)->matrix_mode( VSX_GL_PROJECTION_MATRIX );
    ((vsx_gl_state*)gl_state)->matrix_load_identity();
    ((vsx_gl_state*)gl_state)->matrix_mult_f( buffer_save_matrix[0].m );
    ((vsx_gl_state*)gl_state)->matrix_mode( VSX_GL_MODELVIEW_MATRIX );
    ((vsx_gl_state*)gl_state)->matrix_load_identity();
    ((vsx_gl_state*)gl_state)->matrix_mult_f( buffer_save_matrix[1].m );
    ((vsx_gl_state*)gl_state)->matrix_mode( VSX_GL_TEXTURE_MATRIX );
    ((vsx_gl_state*)gl_state)->matrix_load_identity();
    ((vsx_gl_state*)gl_state)->matrix_mult_f( buffer_save_matrix[2].m );

    #ifndef VSXU_OPENGL_ES_2_0
      glPopAttrib();
    #endif
    locked = false;
  }
}

void vsx_texture::unload_all_active()
{
  for (std::map<vsx_string, vsx_texture_info>::iterator it = t_glist.begin(); it != t_glist.end(); ++it) {
    glDeleteTextures(1,&((*it).second.ogl_id));
  }
}

void vsx_texture::reinit_all_active()
{
  std::map<vsx_string, vsx_texture_info> temp_glist = t_glist;
  vsx_string tname;
  for (std::map<vsx_string, vsx_texture_info>::iterator it = temp_glist.begin(); it != temp_glist.end(); ++it) {
    if ((*it).second.type == 1)
    {
      tname = (*it).first;
      t_glist.erase(tname);
      load_png(tname);
    }
  }
}


void vsx_texture::upload_ram_bitmap_1d( void* data, unsigned long size, bool mipmaps, int bpp, int bpp2 )
{
  GLboolean oldStatus = glIsEnabled(texture_info.ogl_type);

  glEnable(texture_info.ogl_type);
  glBindTexture(texture_info.ogl_type, texture_info.ogl_id);




  if (mipmaps)
  {
    glTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
    glTexParameteri(texture_info.ogl_type, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(texture_info.ogl_type, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(texture_info.ogl_type, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
  } else
  {
    glTexParameteri(texture_info.ogl_type, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(texture_info.ogl_type, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(texture_info.ogl_type, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  }

  // no compression
  if (bpp == GL_RGBA32F_ARB)
  {
    glTexImage1D(
      texture_info.ogl_type,  // opengl type
      0, // mipmap level
      GL_RGBA32F_ARB, // internal format
      size, // size
      0, // border
      bpp2, // source data format
      GL_FLOAT, // source data type
      data
    );
  } else
  {
    if (bpp == 3)
    {
      glTexImage1D(
        texture_info.ogl_type,  // opengl type
        0,  // mipmap level
        GL_COMPRESSED_RGB_ARB, // storage type
        size, // size x
        0,      // border 0 or 1
        bpp2,   // source data format
        GL_UNSIGNED_BYTE, // source data type
        data // pointer to data
      );
    }
    else
    {
      glTexImage1D(
        texture_info.ogl_type,  // opengl type
        0,  // mipmap level
        GL_COMPRESSED_RGBA_ARB, // storage type
        size, // size x
        0,      // border 0 or 1
        bpp2,   // source data format
        GL_UNSIGNED_BYTE, // source data type
        data // pointer to data
      );
    }
  }



  if(!oldStatus)
  {
    glDisable(texture_info.ogl_type);
  }
  valid = true;

}

void vsx_texture::upload_ram_bitmap_2d(vsx_bitmap* vbitmap,bool mipmaps, bool upside_down)
{
  upload_ram_bitmap_2d(vbitmap->data, vbitmap->size_x, vbitmap->size_y,mipmaps,vbitmap->bpp, vbitmap->bformat,upside_down);
}

void vsx_texture::upload_ram_bitmap_2d(void* data, unsigned long size_x, unsigned long size_y, bool mipmaps, int bpp, int bpp2, bool upside_down)
{
  // prepare data
  if (upside_down)
  {
    //printf("texture is upside down\n");
    if (bpp == GL_RGBA32F_ARB)
    {
      GLfloat* data2 = new GLfloat[size_x * size_y * 4];
      int dy = 0;
      int sxbpp = size_x*4;
      for (int y = size_y-1; y >= 0; --y) {
        for (unsigned long x = 0; x < size_x*4; ++x) {
          data2[dy*sxbpp + x] = ((GLfloat*)data)[y*sxbpp + x];
        }
        ++dy;
      }
      data = (GLfloat*)data2;
    } else
    {
      unsigned char* data2 = new unsigned char[(size_x) * (size_y) * (bpp)];
      int dy = 0;
      int sxbpp = size_x*bpp;
      for (int y = size_y-1; y >= 0; --y)
      {
        //printf("y: %d\n",y);
        int dysxbpp = dy*sxbpp;
        int ysxbpp = y * sxbpp;
        for (size_t x = 0; x < size_x*bpp; ++x)
        {
          data2[dysxbpp + x] = ((unsigned char*)data)[ysxbpp + x];
        }
        ++dy;
      }
      data = (unsigned long*)data2;
    }
  }






  GLboolean oldStatus = glIsEnabled(texture_info.ogl_type);

  glEnable(texture_info.ogl_type);
  glBindTexture(texture_info.ogl_type, texture_info.ogl_id);

  if (mipmaps)
  {
    glTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
    glTexParameteri(texture_info.ogl_type, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(texture_info.ogl_type, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(texture_info.ogl_type, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
    float rMaxAniso;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &rMaxAniso);
    glTexParameterf( texture_info.ogl_type, GL_TEXTURE_MAX_ANISOTROPY_EXT, rMaxAniso);

  } else
  {
    glTexParameteri(texture_info.ogl_type, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(texture_info.ogl_type, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(texture_info.ogl_type, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  }

  // no compression
  if (bpp == GL_RGBA32F_ARB)
  {
    glTexImage2D(texture_info.ogl_type, 0,bpp , size_x, size_y, 0, bpp2, GL_FLOAT, data);
  } else
  {
    if (bpp == 3)
    {
      glTexImage2D(
        texture_info.ogl_type,  // opengl type
        0,  // mipmap level
        GL_COMPRESSED_RGB_ARB, // storage type
        size_x, // size x
        size_y, // size y
        0,      // border 0 or 1
        bpp2,   // source data format
        GL_UNSIGNED_BYTE, // source data type
        data // pointer to data
      );
    }
    else
    {
      glTexImage2D(
        texture_info.ogl_type,  // opengl type
        0,  // mipmap level
        GL_COMPRESSED_RGBA_ARB, // storage type
        size_x, // size x
        size_y, // size y
        0,      // border 0 or 1
        bpp2,   // source data format
        GL_UNSIGNED_BYTE, // source data type
        data // pointer to data
      );
    }
    // original:
    //       glTexImage2D(texture_info.ogl_type, 0,bpp , size_x, size_y, 0, bpp2, GL_UNSIGNED_BYTE, data);
  }

  if (upside_down)
  {
    if (bpp == GL_RGBA32F_ARB)
    {
      delete[] (GLfloat*)data;
    } else
    {
      delete[] (unsigned long*)data;
    }
  }

  this->texture_info.size_x = size_x;
  this->texture_info.size_y = size_y;

  if(!oldStatus)
  {
    glDisable(texture_info.ogl_type);
  }
  valid = true;
}

void vsx_texture::load_png(vsx_string fname, bool mipmaps, vsxf* filesystem)
{
  if (t_glist.find(fname) != t_glist.end()) {
    //printf("already found png: %s\n",fname.c_str());
    locked = true;
    texture_info = t_glist[fname];
    return;
  } else
  {
    locked = false;
    vsxf* i_filesystem = 0x0;
    //printf("processing png: %s\n",fname.c_str());
    if (filesystem == 0x0)
    {
      i_filesystem = new vsxf;
      filesystem = i_filesystem;
    }
    pngRawInfo* pp = new pngRawInfo;
    if (pngLoadRaw(fname.c_str(), pp, filesystem))
    {
      this->name = fname;
      init_opengl_texture_2d();
      if (pp->Components == 1)
      upload_ram_bitmap_2d((unsigned long*)(pp->Data),pp->Width,pp->Height,mipmaps,3,GL_RGB);
      if (pp->Components == 2)
      upload_ram_bitmap_2d((unsigned long*)(pp->Data),pp->Width,pp->Height,mipmaps,4,GL_RGBA);
      if (pp->Components == 3)
      upload_ram_bitmap_2d((unsigned long*)(pp->Data),pp->Width,pp->Height,mipmaps,pp->Components,GL_RGB);
      if (pp->Components == 4)
      upload_ram_bitmap_2d((unsigned long*)(pp->Data),pp->Width,pp->Height,mipmaps,pp->Components,GL_RGBA);
      free(pp->Data);
      texture_info.type = 1; // png

      t_glist[fname] = texture_info;
    }
    delete pp;
    if (i_filesystem) delete i_filesystem;
  }
}

void vsx_texture::load_jpeg(vsx_string fname, bool mipmaps) {
    CJPEGTest cj;
    vsx_string ret;
    vsxf filesystem;
    cj.LoadJPEG(fname,ret,&filesystem);
    upload_ram_bitmap_2d((unsigned long*)cj.m_pBuf, cj.GetResX(), cj.GetResY(), mipmaps, 3, GL_RGB);
}

// PNG THREAD STUFF
typedef struct {
  pngRawInfo*       pp;
  int               thread_state;
  pthread_t					worker_t;
  pthread_attr_t		worker_t_attr;
  vsx_bitmap        bitmap;
  vsx_string       filename;
  bool              mipmaps;
} pti; // png thread info

// thread run by the load_png_thread
void* png_worker(void *ptr) {
  ((pti*)((vsx_texture*)ptr)->pti_l)->pp = new pngRawInfo;
  vsxf filesystem;
  if (pngLoadRaw(((pti*)((vsx_texture*)ptr)->pti_l)->filename.c_str(), ((pti*)((vsx_texture*)ptr)->pti_l)->pp,&filesystem))
  {
    ((pti*)((vsx_texture*)ptr)->pti_l)->thread_state = 2;
  }
  return 0;
}

// load a png but put the heavy processing in a thread
void vsx_texture::load_png_thread(vsx_string fname, bool mipmaps)
{
  if (t_glist.find(fname) != t_glist.end()) {
    locked = true;
    texture_info = t_glist[fname];
    this->name = fname;
    return;
  } else
  {
    locked = false;
    if (pti_l) {
      if (((pti*)pti_l)->thread_state == 1) {
        long* aa;
        pthread_join(((pti*)pti_l)->worker_t, (void **)&aa);
      }
      free(((pti*)pti_l)->pp->Data);
      free(pti_l);
    }
    this->name = fname;
    valid = false;
    pti* pt = new pti;
    pt->filename = fname;
    pt->mipmaps = mipmaps;
    pthread_attr_init(&pt->worker_t_attr);
    pt->thread_state = 1;
    pti_l = (void*)pt;
    pthread_create(&(pt->worker_t), &(pt->worker_t_attr), &png_worker, (void*)this);
  }
}

bool vsx_texture::bind()
{
    if (pti_l)
    if (((pti*)pti_l)->thread_state == 2)
    {
      if (texture_info.ogl_id != 0)
      unload();
      init_opengl_texture_2d();
      pngRawInfo* pp = (pngRawInfo*)(((pti*)pti_l)->pp);
      if (pp->Components == 1)
      upload_ram_bitmap_2d((unsigned long*)(pp->Data),pp->Width,pp->Height,((pti*)pti_l)->mipmaps,3,GL_RGB); else
      if (pp->Components == 2)
      upload_ram_bitmap_2d((unsigned long*)(pp->Data),pp->Width,pp->Height,((pti*)pti_l)->mipmaps,4,GL_RGBA); else
      if (pp->Components == 3)
      upload_ram_bitmap_2d((unsigned long*)(pp->Data),pp->Width,pp->Height,((pti*)pti_l)->mipmaps,pp->Components,GL_RGB); else
      if (pp->Components == 4)
      upload_ram_bitmap_2d((unsigned long*)(pp->Data),pp->Width,pp->Height,((pti*)pti_l)->mipmaps,pp->Components,GL_RGBA);
      free((((pti*)pti_l)->pp)->Data);
      texture_info.type = 1; // png
      t_glist[name] = texture_info;
      pthread_join(((pti*)pti_l)->worker_t,0);
      valid = true;
      delete (pti*)pti_l;
      pti_l = 0;
    }
  if (texture_info.ogl_id == 0)
  {
    return false;
  }
  glEnable(texture_info.ogl_type);
  glBindTexture(texture_info.ogl_type,texture_info.ogl_id);
  return true;
}


void vsx_texture::_bind()
{
  if (texture_info.ogl_id == 0)
    return;
  glDisable(texture_info.ogl_type);
}

void vsx_texture::texcoord2f(float x, float y) {
  #ifdef VSXU_OPENGL_ES
    printf("NO vsx_texture::texcoord2f support on OpenGL ES!\n");
  #else
    if (texture_info.ogl_type == GL_TEXTURE_RECTANGLE_EXT) {
      glTexCoord2i((GLuint)(x*texture_info.size_x),(GLuint)(y*texture_info.size_y));
    } else {
      glTexCoord2f(x,y);
    }
  #endif
}

void vsx_texture::unload()
{
  if (texture_info.ogl_id != 0)
  {
    if (name != "" && t_glist.find(name) != t_glist.end())
    {
      if (!locked)
      {
        t_glist.erase(name);
        glDeleteTextures(1,&(texture_info.ogl_id));
      }
    } else
    if (locked)
    {
      glDeleteTextures(1,&(texture_info.ogl_id));
    }
    texture_info.ogl_id = 0;
    texture_info.ogl_type = 0;
    valid = false;
  }
}
