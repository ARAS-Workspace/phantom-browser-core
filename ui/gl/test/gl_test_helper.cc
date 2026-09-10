// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/test/gl_test_helper.h"

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/containers/heap_array.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/init/gl_factory.h"

namespace gl {
// static
GLuint GLTestHelper::CreateTexture(GLenum target) {
  // Create the texture object.
  GLuint texture = 0;
  glGenTextures(1, &texture);
  glBindTexture(target, texture);
  glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  return texture;
}

// static
GLuint GLTestHelper::SetupFramebuffer(int width, int height) {
  GLuint color_buffer_texture = CreateTexture(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, color_buffer_texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, nullptr);
  GLuint framebuffer = 0;
  glGenFramebuffersEXT(1, &framebuffer);
  glBindFramebufferEXT(GL_FRAMEBUFFER, framebuffer);
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                            color_buffer_texture, 0);
  if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
              glCheckFramebufferStatusEXT(GL_FRAMEBUFFER))
        << "Error setting up framebuffer";
    glDeleteFramebuffersEXT(1, &framebuffer);
    framebuffer = 0;
  }
  glBindFramebufferEXT(GL_FRAMEBUFFER, 0);
  glDeleteTextures(1, &color_buffer_texture);
  return framebuffer;
}

std::pair<scoped_refptr<GLSurface>, scoped_refptr<GLContext>>
GLTestHelper::CreateOffscreenGLSurfaceAndContext() {
  scoped_refptr<GLSurface> gl_surface = init::CreateOffscreenGLSurface(
      gl::GLSurfaceEGL::GetGLDisplayEGL(), gfx::Size());

  scoped_refptr<GLContext> context =
      gl::init::CreateGLContext(nullptr, gl_surface.get(), GLContextAttribs());
  EXPECT_TRUE(context->MakeCurrent(gl_surface.get()));
  return std::make_pair(std::move(gl_surface), std::move(context));
}

}  // namespace gl
