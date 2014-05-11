
//
// This source file is part of appleseed.
// Visit http://appleseedhq.net/ for additional information and resources.
//
// This software is released under the MIT license.
//
// Copyright (c) 2010-2013 Francois Beaune, Jupiter Jazz Limited
// Copyright (c) 2014 Francois Beaune, The appleseedhq Organization
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

// Interface header.
#include "latlongmapenvironmentedf.h"

// appleseed.renderer headers.
#include "renderer/global/globallogger.h"
#include "renderer/global/globaltypes.h"
#include "renderer/kernel/lighting/imageimportancesampler.h"
#include "renderer/kernel/texturing/texturecache.h"
#include "renderer/kernel/texturing/texturestore.h"
#include "renderer/modeling/environmentedf/environmentedf.h"
#include "renderer/modeling/environmentedf/sphericalcoordinates.h"
#include "renderer/modeling/input/inputarray.h"
#include "renderer/modeling/input/inputevaluator.h"
#include "renderer/modeling/input/source.h"
#include "renderer/modeling/input/texturesource.h"
#include "renderer/modeling/project/project.h"
#include "renderer/modeling/scene/scene.h"
#include "renderer/modeling/scene/textureinstance.h"
#include "renderer/modeling/texture/texture.h"
#include "renderer/utility/paramarray.h"

// appleseed.foundation headers.
#include "foundation/image/canvasproperties.h"
#include "foundation/image/color.h"
#include "foundation/image/colorspace.h"
#include "foundation/math/sampling.h"
#include "foundation/math/scalar.h"
#include "foundation/math/vector.h"
#include "foundation/platform/compiler.h"
#include "foundation/platform/types.h"
#include "foundation/utility/containers/dictionary.h"
#include "foundation/utility/job/abortswitch.h"

// Standard headers.
#include <cassert>
#include <cmath>
#include <cstddef>
#include <memory>

using namespace foundation;
using namespace std;

namespace renderer
{

namespace
{
    //
    // Latitude-longitude environment map EDF.
    //
    // Reference:
    //
    //   http://www.cs.virginia.edu/~gfx/courses/2007/ImageSynthesis/assignments/envsample.pdf
    //
    // Light probes:
    //
    //   http://gl.ict.usc.edu/Data/HighResProbes/
    //   http://www.cs.kuleuven.be/~graphics/index.php/environment-maps
    //

    struct Payload
    {
        uint32      m_x;
        Color3f     m_color;
    };

    typedef ImageImportanceSampler<Payload, double> ImageImportanceSamplerType;

    class ImageSampler
    {
      public:
        ImageSampler(
            TextureCache&   texture_cache,
            const Source*   radiance_source,
            const Source*   multiplier_source,
            const size_t    width,
            const size_t    height,
            const double    u_shift,
            const double    v_shift)
          : m_texture_cache(texture_cache)
          , m_radiance_source(radiance_source)
          , m_multiplier_source(multiplier_source)
          , m_rcp_width(1.0 / width)
          , m_rcp_height(1.0 / height)
          , m_u_shift(u_shift)
          , m_v_shift(v_shift)
        {
        }

        void sample(const size_t x, const size_t y, Payload& payload, double& importance)
        {
            payload.m_x = static_cast<uint32>(x);

            if (m_radiance_source == 0)
            {
                payload.m_color.set(0.0f);
                importance = 0.0;
                return;
            }

            const Vector2d uv(
                (x + 0.5) * m_rcp_width + m_u_shift,
                1.0 - (y + 0.5) * m_rcp_height + m_v_shift);

            m_radiance_source->evaluate(m_texture_cache, uv, payload.m_color);

            double multiplier;
            m_multiplier_source->evaluate(m_texture_cache, uv, multiplier);
            payload.m_color *= static_cast<float>(multiplier);

            importance = static_cast<double>(luminance(payload.m_color));
        }

      private:
        TextureCache&   m_texture_cache;
        const Source*   m_radiance_source;
        const Source*   m_multiplier_source;
        const double    m_rcp_width;
        const double    m_rcp_height;
        const double    m_u_shift;
        const double    m_v_shift;
    };

    const char* Model = "latlong_map_environment_edf";

