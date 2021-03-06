/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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
 */

#ifndef SVGPathSource_h
#define SVGPathSource_h

#include "core/CoreExport.h"
#include "core/svg/SVGPathData.h"

namespace blink {

class CORE_EXPORT SVGPathSource {
    WTF_MAKE_NONCOPYABLE(SVGPathSource);
    STACK_ALLOCATED();
public:
    SVGPathSource() { }
    virtual ~SVGPathSource() { }

    virtual bool hasMoreData() const = 0;
    virtual SVGPathSegType peekSegmentType() = 0;
    virtual PathSegmentData parseSegment() = 0;
};

} // namespace blink

#endif // SVGPathSource_h
