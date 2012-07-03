
//
// This source file is part of appleseed.
// Visit http://appleseedhq.net/ for additional information and resources.
//
// This software is released under the MIT license.
//
// Copyright (c) 2010-2012 Francois Beaune, Jupiter Jazz Limited
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

#ifndef APPLESEED_STUDIO_MAINWINDOW_PROJECT_TEXTUREITEM_H
#define APPLESEED_STUDIO_MAINWINDOW_PROJECT_TEXTUREITEM_H

// appleseed.studio headers.
#include "mainwindow/project/multimodelentityitem.h"
#include "mainwindow/project/projectbuilder.h"

// appleseed.renderer headers.
#include "renderer/api/texture.h"

// appleseed.foundation headers.
#include "foundation/platform/compiler.h"

namespace appleseed {
namespace studio {

template <typename ParentEntity>
class TextureItem
  : public MultiModelEntityItem<renderer::Texture, ParentEntity>
{
  public:
    TextureItem(
        renderer::Texture*  texture,
        ParentEntity&       parent,
        ProjectBuilder&     project_builder);

  private:
    virtual void slot_delete() override;
};


//
// Implementation.
//

template <typename ParentEntity>
TextureItem<ParentEntity>::TextureItem(
    renderer::Texture*      texture,
    ParentEntity&           parent,
    ProjectBuilder&         project_builder)
  : MultiModelEntityItem<renderer::Texture, ParentEntity>(texture, parent, project_builder)
{
}

template <typename ParentEntity>
void TextureItem<ParentEntity>::slot_delete()
{
    typedef MultiModelEntityItem<renderer::Texture, ParentEntity> Base;
    Base::m_project_builder.remove_texture(Base::m_parent, Base::m_entity->get_uid());
}

}       // namespace studio
}       // namespace appleseed

#endif  // !APPLESEED_STUDIO_MAINWINDOW_PROJECT_TEXTUREITEM_H