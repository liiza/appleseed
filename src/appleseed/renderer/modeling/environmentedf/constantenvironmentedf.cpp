
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
#include "constantenvironmentedf.h"

// appleseed.renderer headers.
#include "renderer/global/globaltypes.h"
#include "renderer/modeling/environmentedf/environmentedf.h"
#include "renderer/modeling/input/inputarray.h"
#include "renderer/modeling/input/source.h"

// appleseed.foundation headers.
#include "foundation/math/sampling.h"
#include "foundation/math/scalar.h"
#include "foundation/math/vector.h"
#include "foundation/platform/compiler.h"
#include "foundation/utility/containers/dictionary.h"

// Standard headers.
#include <cassert>

// Forward declarations.
namespace foundation    { class AbortSwitch; }
namespace renderer      { class InputEvaluator; }
namespace renderer      { class Project; }

using namespace foundation;
using namespace std;

namespace renderer
{

namespace
{
    //
    // Constant-emittance environment EDF.
    //

    const char* Model = "constant_environment_edf";

    class ConstantEnvironmentEDF
      : public EnvironmentEDF
    {
      public:
        ConstantEnvironmentEDF(
            const char*         name,
            const ParamArray&   params)
          : EnvironmentEDF(name, params)
        {
            m_inputs.declare("radiance", InputFormatSpectralIlluminance);
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

            if (!check_uniform("radiance"))
                return false;

            check_non_zero_radiance("radiance");

            m_inputs.evaluate_uniforms(&m_values);

            return true;
        }

        virtual void sample(
            InputEvaluator&     input_evaluator,
            const Vector2d&     s,
            Vector3d&           outgoing,
            Spectrum&           value,
            double&             probability) const OVERRIDE
        {
            outgoing = sample_sphere_uniform(s);
            value = m_values.m_radiance;
            probability = 1.0 / (4.0 * Pi);
        }

        virtual void evaluate(
            InputEvaluator&     input_evaluator,
            const Vector3d&     outgoing,
            Spectrum&           value) const OVERRIDE
        {
            assert(is_normalized(outgoing));
            value = m_values.m_radiance;
        }

        virtual void evaluate(
            InputEvaluator&     input_evaluator,
            const Vector3d&     outgoing,
            Spectrum&           value,
            double&             probability) const OVERRIDE
        {
            assert(is_normalized(outgoing));
            value = m_values.m_radiance;
            probability = 1.0 / (4.0 * Pi);
        }

        virtual double evaluate_pdf(
            InputEvaluator&     input_evaluator,
            const Vector3d&     outgoing) const OVERRIDE
        {
            assert(is_normalized(outgoing));
            return 1.0 / (4.0 * Pi);
        }

      private:
        DECLARE_INPUT_VALUES(InputValues)
        {
            Spectrum    m_radiance;             // emitted radiance in W.m^-2.sr^-1
        };

        InputValues     m_values;
    };
}


//
// ConstantEnvironmentEDFFactory class implementation.
//

const char* ConstantEnvironmentEDFFactory::get_model() const
{
    return Model;
}

const char* ConstantEnvironmentEDFFactory::get_human_readable_model() const
{
    return "Constant Environment EDF";
}

DictionaryArray ConstantEnvironmentEDFFactory::get_input_metadata() const
{
    DictionaryArray metadata;

    metadata.push_back(
        Dictionary()
            .insert("name", "radiance")
            .insert("label", "Radiance")
            .insert("type", "colormap")
            .insert("entity_types",
                Dictionary().insert("color", "Colors"))
            .insert("use", "required")
            .insert("default", "1.0"));

    return metadata;
}

auto_release_ptr<EnvironmentEDF> ConstantEnvironmentEDFFactory::create(
    const char*         name,
    const ParamArray&   params) const
{
    return
        auto_release_ptr<EnvironmentEDF>(
            new ConstantEnvironmentEDF(name, params));
}

}   // namespace renderer
