// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefsimple_capi/simple_views.h"

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "include/capi/views/cef_box_layout_capi.h"
#include "include/capi/views/cef_panel_capi.h"
#include "tests/cefsimple_capi/ref_counted.h"
#include "tests/cefsimple_capi/simple_handler.h"
#include "tests/cefsimple_capi/simple_utils.h"

static void LogMsg(const char* format, ...) {
  FILE* f = fopen("C:\\projects\\lite_browser\\debug_c.txt", "a");
  if (f) {
    va_list args;
    va_start(args, format);
    vfprintf(f, format, args);
    va_end(args);
    fclose(f);
  }
}

//

//
// Browser View Delegate Implementation
//

// Implement reference counting functions for browser view delegate.
IMPLEMENT_REFCOUNTING_SIMPLE(simple_browser_view_delegate_t,
                             browser_view_delegate,
                             ref_count)

// Returns the delegate for a popup BrowserView.
// Called before on_popup_browser_view_created.
cef_browser_view_delegate_t* CEF_CALLBACK
browser_view_delegate_get_delegate_for_popup_browser_view(
    cef_browser_view_delegate_t* self,
    cef_browser_view_t* browser_view,
    const cef_browser_settings_t* settings,
    cef_client_t* client,
    int is_devtools) {
  simple_browser_view_delegate_t* delegate =
      (simple_browser_view_delegate_t*)self;

  // Release parameters before returning.
  browser_view->base.base.release(&browser_view->base.base);
  // settings is a const pointer, not ref-counted
  client->base.release(&client->base);

  // Return this same delegate for the popup (matches C++ default).
  // Add a reference since we're returning to CEF.
  delegate->delegate.base.base.add_ref(&delegate->delegate.base.base);
  return &delegate->delegate;
}

// Called when a popup BrowserView is created.
// Return true if we handle creating the window for the popup.
int CEF_CALLBACK browser_view_delegate_on_popup_browser_view_created(
    cef_browser_view_delegate_t* self,
    cef_browser_view_t* browser_view,
    cef_browser_view_t* popup_browser_view,
    int is_devtools) {
  simple_browser_view_delegate_t* delegate =
      (simple_browser_view_delegate_t*)self;

  // Create a new top-level Window for the popup.
  // We transfer our reference to the window delegate.
  simple_window_delegate_t* window_delegate = window_delegate_create(
      NULL, popup_browser_view, delegate->runtime_style, CEF_SHOW_STATE_NORMAL);

  if (window_delegate) {
    // Create the window. The window will show itself after creation.
    // We transfer our window_delegate reference to CEF.
    cef_window_create_top_level(&window_delegate->delegate);
  }

  // Release parameters before returning.
  browser_view->base.base.release(&browser_view->base.base);
  // Don't release popup_browser_view - we transferred it to window_delegate.

  // We created the Window.
  return 1;
}

// Returns the runtime style for the browser.
cef_runtime_style_t CEF_CALLBACK
browser_view_delegate_get_browser_runtime_style(
    cef_browser_view_delegate_t* self) {
  simple_browser_view_delegate_t* delegate =
      (simple_browser_view_delegate_t*)self;
  return delegate->runtime_style;
}

cef_size_t CEF_CALLBACK browser_view_delegate_get_preferred_size(
    cef_view_delegate_t* self,
    cef_view_t* view) {
  simple_browser_view_delegate_t* delegate =
      (simple_browser_view_delegate_t*)self;
  cef_size_t size = {800, 600};

  if (delegate->preferred_height > 0) {
    size.height = delegate->preferred_height;
  }

  LogMsg("browser_view_delegate_get_preferred_size: is_ui_view=%d, width=%d, height=%d\n",
         delegate->is_ui_view, size.width, size.height);

  // Release view parameter before returning.
  view->base.release(&view->base);

  return size;
}