    class LatLongMapEnvironmentEDF
      : public EnvironmentEDF
    {
      public:
        LatLongMapEnvironmentEDF(
            const char*         name,
            const ParamArray&   params)
          : EnvironmentEDF(name, params)
          , m_importance_map_width(0)
          , m_importance_map_height(0)
          , m_probability_scale(0.0)
        {
            m_inputs.declare("radiance", InputFormatSpectralIlluminance);
            m_inputs.declare("radiance_multiplier", InputFormatScalar, "1.0");

            m_u_shift = m_params.get_optional<double>("horizontal_shift", 0.0) / 360.0;
            m_v_shift = m_params.get_optional<double>("vertical_shift", 0.0) / 360.0;
        }

        virtual void release() OVERRIDE
        {
            delete this;
        }

        virtual const char* get_model() const OVERRIDE
        {
            return Model;
        }

        virtual bool on_frame_begin(
            const Project&      project,
            AbortSwitch*        abort_switch) OVERRIDE
        {
            if (!EnvironmentEDF::on_frame_begin(project, abort_switch))
                return false;

            check_non_zero_radiance("radiance", "radiance_multiplier");

            if (m_importance_sampler.get() == 0)
                build_importance_map(*project.get_scene(), abort_switch);

            return true;
        }

        virtual void sample(
            InputEvaluator&     input_evaluator,
            const Vector2d&     s,
            Vector3d&           outgoing,
            Spectrum&           value,
            double&             probability) const OVERRIDE
        {
            // Sample the importance map.
            Payload payload;
            size_t y;
            double prob_xy;
            m_importance_sampler->sample(s, payload, y, prob_xy);

            // Compute the coordinates in [0,1]^2 of the sample.
            const double u = (payload.m_x + 0.5) * m_rcp_importance_map_width;
            const double v = (y + 0.5) * m_rcp_importance_map_height;

            // Compute the world space emission direction.
            double theta, phi;
            unit_square_to_angles(u, v, theta, phi);
            outgoing = Vector3d::unit_vector(theta, phi);

            // Compute the spectral illuminance.
            linear_rgb_illuminance_to_spectrum(payload.m_color, value);

            // Compute the probability density of this direction.
            probability = prob_xy * m_probability_scale / sin(theta);
        }

        virtual void evaluate(
            InputEvaluator&     input_evaluator,
            const Vector3d&     outgoing,
            Spectrum&           value) const OVERRIDE
        {
            assert(is_normalized(outgoing));

            double theta, phi;
            unit_vector_to_angles(outgoing, theta, phi);

            double u, v;
            angles_to_unit_square(theta, phi, u, v);

            lookup_environment_map(input_evaluator, u, v, value);
        }

        virtual void evaluate(
            InputEvaluator&     input_evaluator,
            const Vector3d&     outgoing,
            Spectrum&           value,
            double&             probability) const OVERRIDE
        {
            assert(is_normalized(outgoing));

            double theta, phi;
            unit_vector_to_angles(outgoing, theta, phi);

            double u, v;
            angles_to_unit_square(theta, phi, u, v);

            lookup_environment_map(input_evaluator, u, v, value);

            probability = compute_pdf(u, v, theta);
        }

        virtual double evaluate_pdf(
            InputEvaluator&     input_evaluator,
            const Vector3d&     outgoing) const OVERRIDE
        {
            assert(is_normalized(outgoing));

            double theta, phi;
            unit_vector_to_angles(outgoing, theta, phi);

            double u, v;
            angles_to_unit_square(theta, phi, u, v);

            return compute_pdf(u, v, theta);
        }

      private:
        DECLARE_INPUT_VALUES(InputValues)
        {
            Spectrum    m_radiance;             // emitted radiance in W.m^-2.sr^-1
            double      m_radiance_multiplier;  // emitted radiance multiplier
        };

        double  m_u_shift;
        double  m_v_shift;

        size_t  m_importance_map_width;
        size_t  m_importance_map_height;

        double  m_rcp_importance_map_width;
        double  m_rcp_importance_map_height;
        double  m_probability_scale;

        auto_ptr<ImageImportanceSamplerType>    m_importance_sampler;

        void build_importance_map(const Scene& scene, AbortSwitch* abort_switch)
        {
            const Source* radiance_source = m_inputs.source("radiance");
            assert(radiance_source);

            if (dynamic_cast<const TextureSource*>(radiance_source))
            {
                const TextureSource* texture_source = static_cast<const TextureSource*>(radiance_source);
                const TextureInstance& texture_instance = texture_source->get_texture_instance();
                const CanvasProperties& texture_props = texture_instance.get_texture().properties();

                m_importance_map_width = texture_props.m_canvas_width;
                m_importance_map_height = texture_props.m_canvas_height;
            }
            else
            {
                RENDERER_LOG_ERROR(
                    "while building importance map for environment edf \"%s\": a texture instance "
                    "must be bound to the \"radiance\" input.",
                    get_name());

                m_importance_map_width = 1;
                m_importance_map_height = 1;
            }

            m_rcp_importance_map_width = 1.0 / m_importance_map_width;
            m_rcp_importance_map_height = 1.0 / m_importance_map_height;

            const size_t texel_count = m_importance_map_width * m_importance_map_height;
            m_probability_scale = texel_count / (2.0 * Pi * Pi);

            TextureStore texture_store(scene);
            TextureCache texture_cache(texture_store);
            ImageSampler sampler(
                texture_cache,
                radiance_source,
                m_inputs.source("radiance_multiplier"),
                m_importance_map_width,
                m_importance_map_height,
                m_u_shift,
                m_v_shift);

            m_importance_sampler.reset(
                new ImageImportanceSamplerType(
                    m_importance_map_width,
                    m_importance_map_height));

            RENDERER_LOG_INFO(
                "building " FMT_SIZE_T "x" FMT_SIZE_T " importance map "
                "for environment edf \"%s\"...",
                m_importance_map_width,
                m_importance_map_height,
                get_name());

            m_importance_sampler->rebuild(sampler, abort_switch);

            if (is_aborted(abort_switch))
                m_importance_sampler.reset();
            else
            {
                RENDERER_LOG_INFO(
                    "built importance map for environment edf \"%s\".",
                    get_name());
            }
        }

        void lookup_environment_map(
            InputEvaluator&     input_evaluator,
            const double        u,
            const double        v,
            Spectrum&           value) const
        {
            const Vector2d uv(u + m_u_shift, 1.0 - v + m_v_shift);

            const InputValues* values =
                input_evaluator.evaluate<InputValues>(m_inputs, uv);

            value = values->m_radiance;
            value *= static_cast<float>(values->m_radiance_multiplier);
        }

        double compute_pdf(
            const double        u,
            const double        v,
            const double        theta) const
        {
            // Compute the probability density of this sample in the importance map.
            const size_t x = truncate<size_t>(m_importance_map_width * u);
            const size_t y = truncate<size_t>(m_importance_map_height * v);
            const double prob_xy = m_importance_sampler->get_pdf(x, y);

            // Compute the probability density of the emission direction.
            return prob_xy * m_probability_scale / sin(theta);
        }
    };
}


//
// LatLongMapEnvironmentEDFFactory class implementation.
//

const char* LatLongMapEnvironmentEDFFactory::get_model() const
{
    return Model;
}

const char* LatLongMapEnvironmentEDFFactory::get_human_readable_model() const
{
    return "Latitude-Longitude Map Environment EDF";
}

DictionaryArray LatLongMapEnvironmentEDFFactory::get_input_metadata() const
{
    DictionaryArray metadata;

    metadata.push_back(
        Dictionary()
            .insert("name", "radiance")
            .insert("label", "Radiance")
            .insert("type", "colormap")
            .insert("entity_types",
                Dictionary()
                    .insert("texture_instance", "Textures"))
            .insert("use", "required")
            .insert("default", "1.0"));

    metadata.push_back(
        Dictionary()
            .insert("name", "radiance_multiplier")
            .insert("label", "Radiance Multiplier")
            .insert("type", "colormap")
            .insert("entity_types",
                Dictionary().insert("texture_instance", "Textures"))
            .insert("use", "optional")
            .insert("default", "1.0"));

    metadata.push_back(
        Dictionary()
            .insert("name", "horizontal_shift")
            .insert("label", "Horizontal Shift")
            .insert("type", "numeric")
            .insert("min_value", "-360.0")
            .insert("max_value", "360.0")
            .insert("default", "0.0")
            .insert("use", "optional"));

    metadata.push_back(
        Dictionary()
            .insert("name", "vertical_shift")
            .insert("label", "Vertical Shift")
            .insert("type", "numeric")
            .insert("min_value", "-360.0")
            .insert("max_value", "360.0")
            .insert("default", "0.0")
            .insert("use", "optional"));

    return metadata;
}

auto_release_ptr<EnvironmentEDF> LatLongMapEnvironmentEDFFactory::create(
    const char*         name,
    const ParamArray&   params) const
{
    return
        auto_release_ptr<EnvironmentEDF>(
            new LatLongMapEnvironmentEDF(name, params));
}

}   // namespace renderer
