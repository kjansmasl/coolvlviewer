/**
 * @file llraytrace.h
 * @brief Ray intersection tests for primitives.
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2001-2009, Linden Research, Inc.
 *
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 *
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 *
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 *
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#ifndef LL_RAYTRACE_H
#define LL_RAYTRACE_H

#include "llpreprocessor.h"
#include "stdtypes.h"

class LLVector3;
class LLQuaternion;

// Sides of a box...
//                  . Z      __.Y
//                 /|\        /|       0 = NO_SIDE
//                  |        /         1 = FRONT_SIDE   = +x
//           +------|-----------+      2 = BACK_SIDE    = -x
//          /|      |/     /   /|      3 = LEFT_SIDE    = +y
//         / |     -5-   |/   / |      4 = RIGHT_SIDE   = -y
//        /  |     /|   -3-  /  |      5 = TOP_SIDE     = +z
//       +------------------+   |      6 = BOTTOM_SIDE  = -z
//       |   |      |  /    |   |
//       | |/|      | /     | |/|
//       | 2 |    | *-------|-1--------> X
//       |/| |   -4-        |/| |
//       |   +----|---------|---+
//       |  /        /      |  /
//       | /       -6-      | /
//       |/        /        |/
//       +------------------+
constexpr U32 NO_SIDE 		= 0;
constexpr U32 FRONT_SIDE 	= 1;
constexpr U32 BACK_SIDE 	= 2;
constexpr U32 LEFT_SIDE 	= 3;
constexpr U32 RIGHT_SIDE 	= 4;
constexpr U32 TOP_SIDE 		= 5;
constexpr U32 BOTTOM_SIDE 	= 6;

// All functions produce results in the same reference frame as the arguments.
//
// Any arguments of the form "foo_direction" or "foo_normal" are assumed to
// be normalized, or normalized vectors are stored in them.
//
// Vector arguments of the form "shape_scale" represent the scale of the
// object along the three axes.
//
// All functions return the expected true or false, unless otherwise noted.
// When false is returned, any resulting values that might have been stored
// are undefined.
//
// Rays are defined by a "ray_point" and a "ray_direction" (unit).
//
// Lines are defined by a "line_point" and a "line_direction" (unit).
//
// Line segements are defined by "point_a" and "point_b", and for intersection
// purposes are assumed to point from "point_a" to "point_b".
//
// A ray is different from a line in that it starts at a point and extends
// in only one direction.
//
// Intersection normals always point outside the object, normal to the object's
// surface at the point of intersection.
//
// Object rotations passed as quaternions are expected to rotate from the
// object's local frame to the absolute frame.  So, if "foo" is a vector in
// the object's local frame, then "foo * object_rotation" is in the absolute
// frame.

// Returns true if line is not parallel to plane.
bool line_plane(const LLVector3& line_point, const LLVector3& line_direction,
				const LLVector3& plane_point, const LLVector3 plane_normal,
				LLVector3& intersection);


// Returns true if line is not parallel to plane.
bool ray_plane(const LLVector3& ray_point, const LLVector3& ray_direction,
			   const LLVector3& plane_point, const LLVector3 plane_normal,
			   LLVector3& intersection);


bool ray_circle(const LLVector3& ray_point, const LLVector3& ray_direction,
				const LLVector3& circle_center, const LLVector3 plane_normal,
				F32 circle_radius, LLVector3& intersection);

// point_0 through point_2 define the plane_normal via the right-hand rule:
// circle from point_0 to point_2 with fingers ==> thumb points in direction of
// normal
bool ray_triangle(const LLVector3& ray_point, const LLVector3& ray_direction,
				  const LLVector3& point_0, const LLVector3& point_1,
				  const LLVector3& point_2, LLVector3& intersection,
				  LLVector3& intersection_normal);


// point_0 is the lower-left corner, point_1 is the lower-right, point_2 is the
// upper-right right-hand-rule... curl fingers from lower-left toward lower-
// right then toward upper-right ==> thumb points in direction of normal
// Assumes a parallelogram, so point_3 is determined by the other points.
bool ray_quadrangle(const LLVector3& ray_point, const LLVector3& ray_direction,
					const LLVector3& point_0, const LLVector3& point_1,
					const LLVector3& point_2, LLVector3& intersection,
					LLVector3& intersection_normal);


bool ray_sphere(const LLVector3& ray_point, const LLVector3& ray_direction,
				const LLVector3& sphere_center, F32 sphere_radius,
				LLVector3& intersection, LLVector3& intersection_normal);


// Finite right cylinder is defined by end centers: "cyl_top", "cyl_bottom",
// and by the cylinder radius "cyl_radius"
bool ray_cylinder(const LLVector3& ray_point, const LLVector3& ray_direction,
		          const LLVector3& cyl_center, const LLVector3& cyl_scale,
				  const LLQuaternion& cyl_rotation,
				  LLVector3& intersection, LLVector3& intersection_normal);


// This function doesn't just return a bool because the return is currently
// used to decide how to break up boxes that have been hit by shots...
// a hack that will probably be changed later
//
// returns a number representing the side of the box that was hit by the ray,
// or NO_SIDE if intersection test failed.
U32 ray_box(const LLVector3& ray_point, const LLVector3& ray_direction,
		    const LLVector3& box_center, const LLVector3& box_scale,
			const LLQuaternion& box_rotation,
			LLVector3& intersection, LLVector3& intersection_normal);

bool ray_prism(const LLVector3& ray_point, const LLVector3& ray_direction,
			   const LLVector3& prism_center, const LLVector3& prism_scale,
			   const LLQuaternion& prism_rotation,
			   LLVector3& intersection, LLVector3& intersection_normal);

bool ray_tetrahedron(const LLVector3& ray_point,
					 const LLVector3& ray_direction,
					 const LLVector3& t_center, const LLVector3& t_scale,
					 const LLQuaternion& t_rotation,
					 LLVector3& intersection, LLVector3& intersection_normal);

bool ray_pyramid(const LLVector3& ray_point, const LLVector3& ray_direction,
				 const LLVector3& p_center, const LLVector3& p_scale,
				 const LLQuaternion& p_rotation,
				 LLVector3& intersection, LLVector3& intersection_normal);

bool linesegment_circle(const LLVector3& point_a, const LLVector3& point_b,
						const LLVector3& circle_center,
						const LLVector3 plane_normal, F32 circle_radius,
						LLVector3& intersection);

// point_0 through point_2 define the plane_normal via the right-hand rule:
// circle from point_0 to point_2 with fingers ==> thumb points in direction of
// normal.
bool linesegment_triangle(const LLVector3& point_a, const LLVector3& point_b,
						  const LLVector3& point_0, const LLVector3& point_1,
						  const LLVector3& point_2,
						  LLVector3& intersection,
						  LLVector3& intersection_normal);

// point_0 is the lower-left corner, point_1 is the lower-right, point_2 is the
// upper-right right-hand-rule... curl fingers from lower-left toward lower-
// right then toward upper-right ==> thumb points in direction of normal
// Assumes a parallelogram, so point_3 is determined by the other points.
bool linesegment_quadrangle(const LLVector3& point_a, const LLVector3& point_b,
							const LLVector3& point_0, const LLVector3& point_1,
							const LLVector3& point_2,
							LLVector3& intersection,
							LLVector3& intersection_normal);

bool linesegment_sphere(const LLVector3& point_a, const LLVector3& point_b,
				const LLVector3& sphere_center, F32 sphere_radius,
				LLVector3& intersection, LLVector3& intersection_normal);

// Finite right cylinder is defined by end centers: "cyl_top", "cyl_bottom",
// and by the cylinder radius "cyl_radius"
bool linesegment_cylinder(const LLVector3& point_a, const LLVector3& point_b,
						  const LLVector3& cyl_center,
						  const LLVector3& cyl_scale,
						  const LLQuaternion& cyl_rotation,
						  LLVector3& intersection,
						  LLVector3& intersection_normal);

// This function doesn't just return a bool because the return is currently
// used to decide how to break up boxes that have been hit by shots...
// A hack that will probably be changed later.
//
// Returns a number representing the side of the box that was hit by the ray,
// or NO_SIDE if intersection test failed.
U32 linesegment_box(const LLVector3& point_a, const LLVector3& point_b,
					const LLVector3& box_center, const LLVector3& box_scale,
					const LLQuaternion& box_rotation,
					LLVector3& intersection, LLVector3& intersection_normal);

bool linesegment_prism(const LLVector3& point_a, const LLVector3& point_b,
					   const LLVector3& prism_center,
					   const LLVector3& prism_scale,
					   const LLQuaternion& prism_rotation,
					   LLVector3& intersection, LLVector3& intersection_normal);

bool linesegment_tetrahedron(const LLVector3& point_a,
							 const LLVector3& point_b,
							 const LLVector3& t_center,
							 const LLVector3& t_scale,
							 const LLQuaternion& t_rotation,
							 LLVector3& intersection,
							 LLVector3& intersection_normal);

bool linesegment_pyramid(const LLVector3& point_a, const LLVector3& point_b,
						 const LLVector3& p_center, const LLVector3& p_scale,
						 const LLQuaternion& p_rotation,
						 LLVector3& intersection,
						 LLVector3& intersection_normal);

#endif	// LL_RAYTRACE_H