void CEF_CALLBACK browser_view_delegate_on_browser_created(
    cef_browser_view_delegate_t* self,
    cef_browser_view_t* browser_view,
    cef_browser_t* browser) {
  simple_browser_view_delegate_t* delegate =
      (simple_browser_view_delegate_t*)self;
  simple_handler_t* handler = simple_handler_get_instance();
  LogMsg("browser_view_delegate_on_browser_created: delegate=%p, is_ui_view=%d, handler=%p\n",
         delegate, delegate->is_ui_view, handler);
  
  // (child window creation moved to window created callback)

  // Release parameters before returning.
  browser_view->base.base.release(&browser_view->base.base);
  browser->base.release(&browser->base);
}

void CEF_CALLBACK browser_view_delegate_on_browser_destroyed(
    cef_browser_view_delegate_t* self,
    cef_browser_view_t* browser_view,
    cef_browser_t* browser) {
  // (global release logic removed)

  // Release parameters before returning.
  browser_view->base.base.release(&browser_view->base.base);
  browser->base.release(&browser->base);
}

// Creates a browser view delegate instance.
simple_browser_view_delegate_t* browser_view_delegate_create(
    cef_runtime_style_t runtime_style,
    int is_ui_view,
    int preferred_height) {
  simple_browser_view_delegate_t* delegate =
      (simple_browser_view_delegate_t*)calloc(
          1, sizeof(simple_browser_view_delegate_t));
  CHECK(delegate);

  // Initialize base structure.
  INIT_CEF_BASE_REFCOUNTED(&delegate->delegate.base.base,
                           cef_browser_view_delegate_t, browser_view_delegate);

  // Set callbacks.
  delegate->delegate.get_delegate_for_popup_browser_view =
      browser_view_delegate_get_delegate_for_popup_browser_view;
  delegate->delegate.on_popup_browser_view_created =
      browser_view_delegate_on_popup_browser_view_created;
  delegate->delegate.get_browser_runtime_style =
      browser_view_delegate_get_browser_runtime_style;
  delegate->delegate.on_browser_created =
      browser_view_delegate_on_browser_created;
  delegate->delegate.on_browser_destroyed =
      browser_view_delegate_on_browser_destroyed;

  // Set callbacks on parent structure.
  delegate->delegate.base.get_preferred_size =
      browser_view_delegate_get_preferred_size;

  // Store fields.
  delegate->runtime_style = runtime_style;
  delegate->is_ui_view = is_ui_view;
  delegate->preferred_height = preferred_height;

  // Initialize with ref count of 1.
  atomic_store(&delegate->ref_count, 1);

  return delegate;
}

//
// Window Delegate Implementation
//

// Implement reference counting functions for window delegate.
IMPLEMENT_REFCOUNTING_MANUAL(simple_window_delegate_t,
                             window_delegate,
                             ref_count)

// Release function for window delegate with custom cleanup.
int CEF_CALLBACK window_delegate_release(cef_base_ref_counted_t* self) {
  simple_window_delegate_t* delegate = (simple_window_delegate_t*)self;
  int count = atomic_fetch_sub(&delegate->ref_count, 1) - 1;
  if (count == 0) {
    if (delegate->ui_browser_view) {
      delegate->ui_browser_view->base.base.release(
          &delegate->ui_browser_view->base.base);
    }
    if (delegate->content_browser_view) {
      delegate->content_browser_view->base.base.release(
          &delegate->content_browser_view->base.base);
    }
    free(delegate);
    return 1;
  }
  return 0;
}

