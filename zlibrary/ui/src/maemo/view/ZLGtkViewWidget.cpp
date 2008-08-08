/*
 * Copyright (C) 2004-2008 Geometer Plus <contact@geometerplus.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <ZLOptions.h>

#include "../dialogs/ZLGtkOptionsDialog.h"
#include "ZLGtkViewWidget.h"
#include "ZLGtkPaintContext.h"
#include "../../gtk/pixbuf/ZLGtkPixbufHack.h"
#include "../../gtk/util/ZLGtkSignalUtil.h"

void ZLGtkViewWidget::updateCoordinates(int &x, int &y) {
	switch (rotation()) {
		default:
			break;
		case ZLView::DEGREES90:
		{
			int tmp = x;
			x = height() - y;
			y = tmp;
			break;
		}
		case ZLView::DEGREES180:
			x = width() - x;
			y = height() - y;
			break;
		case ZLView::DEGREES270:
		{
			int tmp = x;
			x = y;
			y = width() - tmp;
			break;
		}
	}
}

bool ZLGtkViewWidget::isStylusEvent(GtkWidget *widget, GdkEventButton *event) {
#if MAEMO_VERSION == 2
	if (event->button == 8) {
		return false;
	}
	if (event->state & GDK_MOD4_MASK) {
		return false;
	}
	if (gtk_widget_get_extension_events(widget) == GDK_EXTENSION_EVENTS_NONE) {
		return true;
	}
	double pressure = 0.0;
	gdk_event_get_axis((GdkEvent*)event, GDK_AXIS_PRESSURE, &pressure);
	int intPressure = (int)(100 * pressure);
	return
		(MinPressureOption.value() <= intPressure) &&
		(intPressure <= MaxPressureOption.value());
#elif MAEMO_VERSION == 4
	gdouble pressure;
	if (gdk_event_get_axis((GdkEvent*)event, GDK_AXIS_PRESSURE, &pressure)) {
		int intPressure = (int)(100 * pressure);
		return
			(MinPressureOption.value() <= intPressure) &&
			(intPressure <= MaxPressureOption.value());
	}
	
	if (event->button == 8)
		return false;

	if (event->button == 1 && event->state & GDK_MOD4_MASK)
		return false;

	if (event->button == 2)
		return false;

	return true;
#endif
}

void ZLGtkViewWidget::onMousePressed(GdkEventButton *event) {
	int x = (int)event->x;
	int y = (int)event->y;
	updateCoordinates(x, y);
	if (isStylusEvent(myArea, event)) {
		view()->onStylusMove(x, y);
		view()->onStylusPress(x, y);
		gtk_widget_set_extension_events(myArea, GDK_EXTENSION_EVENTS_NONE);
	} else {
		view()->onFingerTap(x, y);
	}
}

void ZLGtkViewWidget::onMouseReleased(GdkEventButton *event) {
	if (isStylusEvent(myArea, event)) {
		int x = (int)event->x;
		int y = (int)event->y;
		updateCoordinates(x, y);
		view()->onStylusRelease(x, y);
	}
	gtk_widget_set_extension_events(myArea, GDK_EXTENSION_EVENTS_CURSOR);
}

void ZLGtkViewWidget::onMouseMoved(GdkEventMotion *event) {
	int x, y;
	GdkModifierType state;
	if (event->is_hint) {
		gdk_window_get_pointer(event->window, &x, &y, &state);
	} else {
		x = (int)event->x;
		y = (int)event->y;
		state = (GdkModifierType)event->state;
	}
	updateCoordinates(x, y);
	view()->onStylusMovePressed(x, y);
}

int ZLGtkViewWidget::width() const {
	return (myArea != 0) ? myArea->allocation.width : 0;
}

int ZLGtkViewWidget::height() const {
	return (myArea != 0) ? myArea->allocation.height : 0;
}

static const std::string GROUP = "StylusPressure";

static void vScrollbarMoved(GtkAdjustment *adjustment, ZLGtkViewWidget *data) {
	data->onScrollbarMoved(
		ZLView::VERTICAL,
		(size_t)adjustment->upper,
		(size_t)adjustment->value,
		(size_t)(adjustment->value + adjustment->page_size)
	);
}

static void hScrollbarMoved(GtkAdjustment *adjustment, ZLGtkViewWidget *data) {
	data->onScrollbarMoved(
		ZLView::HORIZONTAL,
		(size_t)adjustment->upper,
		(size_t)adjustment->value,
		(size_t)(adjustment->value + adjustment->page_size)
	);
}

ZLGtkViewWidget::ZLGtkViewWidget(ZLApplication *application, ZLView::Angle initialAngle) :
	ZLViewWidget(initialAngle),
	MinPressureOption(ZLCategoryKey::CONFIG, GROUP, "Minimum", 0, 100, 0),
	MaxPressureOption(ZLCategoryKey::CONFIG, GROUP, "Maximum", 0, 100, 40),
	myVerticalScrollbarPlacementIsStandard(true),
	myHorizontalScrollbarPlacementIsStandard(true) {

	myApplication = application;
	myArea = gtk_drawing_area_new();
	myOriginalPixbuf = 0;
	myRotatedPixbuf = 0;
	GTK_OBJECT_SET_FLAGS(myArea, GTK_CAN_FOCUS);

	myScrollArea = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(0, 0));
	GtkAdjustment *hAdjustment = gtk_scrolled_window_get_hadjustment(myScrollArea);
	ZLGtkSignalUtil::connectSignal(GTK_OBJECT(hAdjustment), "value_changed", GTK_SIGNAL_FUNC(hScrollbarMoved), this);
	GtkAdjustment *vAdjustment = gtk_scrolled_window_get_vadjustment(myScrollArea);
	ZLGtkSignalUtil::connectSignal(GTK_OBJECT(vAdjustment), "value_changed", GTK_SIGNAL_FUNC(vScrollbarMoved), this);
	gtk_container_add(GTK_CONTAINER(myScrollArea), myArea);
	//gtk_scrolled_window_add_with_viewport(myScrollArea, myArea);
	gtk_scrolled_window_set_policy(myScrollArea, GTK_POLICY_NEVER, GTK_POLICY_NEVER);

	gtk_widget_set_double_buffered(myArea, false);
	gtk_widget_set_events(myArea, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK);
	gtk_widget_set_extension_events(myArea, GDK_EXTENSION_EVENTS_CURSOR);
}

void ZLGtkViewWidget::setScrollbarEnabled(ZLView::Direction direction, bool enabled) {
	GtkPolicyType hPolicy;
	GtkPolicyType vPolicy;
	gtk_scrolled_window_get_policy(myScrollArea, &hPolicy, &vPolicy);
	if (direction == ZLView::VERTICAL) {
		vPolicy = enabled ? GTK_POLICY_ALWAYS : GTK_POLICY_NEVER;
	} else {
		hPolicy = enabled ? GTK_POLICY_ALWAYS : GTK_POLICY_NEVER;
	}
	gtk_scrolled_window_set_policy(myScrollArea, hPolicy, vPolicy);
	if (direction == ZLView::VERTICAL) {
		gtk_widget_queue_draw(gtk_scrolled_window_get_vscrollbar(myScrollArea));
	} else {
		gtk_widget_queue_draw(gtk_scrolled_window_get_hscrollbar(myScrollArea));
	}
}

void ZLGtkViewWidget::setScrollbarPlacement(ZLView::Direction direction, bool standard) {
	if (direction == ZLView::VERTICAL) {
		myVerticalScrollbarPlacementIsStandard = standard;
	} else {
		myHorizontalScrollbarPlacementIsStandard = standard;
	}

	GtkCornerType cornerType = GTK_CORNER_TOP_LEFT;
	if (!myVerticalScrollbarPlacementIsStandard) {
  	cornerType = myHorizontalScrollbarPlacementIsStandard ?
			GTK_CORNER_TOP_RIGHT :
  		GTK_CORNER_BOTTOM_RIGHT;
	} else if (!myHorizontalScrollbarPlacementIsStandard) {
  	cornerType = GTK_CORNER_BOTTOM_LEFT;
	}
	gtk_scrolled_window_set_placement(myScrollArea, cornerType);
}

void ZLGtkViewWidget::setScrollbarParameters(ZLView::Direction direction, size_t full, size_t from, size_t to, size_t step) {
	GtkAdjustment *adjustment =
		(direction == ZLView::VERTICAL) ?
			gtk_scrolled_window_get_vadjustment(myScrollArea) :
			gtk_scrolled_window_get_hadjustment(myScrollArea);
	adjustment->lower = 0;
	adjustment->upper = full;
	adjustment->value = from;
	adjustment->step_increment = step;
	adjustment->page_increment = step;
	adjustment->page_size = to - from;
	if (direction == ZLView::VERTICAL) {
		gtk_scrolled_window_set_vadjustment(myScrollArea, adjustment);
		gtk_widget_queue_draw(gtk_scrolled_window_get_vscrollbar(myScrollArea));
	} else {
		gtk_scrolled_window_set_hadjustment(myScrollArea, adjustment);
		gtk_widget_queue_draw(gtk_scrolled_window_get_hscrollbar(myScrollArea));
	}
}

ZLGtkViewWidget::~ZLGtkViewWidget() {
	cleanOriginalPixbuf();
	cleanRotatedPixbuf();
}

void ZLGtkViewWidget::cleanOriginalPixbuf() {
	if (myOriginalPixbuf != 0) {
		gdk_pixbuf_unref(myOriginalPixbuf);
		gdk_image_unref(myImage);
		myOriginalPixbuf = 0;
	}
}

void ZLGtkViewWidget::cleanRotatedPixbuf() {
	if (myRotatedPixbuf != 0) {
		gdk_pixbuf_unref(myRotatedPixbuf);
		myRotatedPixbuf = 0;
	}
}

void ZLGtkViewWidget::trackStylus(bool track) {
	// TODO: implement
}

void ZLGtkViewWidget::repaint()	{
	gtk_widget_queue_draw(myArea);
}

void ZLGtkViewWidget::doPaint()	{
	ZLGtkPaintContext &gtkContext = (ZLGtkPaintContext&)view()->context();
	ZLView::Angle angle = rotation();
	bool isRotated = (angle == ZLView::DEGREES90) || (angle == ZLView::DEGREES270);
	int w = isRotated ? myArea->allocation.height : myArea->allocation.width;
	int h = isRotated ? myArea->allocation.width : myArea->allocation.height;
	gtkContext.updatePixmap(myArea, w, h);
	view()->paint();
	switch (angle) {
		default:
			cleanOriginalPixbuf();
			cleanRotatedPixbuf();
			gdk_draw_pixmap(myArea->window, myArea->style->white_gc, gtkContext.pixmap(), 0, 0, 0, 0, myArea->allocation.width, myArea->allocation.height);
			break;
		case ZLView::DEGREES180:
			cleanRotatedPixbuf();
			if ((myOriginalPixbuf != 0) &&
					((gdk_pixbuf_get_width(myOriginalPixbuf) != w) ||
					 (gdk_pixbuf_get_height(myOriginalPixbuf) != h))) {
				cleanOriginalPixbuf();
			}
			if (myOriginalPixbuf == 0) {
				myOriginalPixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, false, 8, w, h);
				myImage = gdk_image_new(GDK_IMAGE_FASTEST, gdk_drawable_get_visual(gtkContext.pixmap()), w, h);
			}
			gdk_drawable_copy_to_image(gtkContext.pixmap(), myImage, 0, 0, 0, 0, w, h);
			gdk_pixbuf_get_from_image(myOriginalPixbuf, myImage, gdk_drawable_get_colormap(gtkContext.pixmap()), 0, 0, 0, 0, w, h);
			::rotate180(myOriginalPixbuf);
			gdk_draw_pixbuf(myArea->window, myArea->style->white_gc, myOriginalPixbuf, 0, 0, 0, 0, w, h, GDK_RGB_DITHER_NONE, 0, 0);
			break;
		case ZLView::DEGREES90:
		case ZLView::DEGREES270:
			if ((myOriginalPixbuf != 0) &&
					((gdk_pixbuf_get_width(myOriginalPixbuf) != w) ||
					 (gdk_pixbuf_get_height(myOriginalPixbuf) != h))) {
				cleanOriginalPixbuf();
			}
			if ((myRotatedPixbuf != 0) &&
					((gdk_pixbuf_get_width(myRotatedPixbuf) != h) ||
					 (gdk_pixbuf_get_height(myRotatedPixbuf) != w))) {
				cleanRotatedPixbuf();
			}
			if (myOriginalPixbuf == 0) {
				myOriginalPixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, false, 8, w, h);
				myImage = gdk_image_new(GDK_IMAGE_FASTEST, gdk_drawable_get_visual(gtkContext.pixmap()), w, h);
			}
			if (myRotatedPixbuf == 0) {
				myRotatedPixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, false, 8, h, w);
			}
			gdk_drawable_copy_to_image(gtkContext.pixmap(), myImage, 0, 0, 0, 0, w, h);
			gdk_pixbuf_get_from_image(myOriginalPixbuf, myImage, gdk_drawable_get_colormap(gtkContext.pixmap()), 0, 0, 0, 0, w, h);
			::rotate90(myRotatedPixbuf, myOriginalPixbuf, angle == ZLView::DEGREES90);
			gdk_draw_pixbuf(myArea->window, myArea->style->white_gc, myRotatedPixbuf, 0, 0, 0, 0, h, w, GDK_RGB_DITHER_NONE, 0, 0);
			break;
	}
}

GtkWidget *ZLGtkViewWidget::area() {
	return myArea;
}

GtkWidget *ZLGtkViewWidget::areaWithScrollbars() {
	return GTK_WIDGET(myScrollArea);
}
