// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "simple_handler.h"

#include <sstream>
#include <string>

#include "include/base/cef_bind.h"
#include "include/cef_app.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_helpers.h"

// DCHECK on gl errors.
#if DCHECK_IS_ON()
#define VERIFY_NO_ERROR                                                      \
  {                                                                          \
    int _gl_error = glGetError();                                            \
    DCHECK(_gl_error == GL_NO_ERROR) << "glGetError returned " << _gl_error; \
  }
#else
#define VERIFY_NO_ERROR
#endif

namespace {

    SimpleHandler* g_instance = NULL;

}  // namespace

SimpleHandler::SimpleHandler(bool use_views)
  : use_views_(use_views), is_closing_(false) {
  DCHECK(!g_instance);
  g_instance = this;

  Initialize();
}

SimpleHandler::~SimpleHandler() {
  g_instance = NULL;
}

// static
SimpleHandler* SimpleHandler::GetInstance() {
  return g_instance;
}

void SimpleHandler::OnTitleChange(CefRefPtr<CefBrowser> browser,
                                  const CefString& title) {
  CEF_REQUIRE_UI_THREAD();

  if (use_views_) {
    // Set the title of the window using the Views framework.
    CefRefPtr<CefBrowserView> browser_view =
      CefBrowserView::GetForBrowser(browser);
    if (browser_view) {
      CefRefPtr<CefWindow> window = browser_view->GetWindow();
      if (window)
        window->SetTitle(title);
    }
  } else {
    // Set the title of the window using platform APIs.
    PlatformTitleChange(browser, title);
  }
}

void SimpleHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  // Add to the list of existing browsers.
  browser_list_.push_back(browser);
}

bool SimpleHandler::DoClose(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  // Closing the main window requires special handling. See the DoClose()
  // documentation in the CEF header for a detailed destription of this
  // process.
  if (browser_list_.size() == 1) {
    // Set a flag to indicate that the window close should be allowed.
    is_closing_ = true;
  }

  // Allow the close. For windowed browsers this will result in the OS close
  // event being sent.
  return false;
}

void SimpleHandler::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  // Remove from the list of existing browsers.
  BrowserList::iterator bit = browser_list_.begin();
  for (; bit != browser_list_.end(); ++bit) {
    if ((*bit)->IsSame(browser)) {
      browser_list_.erase(bit);
      break;
    }
  }

  if (browser_list_.empty()) {
    // All browser windows have closed. Quit the application message loop.
    CefQuitMessageLoop();
  }
}

void SimpleHandler::OnLoadError(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                ErrorCode errorCode,
                                const CefString& errorText,
                                const CefString& failedUrl) {
  CEF_REQUIRE_UI_THREAD();

  // Don't display an error for downloaded files.
  if (errorCode == ERR_ABORTED)
    return;

  // Display a load error message.
  std::stringstream ss;
  ss << "<html><body bgcolor=\"white\">"
        "<h2>Failed to load URL "
     << std::string(failedUrl) << " with error " << std::string(errorText)
     << " (" << errorCode << ").</h2></body></html>";
  frame->LoadString(ss.str(), failedUrl);
}

void SimpleHandler::CloseAllBrowsers(bool force_close) {
  if (!CefCurrentlyOn(TID_UI)) {
    // Execute on the UI thread.
    CefPostTask(TID_UI, base::Bind(&SimpleHandler::CloseAllBrowsers, this,
                                   force_close));
    return;
  }

  if (browser_list_.empty())
    return;

  BrowserList::const_iterator it = browser_list_.begin();
  for (; it != browser_list_.end(); ++it)
    (*it)->GetHost()->CloseBrowser(force_close);
}

void SimpleHandler::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) {
  printf("wtf:GetViewRect\n");

  rect = CefRect(0, 0, 480, 320);
}