// Called when the window is created.
void CEF_CALLBACK window_delegate_on_window_created(cef_window_delegate_t* self,
                                                    cef_window_t* window) {
  simple_window_delegate_t* delegate = (simple_window_delegate_t*)self;

  LogMsg("window_delegate_on_window_created: start. ui_view=%p, content_view=%p\n",
         delegate->ui_browser_view, delegate->content_browser_view);

  // Add references for ourselves
  if (delegate->ui_browser_view) {
    delegate->ui_browser_view->base.base.add_ref(
        &delegate->ui_browser_view->base.base);
  }
  if (delegate->content_browser_view) {
    delegate->content_browser_view->base.base.add_ref(
        &delegate->content_browser_view->base.base);
  }

  if (delegate->ui_browser_view && delegate->content_browser_view) {
    cef_panel_t* panel = (cef_panel_t*)window;

    cef_box_layout_settings_t layout_settings = {};
    layout_settings.size = sizeof(cef_box_layout_settings_t);
    layout_settings.horizontal = 0;
    layout_settings.inside_border_horizontal_spacing = 0;
    layout_settings.inside_border_vertical_spacing = 0;
    layout_settings.between_child_spacing = 0;
    layout_settings.main_axis_alignment = CEF_AXIS_ALIGNMENT_START;
    layout_settings.cross_axis_alignment = CEF_AXIS_ALIGNMENT_STRETCH;
    layout_settings.default_flex = 0;

    cef_box_layout_t* layout = panel->set_to_box_layout(panel, &layout_settings);
    LogMsg("set_to_box_layout: layout=%p\n", layout);

    // Add UI view (transfers original reference)
    LogMsg("Adding ui_browser_view child view\n");
    panel->add_child_view(panel, &delegate->ui_browser_view->base);

    // Add Content view (transfers original reference)
    LogMsg("Adding content_browser_view child view\n");
    panel->add_child_view(panel, &delegate->content_browser_view->base);

    // Set flex for content browser view to 1 so it fills the space!
    layout->set_flex_for_view(layout, (cef_view_t*)delegate->content_browser_view, 1);

    // Force layout of the panel
    LogMsg("Forcing panel layout\n");
    panel->layout(panel);
  } else {
    // Single view, just add the content view
    LogMsg("No UI view. Adding single content view\n");
    if (delegate->content_browser_view) {
      window->base.add_child_view(&window->base, &delegate->content_browser_view->base);
    }
  }

  // Show the window now
  if (delegate->initial_show_state != CEF_SHOW_STATE_HIDDEN) {
    LogMsg("Showing window now\n");
    window->show(window);
  }

  // Release the window parameter before returning.
  window->base.base.base.release(&window->base.base.base);
  LogMsg("window_delegate_on_window_created: end\n");
}

// Called when the window is destroyed.
void CEF_CALLBACK
window_delegate_on_window_destroyed(cef_window_delegate_t* self,
                                    cef_window_t* window) {
  simple_window_delegate_t* delegate = (simple_window_delegate_t*)self;

  if (delegate->ui_browser_view) {
    delegate->ui_browser_view->base.base.release(
        &delegate->ui_browser_view->base.base);
    delegate->ui_browser_view = NULL;
  }
  if (delegate->content_browser_view) {
    delegate->content_browser_view->base.base.release(
        &delegate->content_browser_view->base.base);
    delegate->content_browser_view = NULL;
  }

  // Release the window parameter before returning.
  window->base.base.base.release(&window->base.base.base);
}

// Called to check if the window can be resized.
// C++ defaults this to true in the header. We don't get those defaults with
// the C API.
int CEF_CALLBACK window_delegate_can_resize(cef_window_delegate_t* self,
                                            cef_window_t* window) {
  // Release the window parameter before returning.
  window->base.base.base.release(&window->base.base.base);

  // Default: allow resize.
  return 1;
}

// Called to check if the window can be maximized.
// C++ defaults this to true in the header. We don't get those defaults with
// the C API.
int CEF_CALLBACK window_delegate_can_maximize(cef_window_delegate_t* self,
                                              cef_window_t* window) {
  // Release the window parameter before returning.
  window->base.base.base.release(&window->base.base.base);

  // Default: allow maximize.
  return 1;
}

// Called to check if the window can be minimized.
// C++ defaults this to true in the header. We don't get those defaults with
// the C API.
int CEF_CALLBACK window_delegate_can_minimize(cef_window_delegate_t* self,
                                              cef_window_t* window) {
  // Release the window parameter before returning.
  window->base.base.base.release(&window->base.base.base);

  // Default: allow minimize.
  return 1;
}

// Called to check if the window can close.
int CEF_CALLBACK window_delegate_can_close(cef_window_delegate_t* self,
                                           cef_window_t* window) {
  // (unused delegate removed)

  int can_close = 1;  // Default to allowing close.

  // Ask the content browser if it's OK to close.
  if (g_content_browser) {
    cef_browser_t* browser = g_content_browser;
    if (browser) {
      cef_browser_host_t* host = browser->get_host(browser);
      if (host) {
        can_close = host->try_close_browser(host);
        // Release host (get_host returns a new reference).
        host->base.release(&host->base);
      }
    }
  }

  // Release the window parameter before returning.
  window->base.base.base.release(&window->base.base.base);

  return can_close;
}

