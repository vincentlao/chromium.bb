/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "core/layout/style/StyleInheritedData.h"

#include "core/layout/style/LayoutStyle.h"

namespace blink {

StyleInheritedData::StyleInheritedData()
    : horizontal_border_spacing(LayoutStyle::initialHorizontalBorderSpacing())
    , vertical_border_spacing(LayoutStyle::initialVerticalBorderSpacing())
    , line_height(LayoutStyle::initialLineHeight())
    , color(LayoutStyle::initialColor())
    , visitedLinkColor(LayoutStyle::initialColor())
    , textAutosizingMultiplier(1)
{
}

StyleInheritedData::~StyleInheritedData()
{
}

StyleInheritedData::StyleInheritedData(const StyleInheritedData& o)
    : RefCounted<StyleInheritedData>()
    , horizontal_border_spacing(o.horizontal_border_spacing)
    , vertical_border_spacing(o.vertical_border_spacing)
    , line_height(o.line_height)
    , font(o.font)
    , color(o.color)
    , visitedLinkColor(o.visitedLinkColor)
    , textAutosizingMultiplier(o.textAutosizingMultiplier)
{
}

bool StyleInheritedData::operator==(const StyleInheritedData& o) const
{
    return line_height == o.line_height
        && font == o.font
        && color == o.color
        && visitedLinkColor == o.visitedLinkColor
        && horizontal_border_spacing == o.horizontal_border_spacing
        && textAutosizingMultiplier == o.textAutosizingMultiplier
        && vertical_border_spacing == o.vertical_border_spacing;
}

} // namespace blink