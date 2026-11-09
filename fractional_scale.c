/* Wayland compositor running on top of an X server.

Copyright (C) 2022 to various contributors.

This file is part of 12to11.

12to11 is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation, either version 3 of the License, or (at your
option) any later version.

12to11 is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with 12to11.  If not, see <https://www.gnu.org/licenses/>.  */

#include <string.h>

#include "compositor.h"
#include "fractional-scale-v1.h"

#define FRACTIONAL_SCALE_120 132.0

typedef struct _FractionalScaleExt FractionalScaleExt;

struct _FractionalScaleExt
{
  /* The attached surface.  */
  Surface *surface;

  /* The attached destroy callback.  */
  DestroyCallback *destroy_callback;

  /* The associated wl_resource.  */
  struct wl_resource *resource;
};

/* The wp_fractional_scale_manager_v1 global.  */
static struct wl_global *fractional_scale_manager_global;

static void
HandleResourceDestroy (struct wl_resource *resource)
{
  FractionalScaleExt *ext;

  ext = wl_resource_get_user_data (resource);

  if (!ext)
    return;

  if (ext->surface)
    XLSurfaceCancelRunOnFree (ext->destroy_callback);

  XLFree (ext);
}

static void
HandleSurfaceDestroy (void *data)
{
  FractionalScaleExt *ext;

  ext = data;
  ext->surface = NULL;
  ext->destroy_callback = NULL;

  if (ext->resource)
    {
      wl_resource_set_user_data (ext->resource, NULL);
      wl_resource_destroy (ext->resource);
      ext->resource = NULL;
    }

  XLFree (ext);
}

static void
Destroy (struct wl_client *client, struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static const struct wp_fractional_scale_v1_interface
fractional_scale_impl =
  {
    .destroy = Destroy,
  };

static void
GetFractionalScale (struct wl_client *client, struct wl_resource *resource,
                    uint32_t id, struct wl_resource *surface_resource)
{
  Surface *surface;
  FractionalScaleExt *ext;
  struct wl_resource *res;

  surface = wl_resource_get_user_data (surface_resource);

  ext = XLCalloc (1, sizeof *ext);

  if (!ext)
    {
      wl_resource_post_no_memory (resource);
      return;
    }

  res = wl_resource_create (client, &wp_fractional_scale_v1_interface,
                            wl_resource_get_version (resource), id);

  if (!res)
    {
      XLFree (ext);
      wl_resource_post_no_memory (resource);
      return;
    }

  ext->surface = surface;
  ext->resource = res;
  ext->destroy_callback
    = XLSurfaceRunOnFree (surface, HandleSurfaceDestroy, ext);

  wl_resource_set_implementation (res, &fractional_scale_impl, ext,
                                  HandleResourceDestroy);

  /* Tell the client that the preferred scale is 1.1.  */
  surface->fractional_scale = FRACTIONAL_SCALE_120 / 120.0;

  /* Recompute the surface factor now, since Chromium never sends
     set_buffer_scale on the toplevel surface.  */
  XLSurfaceApplyScale (surface);

  wp_fractional_scale_v1_send_preferred_scale (res,
                                               FRACTIONAL_SCALE_120);
}

static const struct wp_fractional_scale_manager_v1_interface
fractional_scale_manager_impl =
  {
    .destroy = Destroy,
    .get_fractional_scale = GetFractionalScale,
  };

static void
HandleBind (struct wl_client *client, void *data, uint32_t version,
            uint32_t id)
{
  struct wl_resource *resource;

  resource
    = wl_resource_create (client,
                          &wp_fractional_scale_manager_v1_interface,
                          version, id);

  if (!resource)
    {
      wl_client_post_no_memory (client);
      return;
    }

  wl_resource_set_implementation (resource, &fractional_scale_manager_impl,
                                  NULL, NULL);
}

void
XLInitFractionalScale (void)
{
  fractional_scale_manager_global
    = wl_global_create (compositor.wl_display,
                        &wp_fractional_scale_manager_v1_interface,
                        1, NULL, HandleBind);
}