// Called to check if the window should have standard window buttons (macOS).
// C++ defaults this to true in the header. We don't get those defaults with
// the C API.
int CEF_CALLBACK
window_delegate_with_standard_window_buttons(cef_window_delegate_t* self,
                                             cef_window_t* window) {
  // Release the window parameter before returning.
  window->base.base.base.release(&window->base.base.base);

  // Default: show standard window buttons.
  return 1;
}

// Returns the preferred size for the view.
cef_size_t CEF_CALLBACK
window_delegate_get_preferred_size(cef_view_delegate_t* self,
                                   cef_view_t* view) {
  cef_size_t size = {800, 600};

  // Release the view parameter before returning.
  view->base.release(&view->base);

  return size;
}

// Returns the initial show state for the window.
cef_show_state_t CEF_CALLBACK
window_delegate_get_initial_show_state(cef_window_delegate_t* self,
                                       cef_window_t* window) {
  simple_window_delegate_t* delegate = (simple_window_delegate_t*)self;

  cef_show_state_t show_state = delegate->initial_show_state;

  // Release the window parameter before returning.
  window->base.base.base.release(&window->base.base.base);

  return show_state;
}

// Returns the runtime style for the window.
cef_runtime_style_t CEF_CALLBACK
window_delegate_get_window_runtime_style(cef_window_delegate_t* self) {
  simple_window_delegate_t* delegate = (simple_window_delegate_t*)self;
  return delegate->runtime_style;
}

void CEF_CALLBACK
window_delegate_on_window_bounds_changed(cef_window_delegate_t* self,
                                        cef_window_t* window,
                                        const cef_rect_t* new_bounds) {
  LogMsg("window_delegate_on_window_bounds_changed: new_bounds={%d, %d, %d, %d} (no-op, handled by WM_SIZE)\n",
         new_bounds->x, new_bounds->y, new_bounds->width, new_bounds->height);
  
  // Release the window parameter before returning.
  window->base.base.base.release(&window->base.base.base);
}

// Creates a window delegate instance.
simple_window_delegate_t* window_delegate_create(
    cef_browser_view_t* ui_browser_view,
    cef_browser_view_t* content_browser_view,
    cef_runtime_style_t runtime_style,
    cef_show_state_t initial_show_state) {
  simple_window_delegate_t* delegate =
      (simple_window_delegate_t*)calloc(1, sizeof(simple_window_delegate_t));
  CHECK(delegate);

  // Initialize base structure.
  INIT_CEF_BASE_REFCOUNTED(&delegate->delegate.base.base.base,
                           cef_window_delegate_t, window_delegate);

  // Set callbacks for cef_window_delegate_t.
  delegate->delegate.on_window_created = window_delegate_on_window_created;
  delegate->delegate.on_window_destroyed = window_delegate_on_window_destroyed;
  delegate->delegate.on_window_bounds_changed = window_delegate_on_window_bounds_changed;
  delegate->delegate.can_resize = window_delegate_can_resize;
  delegate->delegate.can_maximize = window_delegate_can_maximize;
  delegate->delegate.can_minimize = window_delegate_can_minimize;
  delegate->delegate.can_close = window_delegate_can_close;
  delegate->delegate.with_standard_window_buttons =
      window_delegate_with_standard_window_buttons;
  delegate->delegate.get_initial_show_state =
      window_delegate_get_initial_show_state;
  delegate->delegate.get_window_runtime_style =
      window_delegate_get_window_runtime_style;

  // Set callbacks for cef_view_delegate_t (parent structure).
  delegate->delegate.base.base.get_preferred_size =
      window_delegate_get_preferred_size;

  // Store the browser view references (we take ownership).
  delegate->ui_browser_view = ui_browser_view;
  delegate->content_browser_view = content_browser_view;

  // Store runtime style and initial show state.
  delegate->runtime_style = runtime_style;
  delegate->initial_show_state = initial_show_state;

  // Initialize with ref count of 1.
  atomic_store(&delegate->ref_count, 1);

  return delegate;
}
