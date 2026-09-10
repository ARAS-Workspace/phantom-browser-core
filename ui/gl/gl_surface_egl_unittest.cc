// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface_egl.h"

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_surface_test_support.h"

namespace gl {

namespace {

class GLSurfaceEGLTest : public testing::Test {
 protected:
  void SetUp() override {
    display_ = GLSurfaceTestSupport::InitializeOneOffImplementation(
        GLImplementationParts(kGLImplementationEGLANGLE));
  }

  void TearDown() override { GLSurfaceTestSupport::ShutdownGL(display_); }

  GLDisplay* GetTestDisplay() {
    EXPECT_NE(display_, nullptr);
    return display_;
  }

 private:
  raw_ptr<GLDisplay> display_ = nullptr;
};

}  // namespace
}  // namespace gl