void SimpleHandler::OnPaint(CefRefPtr<CefBrowser> browser,
                            PaintElementType type,
                            const RectList& dirtyRects,
                            const void* buffer,
                            int width,
                            int height) {
  printf("wtf:OnPaint:width=%d, height=%d\n", width, height);

  if (!initialized_)
    Initialize();

  if (IsTransparent()) {
    // Enable alpha blending.
    glEnable(GL_BLEND);
    VERIFY_NO_ERROR;
  }

  // Enable 2D textures.
  glEnable(GL_TEXTURE_2D);
  VERIFY_NO_ERROR;

  DCHECK_NE(texture_id_, 0U);
  glBindTexture(GL_TEXTURE_2D, texture_id_);
  VERIFY_NO_ERROR;

  if (type == PET_VIEW) {
    int old_width = view_width_;
    int old_height = view_height_;

    view_width_ = width;
    view_height_ = height;

    if (settings_.show_update_rect)
      update_rect_ = dirtyRects[0];

    glPixelStorei(GL_UNPACK_ROW_LENGTH, view_width_);
    VERIFY_NO_ERROR;

    if (old_width != view_width_ || old_height != view_height_ ||
        (dirtyRects.size() == 1 &&
         dirtyRects[0] == CefRect(0, 0, view_width_, view_height_))) {
      // Update/resize the whole texture.
      glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
      VERIFY_NO_ERROR;
      glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
      VERIFY_NO_ERROR;
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, view_width_, view_height_, 0,
                   GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, buffer);
      VERIFY_NO_ERROR;
    } else {
      // Update just the dirty rectangles.
      CefRenderHandler::RectList::const_iterator i = dirtyRects.begin();
      for (; i != dirtyRects.end(); ++i) {
        const CefRect& rect = *i;
        DCHECK(rect.x + rect.width <= view_width_);
        DCHECK(rect.y + rect.height <= view_height_);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, rect.x);
        VERIFY_NO_ERROR;
        glPixelStorei(GL_UNPACK_SKIP_ROWS, rect.y);
        VERIFY_NO_ERROR;
        glTexSubImage2D(GL_TEXTURE_2D, 0, rect.x, rect.y, rect.width,
                        rect.height, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                        buffer);
        VERIFY_NO_ERROR;
      }
    }
  } else if (type == PET_POPUP && popup_rect_.width > 0 &&
             popup_rect_.height > 0) {
    int skip_pixels = 0, x = popup_rect_.x;
    int skip_rows = 0, y = popup_rect_.y;
    int w = width;
    int h = height;

    // Adjust the popup to fit inside the view.
    if (x < 0) {
      skip_pixels = -x;
      x = 0;
    }
    if (y < 0) {
      skip_rows = -y;
      y = 0;
    }
    if (x + w > view_width_)
      w -= x + w - view_width_;
    if (y + h > view_height_)
      h -= y + h - view_height_;

    // Update the popup rectangle.
    glPixelStorei(GL_UNPACK_ROW_LENGTH, width);
    VERIFY_NO_ERROR;
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, skip_pixels);
    VERIFY_NO_ERROR;
    glPixelStorei(GL_UNPACK_SKIP_ROWS, skip_rows);
    VERIFY_NO_ERROR;
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_BGRA,
                    GL_UNSIGNED_INT_8_8_8_8_REV, buffer);
    VERIFY_NO_ERROR;
  }

  // Disable 2D textures.
  glDisable(GL_TEXTURE_2D);
  VERIFY_NO_ERROR;

  if (IsTransparent()) {
    // Disable alpha blending.
    glDisable(GL_BLEND);
    VERIFY_NO_ERROR;
  }
}

bool SimpleHandler::IsTransparent() {
  return CefColorGetA(settings_.background_color) == 0;
};

void SimpleHandler::Initialize() {

  if (initialized_)
    return;

  // init settings
  settings_.show_update_rect = false;

#if defined(OS_WIN)
  settings->shared_texture_enabled = shared_texture_enabled_;
#endif
  settings_.external_begin_frame_enabled = false;
  settings_.begin_frame_rate = 0;
//  settings_.background_color = browser_background_color_;

  glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
  VERIFY_NO_ERROR;

  if (IsTransparent()) {
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    VERIFY_NO_ERROR;
  } else {
    glClearColor(float(CefColorGetR(settings_.background_color)) / 255.0f,
                 float(CefColorGetG(settings_.background_color)) / 255.0f,
                 float(CefColorGetB(settings_.background_color)) / 255.0f,
                 1.0f);
    VERIFY_NO_ERROR;
  }

  // Necessary for non-power-of-2 textures to render correctly.
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  VERIFY_NO_ERROR;

  // Create the texture.
  glGenTextures(1, &texture_id_);
  VERIFY_NO_ERROR;
  DCHECK_NE(texture_id_, 0U);
  VERIFY_NO_ERROR;

  glBindTexture(GL_TEXTURE_2D, texture_id_);
  VERIFY_NO_ERROR;
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  VERIFY_NO_ERROR;
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  VERIFY_NO_ERROR;
  glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  VERIFY_NO_ERROR;

  initialized_ = true;
}

