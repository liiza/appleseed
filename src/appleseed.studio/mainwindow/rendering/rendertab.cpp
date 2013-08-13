
//
// This source file is part of appleseed.
// Visit http://appleseedhq.net/ for additional information and resources.
//
// This software is released under the MIT license.
//
// Copyright (c) 2010-2013 Francois Beaune, Jupiter Jazz Limited
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
#include "rendertab.h"

// appleseed.studio headers.
#include "mainwindow/rendering/renderwidget.h"

// appleseed.renderer headers.
#include "renderer/api/frame.h"
#include "renderer/api/project.h"

// appleseed.foundation headers.
#include "foundation/image/canvasproperties.h"
#include "foundation/image/color.h"
#include "foundation/image/image.h"

// Qt headers.
#include <QComboBox>
#include <QGridLayout>
#include <QIcon>
#include <QLabel>
#include <QLayout>
#include <QScrollArea>
#include <QSize>
#include <QString>
#include <Qt>
#include <QToolBar>
#include <QToolButton>

using namespace foundation;
using namespace renderer;

namespace appleseed {
namespace studio {

//
// RenderTab class implementation.
//

RenderTab::RenderTab(
    ProjectExplorer&    project_explorer,
    Project&            project)
  : m_project_explorer(project_explorer)
  , m_project(project)
{
    create_render_widget();
    create_toolbar();
    create_scrollarea();

    setObjectName(QString::fromUtf8("render_widget_tab"));
    setLayout(new QGridLayout());
    layout()->setSpacing(0);
    layout()->setMargin(0);
    layout()->addWidget(m_toolbar);
    layout()->addWidget(m_scroll_area);

    recreate_handlers();
}

void RenderTab::clear()
{
    m_render_widget->clear(Color4f(0.0f));
}

void RenderTab::darken()
{
    m_render_widget->multiply(0.2f);
}

void RenderTab::update_size()
{
    m_set_render_region_button->setChecked(false);

    const CanvasProperties& props = m_project.get_frame()->image().properties();

    m_render_widget->resize(
        props.m_canvas_width,
        props.m_canvas_height);

    recreate_handlers();
}

RenderWidget* RenderTab::get_render_widget() const
{
    return m_render_widget;
}

void RenderTab::slot_render_widget_context_menu(const QPoint& point)
{
    emit signal_render_widget_context_menu(m_render_widget->mapToGlobal(point));
}

void RenderTab::slot_toggle_render_region(const bool checked)
{
    m_picking_handler->set_enabled(!checked);
    m_render_region_handler->set_enabled(checked);
}

void RenderTab::slot_set_render_region(const QRect& rect)
{
    m_set_render_region_button->setChecked(false);
    emit signal_set_render_region(rect);
}

void RenderTab::create_render_widget()
{
    const CanvasProperties& props = m_project.get_frame()->image().properties();

    m_render_widget =
        new RenderWidget(
            props.m_canvas_width,
            props.m_canvas_height);

    m_render_widget->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(
        m_render_widget, SIGNAL(customContextMenuRequested(const QPoint&)),
        SLOT(slot_render_widget_context_menu(const QPoint&)));
}

void RenderTab::create_toolbar()
{
    // Create the render toolbar.
    m_toolbar = new QToolBar();
    m_toolbar->setObjectName(QString::fromUtf8("render_toolbar"));
    m_toolbar->setIconSize(QSize(18, 18));

    // Create the label preceding the picking mode combobox.
    QLabel* picking_mode_label = new QLabel("Picking Mode:");
    picking_mode_label->setObjectName(QString::fromUtf8("picking_mode_label"));
    m_toolbar->addWidget(picking_mode_label);

    // Create the picking mode combobox.
    // The combo will be populated by the ScenePickingHandler instantiated below.
    m_picking_mode_combo = new QComboBox();
    m_picking_mode_combo->setObjectName(QString::fromUtf8("picking_mode_combo"));
    m_toolbar->addWidget(m_picking_mode_combo);

    m_toolbar->addSeparator();

    // Create the Set Render Region button in the render toolbar.
    m_set_render_region_button = new QToolButton();
    m_set_render_region_button->setIcon(QIcon(":/icons/selection-blue.png"));
    m_set_render_region_button->setToolTip("Set Render Region");
    m_set_render_region_button->setShortcut(Qt::Key_R);
    m_set_render_region_button->setCheckable(true);
    connect(
        m_set_render_region_button, SIGNAL(toggled(bool)),
        SLOT(slot_toggle_render_region(const bool)));
    m_toolbar->addWidget(m_set_render_region_button);

    // Create the Clear Render Region button in the render toolbar.
    m_clear_render_region_button = new QToolButton();
    m_clear_render_region_button->setIcon(QIcon(":/icons/selection-crossed.png"));
    m_clear_render_region_button->setToolTip("Clear Render Region");
    m_clear_render_region_button->setShortcut(Qt::Key_C);
    connect(
        m_clear_render_region_button, SIGNAL(clicked()),
        SIGNAL(signal_clear_render_region()));
    m_toolbar->addWidget(m_clear_render_region_button);

    m_toolbar->addSeparator();

    // Create a label to display various information such as mouse coordinates, etc.
    m_info_label = new QLabel();
    m_info_label->setObjectName(QString::fromUtf8("info_label"));
    m_toolbar->addWidget(m_info_label);
}

void RenderTab::create_scrollarea()
{
    // Encapsulate the render widget into another widget that adds a margin around it.
    QWidget* render_widget_wrapper = new QWidget();
    render_widget_wrapper->setObjectName(QString::fromUtf8("render_widget_wrapper"));
    render_widget_wrapper->setLayout(new QGridLayout());
    render_widget_wrapper->layout()->setSizeConstraint(QLayout::SetFixedSize);
    render_widget_wrapper->layout()->setContentsMargins(20, 20, 20, 20);
    render_widget_wrapper->layout()->addWidget(m_render_widget);

    // Wrap the render widget in a scroll area.
    m_scroll_area = new QScrollArea();
    m_scroll_area->setObjectName(QString::fromUtf8("render_widget_scrollarea"));
    m_scroll_area->setAlignment(Qt::AlignCenter);
    m_scroll_area->setWidget(render_widget_wrapper);
}

void RenderTab::recreate_handlers()
{
    // Handler to handle zooming the render widget in and out with the keyboard or the mouse wheel.
    m_zoom_handler.reset(
        new WidgetZoomHandler(
            m_scroll_area,
            m_render_widget));

    // Handler for camera panning with the mouse.
    m_pan_handler.reset(
        new ScrollAreaPanHandler(
            m_scroll_area));

    // Handler for camera tracking with the mouse.
    m_mouse_tracker.reset(
        new MouseCoordinatesTracker(
            m_render_widget,
            m_info_label));

    // Handler for picking scene entities in the render widget.
    m_picking_handler.reset(
        new ScenePickingHandler(
            m_render_widget,
            m_picking_mode_combo,
            *m_mouse_tracker.get(),
            m_project_explorer,
            m_project));

    // Handler for defining render regions with the mouse.
    m_render_region_handler.reset(
        new RenderRegionHandler(
            m_render_widget,
            *m_mouse_tracker.get()));
    connect(
        m_render_region_handler.get(), SIGNAL(signal_render_region(const QRect&)),
        SLOT(slot_set_render_region(const QRect&)));

    // Initially, the picking handler is active and the render region is inactive.
    m_picking_handler->set_enabled(true);
    m_render_region_handler->set_enabled(false);
}

}   // namespace studio
}   // namespace appleseed

#include "mainwindow/rendering/moc_cpp_rendertab.cxx"