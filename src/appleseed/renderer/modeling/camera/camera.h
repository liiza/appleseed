
//
// This source file is part of appleseed.
// Visit http://appleseedhq.net/ for additional information and resources.
//
// This software is released under the MIT license.
//
// Copyright (c) 2010-2011 Francois Beaune
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#ifndef APPLESEED_RENDERER_MODELING_CAMERA_CAMERA_H
#define APPLESEED_RENDERER_MODELING_CAMERA_CAMERA_H

// appleseed.renderer headers.
#include "renderer/global/global.h"
#include "renderer/kernel/shading/shadingray.h"
#include "renderer/modeling/entity/entity.h"

// appleseed.foundation headers.
#include "foundation/math/frustum.h"
#include "foundation/math/transform.h"
#include "foundation/utility/implptr.h"

// Forward declarations.
namespace foundation    { class DictionaryArray; }
namespace renderer      { class Project; }

namespace renderer
{

//
// Camera.
//

class RENDERERDLL Camera
  : public Entity
{
  public:
    // Constructor.
    Camera(
        const char*         name,
        const ParamArray&   params);

    // Return a string identifying the model of this entity.
    virtual const char* get_model() const = 0;

    // Set the camera transformation.
    void set_transform(const foundation::Transformd& transform);

    // Get the camera transformation.
    const foundation::Transformd& get_transform() const;

    // Get the film dimensions (in meters).
    const foundation::Vector2d& get_film_dimensions() const;

    // Get the focal length (in meters).
    double get_focal_length() const;

    // Get the view pyramid of the camera.
    const foundation::Pyramid3d& get_view_pyramid() const;

    // This method is called once before rendering each frame.
    virtual void on_frame_begin(const Project& project);

    // This method is called once after rendering each frame.
    virtual void on_frame_end(const Project& project);

    // Generate a ray originating from a given point on the lens surface of the camera
    // and directed toward a given point on the film plane, at a given time. point is
    // the target point on the film plane expressed in normalized device coordinates
    // (http://appleseedhq.net/conventions). time is the time at which the ray must be
    // generated. The generated ray is expressed in world space.
    virtual void generate_ray(
        SamplingContext&            sampling_context,
        const foundation::Vector2d& point,              // point on film plane, in NDC
        const float                 time,               // time at which to generate the ray
        ShadingRay&                 ray) const = 0;

    // Project a 3D point back to the film plane. The input point is expressed in camera
    // space. The returned point is expressed in normalized device coordinates.
    virtual foundation::Vector2d project(
        const foundation::Vector3d& point) const = 0;   // point in camera space

  protected:
    struct Impl;
    foundation::impl_ptr<Impl, false> impl;

    // Utility function to retrieve the film dimensions from the entity parameters.
    foundation::Vector2d extract_film_dimensions() const;

    // Utility function to retrieve the focal length from the entity parameters.
    double extract_focal_length(const double film_width) const;

    // Utility function to retrieve the f-stop value from the entity parameters.
    double extract_f_stop() const;

    // Utility function to retrieve the focal distance from the entity parameters.
    void extract_focal_distance(
        bool&                       autofocus_enabled,
        foundation::Vector2d&       autofocus_target,
        double&                     focal_distance) const;

  private:
    bool has_param(const char* name) const;
    bool has_params(const char* name1, const char* name2) const;

    double get_greater_than_zero(
        const char*                 name,
        const double                default_value) const;

    void compute_view_pyramid();
};


//
// An incomplete factory class whose main purpose is to factorize the code
// common to all camera models.
//

class RENDERERDLL CameraFactory
{
  public:
    // Return a set of widget definitions common to all camera models.
    static foundation::DictionaryArray get_widget_definitions();
};

}       // namespace renderer

#endif  // !APPLESEED_RENDERER_MODELING_CAMERA_CAMERA_H