void SimpleHandler::Render() {
  if (view_width_ == 0 || view_height_ == 0)
    return;

  DCHECK(initialized_);

  struct {
      float tu, tv;
      float x, y, z;
  } static vertices[] = {{0.0f, 1.0f, -1.0f, -1.0f, 0.0f},
                         {1.0f, 1.0f, 1.0f, -1.0f, 0.0f},
                         {1.0f, 0.0f, 1.0f, 1.0f, 0.0f},
                         {0.0f, 0.0f, -1.0f, 1.0f, 0.0f}};

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  VERIFY_NO_ERROR;

  glMatrixMode(GL_MODELVIEW);
  VERIFY_NO_ERROR;
  glLoadIdentity();
  VERIFY_NO_ERROR;

  // Match GL units to screen coordinates.
  glViewport(0, 0, view_width_, view_height_);
  VERIFY_NO_ERROR;
  glMatrixMode(GL_PROJECTION);
  VERIFY_NO_ERROR;
  glLoadIdentity();
  VERIFY_NO_ERROR;

  // Draw the background gradient.
  glPushAttrib(GL_ALL_ATTRIB_BITS);
  VERIFY_NO_ERROR;
  // Don't check for errors until glEnd().
  glBegin(GL_QUADS);
  glColor4f(1.0, 0.0, 0.0, 1.0);  // red
  glVertex2f(-1.0, -1.0);
  glVertex2f(1.0, -1.0);
  glColor4f(0.0, 0.0, 1.0, 1.0);  // blue
  glVertex2f(1.0, 1.0);
  glVertex2f(-1.0, 1.0);
  glEnd();
  VERIFY_NO_ERROR;
  glPopAttrib();
  VERIFY_NO_ERROR;

  // Rotate the view based on the mouse spin.
  if (spin_x_ != 0) {
    glRotatef(-spin_x_, 1.0f, 0.0f, 0.0f);
    VERIFY_NO_ERROR;
  }
  if (spin_y_ != 0) {
    glRotatef(-spin_y_, 0.0f, 1.0f, 0.0f);
    VERIFY_NO_ERROR;
  }

  if (IsTransparent()) {
    // Alpha blending style. Texture values have premultiplied alpha.
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    VERIFY_NO_ERROR;

    // Enable alpha blending.
    glEnable(GL_BLEND);
    VERIFY_NO_ERROR;
  }

  // Enable 2D textures.
  glEnable(GL_TEXTURE_2D);
  VERIFY_NO_ERROR;

  // Draw the facets with the texture.
  DCHECK_NE(texture_id_, 0U);
  VERIFY_NO_ERROR;
  glBindTexture(GL_TEXTURE_2D, texture_id_);
  VERIFY_NO_ERROR;
  glInterleavedArrays(GL_T2F_V3F, 0, vertices);
  VERIFY_NO_ERROR;
  glDrawArrays(GL_QUADS, 0, 4);
  VERIFY_NO_ERROR;

  // Disable 2D textures.
  glDisable(GL_TEXTURE_2D);
  VERIFY_NO_ERROR;

  if (IsTransparent()) {
    // Disable alpha blending.
    glDisable(GL_BLEND);
    VERIFY_NO_ERROR;
  }

  // Draw a rectangle around the update region.
  if (settings_.show_update_rect && !update_rect_.IsEmpty()) {
    int left = update_rect_.x;
    int right = update_rect_.x + update_rect_.width;
    int top = update_rect_.y;
    int bottom = update_rect_.y + update_rect_.height;

#if defined(OS_LINUX)
    // Shrink the box so that top & right sides are drawn.
    top += 1;
    right -= 1;
#else
    // Shrink the box so that left & bottom sides are drawn.
    left += 1;
    bottom -= 1;
#endif

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    VERIFY_NO_ERROR
    glMatrixMode(GL_PROJECTION);
    VERIFY_NO_ERROR;
    glPushMatrix();
    VERIFY_NO_ERROR;
    glLoadIdentity();
    VERIFY_NO_ERROR;
    glOrtho(0, view_width_, view_height_, 0, 0, 1);
    VERIFY_NO_ERROR;

    glLineWidth(1);
    VERIFY_NO_ERROR;
    glColor3f(1.0f, 0.0f, 0.0f);
    VERIFY_NO_ERROR;
    // Don't check for errors until glEnd().
    glBegin(GL_LINE_STRIP);
    glVertex2i(left, top);
    glVertex2i(right, top);
    glVertex2i(right, bottom);
    glVertex2i(left, bottom);
    glVertex2i(left, top);
    glEnd();
    VERIFY_NO_ERROR;

    glPopMatrix();
    VERIFY_NO_ERROR;
    glPopAttrib();
    VERIFY_NO_ERROR;
  }
}
