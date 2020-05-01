/**
\file
*/ 

#ifndef __CLIENTMATH_CAPI_H
#define __CLIENTMATH_CAPI_H
#include "HSLClient_export.h"
#include <stdbool.h>
//cut_before

/** 
\brief Math data structures and functions used by the Client API  
\defgroup Math_CAPI Client Geometry
\addtogroup Math_CAPI 
@{ 
*/

//-- constants -----

/// A 2D vector with float components.
typedef struct
{
    float x, y;
} HSLVector2f;

/// A 3D vector with float components.
typedef struct
{
    float x, y, z;
} HSLVector3f;

/// A 3D vector with int components.
typedef struct
{
    int x, y, z;
} HSLVector3i;

/** A 3x3 matrix with float components
	storage is row major order: [x0,x1,x2,y0,y1,y1,z0,z1,z2]
 */
typedef struct
{
    float m[3][3]; 
} HSLMatrix3f;

/// A quaternion rotation.
typedef struct
{
    float w, x, y, z;
} HSLQuatf;

// Interface
//----------

// HSLVector2f Methods

/// Adds two 2D vectors together
HSL_PUBLIC_FUNCTION(HSLVector2f) HSL_Vector2fAdd(const HSLVector2f *a, const HSLVector2f *b);
/// Subtracts one 2D vector from another 2D vector
HSL_PUBLIC_FUNCTION(HSLVector2f) HSL_Vector2fSubtract(const HSLVector2f *a, const HSLVector2f *b);
/// Scales a 2D vector by a scalar
HSL_PUBLIC_FUNCTION(HSLVector2f) HSL_Vector2fScale(const HSLVector2f *v, const float s);
/// Scales a 2D vector by a scalar and then adds a vector to the scaled result
HSL_PUBLIC_FUNCTION(HSLVector2f) HSL_Vector2fScaleAndAdd(const HSLVector2f *v, const float s, const HSLVector2f *b);
/// Divides each component of a 2D vector by a scalar without checking for divide-by-zero
HSL_PUBLIC_FUNCTION(HSLVector2f) HSL_Vector2fUnsafeScalarDivide(const HSLVector2f *numerator, const float divisor);
/// Divides each component of a 2D vector by the corresponding componenet of another vector without checking for a divide-by-zero
HSL_PUBLIC_FUNCTION(HSLVector2f) HSL_Vector2fUnsafeVectorDivide(const HSLVector2f *numerator, const HSLVector2f *divisor);
/// Divides each component of a 2D vector by a scalar, returning a default vector in the case of divide by zero
HSL_PUBLIC_FUNCTION(HSLVector2f) HSL_Vector2fSafeScalarDivide(const HSLVector2f *numerator, const float divisor, const HSLVector2f *default_result);
/// Divides each component of a 2D vector by another vector, returning a default value for each component in the case of divide by zero
HSL_PUBLIC_FUNCTION(HSLVector2f) HSL_Vector2fSafeVectorDivide(const HSLVector2f *numerator, const HSLVector2f *divisor, const HSLVector2f *default_result);
/// Computes the absolute value of each component
HSL_PUBLIC_FUNCTION(HSLVector2f) HSL_Vector2fAbs(const HSLVector2f *v);
/// Squares each component of a vector
HSL_PUBLIC_FUNCTION(HSLVector2f) HSL_Vector2fSquare(const HSLVector2f *v);
/// Computes the length of a given vector
HSL_PUBLIC_FUNCTION(float) HSL_Vector2fLength(const HSLVector2f *v);
/// Computes the normalized version of a vector, returning a default vector in the event of a near zero length vector
HSL_PUBLIC_FUNCTION(HSLVector2f) HSL_Vector2fNormalizeWithDefault(const HSLVector2f *v, const HSLVector2f *default_result);
/// Computes the minimum component of a given vector
HSL_PUBLIC_FUNCTION(float) HSL_Vector2fMinValue(const HSLVector2f *v);
/// Computes the maximum component of a given vector
HSL_PUBLIC_FUNCTION(float) HSL_Vector2fMaxValue(const HSLVector2f *v);
/// Computes the 2D dot product of two vectors
HSL_PUBLIC_FUNCTION(float) HSL_Vector2fDot(const HSLVector2f *a, const HSLVector2f *b);
/// Computes the min value of two vectors along each component
HSL_PUBLIC_FUNCTION(HSLVector2f) HSL_Vector2fMin(const HSLVector2f *a, const HSLVector2f *b);
/// Computes the max value of two vectors along each component
HSL_PUBLIC_FUNCTION(HSLVector2f) HSL_Vector2fMax(const HSLVector2f *a, const HSLVector2f *b);

// HSLVector3f Methods

/// Adds two 3D vectors together
HSL_PUBLIC_FUNCTION(HSLVector3f) HSL_Vector3fAdd(const HSLVector3f *a, const HSLVector3f *b);
/// Subtracts one 3D vector from another 3D vector
HSL_PUBLIC_FUNCTION(HSLVector3f) HSL_Vector3fSubtract(const HSLVector3f *a, const HSLVector3f *b);
/// Scales a 3D vector by a scalar
HSL_PUBLIC_FUNCTION(HSLVector3f) HSL_Vector3fScale(const HSLVector3f *v, const float s);
/// Scales a 3D vector by a scalar and then adds a vector to the scaled result
HSL_PUBLIC_FUNCTION(HSLVector3f) HSL_Vector3fScaleAndAdd(const HSLVector3f *v, const float s, const HSLVector3f *b);
/// Divides each component of a 3D vector by a scalar without checking for divide-by-zero
HSL_PUBLIC_FUNCTION(HSLVector3f) HSL_Vector3fUnsafeScalarDivide(const HSLVector3f *numerator, const float divisor);
/// Divides each component of a 3D vector by the corresponding componenet of another vector without checking for a divide-by-zero
HSL_PUBLIC_FUNCTION(HSLVector3f) HSL_Vector3fUnsafeVectorDivide(const HSLVector3f *numerator, const HSLVector3f *divisor);
/// Divides each component of a 3D vector by a scalar, returning a default vector in the case of divide by zero
HSL_PUBLIC_FUNCTION(HSLVector3f) HSL_Vector3fSafeScalarDivide(const HSLVector3f *numerator, const float divisor, const HSLVector3f *default_result);
/// Divides each component of a 2D vector by another vector, returning a default value for each component in the case of divide by zero
HSL_PUBLIC_FUNCTION(HSLVector3f) HSL_Vector3fSafeVectorDivide(const HSLVector3f *numerator, const HSLVector3f *divisor, const HSLVector3f *default_result);
/// Computes the absolute value of each component
HSL_PUBLIC_FUNCTION(HSLVector3f) HSL_Vector3fAbs(const HSLVector3f *v);
/// Squares each component of a vector
HSL_PUBLIC_FUNCTION(HSLVector3f) HSL_Vector3fSquare(const HSLVector3f *v);
/// Computes the length of a given vector
HSL_PUBLIC_FUNCTION(float) HSL_Vector3fLength(const HSLVector3f *v);
/// Computes the normalized version of a vector, returning a default vector in the event of a near zero length vector
HSL_PUBLIC_FUNCTION(HSLVector3f) HSL_Vector3fNormalizeWithDefault(const HSLVector3f *v, const HSLVector3f *default_result);
/// Computes the normalized version of a vector and its original length, returning a default vector in the event of a near zero length vector
HSL_PUBLIC_FUNCTION(HSLVector3f) HSL_Vector3fNormalizeWithDefaultGetLength(const HSLVector3f *v, const HSLVector3f *default_result, float *out_length);
/// Computes the minimum component of a given vector
HSL_PUBLIC_FUNCTION(float) HSL_Vector3fMinValue(const HSLVector3f *v);
/// Computes the maximum component of a given vector
HSL_PUBLIC_FUNCTION(float) HSL_Vector3fMaxValue(const HSLVector3f *v);
/// Computes the 3D dot product of two vectors
HSL_PUBLIC_FUNCTION(float) HSL_Vector3fDot(const HSLVector3f *a, const HSLVector3f *b);
/// Compute the 3D cross product of two vectors
HSL_PUBLIC_FUNCTION(HSLVector3f) HSL_Vector3fCross(const HSLVector3f *a, const HSLVector3f *b);
/// Computes the min value of two vectors along each component
HSL_PUBLIC_FUNCTION(HSLVector3f) HSL_Vector3fMin(const HSLVector3f *a, const HSLVector3f *b);
/// Computes the max value of two vectors along each component
HSL_PUBLIC_FUNCTION(HSLVector3f) HSL_Vector3fMax(const HSLVector3f *a, const HSLVector3f *b);

// HSLVector3i Methods

/// Adds two 3D vectors together
HSL_PUBLIC_FUNCTION(HSLVector3i) HSL_Vector3iAdd(const HSLVector3i *a, const HSLVector3i *b);
/// Subtracts one 3D vector from another 3D vector
HSL_PUBLIC_FUNCTION(HSLVector3i) HSL_Vector3iSubtract(const HSLVector3i *a, const HSLVector3i *b);
/// Divides each component of a 3D vector by a scalar without checking for divide-by-zero
HSL_PUBLIC_FUNCTION(HSLVector3i) HSL_Vector3iUnsafeScalarDivide(const HSLVector3i *numerator, const int divisor);
/// Divides each component of a 3D vector by the corresponding componenet of another vector without checking for a divide-by-zero
HSL_PUBLIC_FUNCTION(HSLVector3i) HSL_Vector3iUnsafeVectorDivide(const HSLVector3i *numerator, const HSLVector3i *divisor);
/// Divides each component of a 3D vector by a scalar, returning a default vector in the case of divide by zero
HSL_PUBLIC_FUNCTION(HSLVector3i) HSL_Vector3iSafeScalarDivide(const HSLVector3i *numerator, const int divisor, const HSLVector3i *default_result);
/// Divides each component of a 2D vector by another vector, returning a default value for each component in the case of divide by zero
HSL_PUBLIC_FUNCTION(HSLVector3i) HSL_Vector3iSafeVectorDivide(const HSLVector3i *numerator, const HSLVector3i *divisor, const HSLVector3i *default_result);
/// Computes the absolute value of each component
HSL_PUBLIC_FUNCTION(HSLVector3i) HSL_Vector3iAbs(const HSLVector3i *v);
/// Squares each component of a vector
HSL_PUBLIC_FUNCTION(HSLVector3i) HSL_Vector3iSquare(const HSLVector3i *v);
/// Computes the squared-length of a given vector
HSL_PUBLIC_FUNCTION(int) HSL_Vector3iLengthSquared(const HSLVector3i *v);
/// Computes the minimum component of a given vector
HSL_PUBLIC_FUNCTION(int) HSL_Vector3iMinValue(const HSLVector3i *v);
/// Computes the maximum component of a given vector
HSL_PUBLIC_FUNCTION(int) HSL_Vector3iMaxValue(const HSLVector3i *v);
/// Computes the min value of two vectors along each component
HSL_PUBLIC_FUNCTION(HSLVector3i) HSL_Vector3iMin(const HSLVector3i *a, const HSLVector3i *b);
/// Computes the max value of two vectors along each component
HSL_PUBLIC_FUNCTION(HSLVector3i) HSL_Vector3iMax(const HSLVector3i *a, const HSLVector3i *b);
/// Convertes a 3D int vector to a 3D float vector
HSL_PUBLIC_FUNCTION(HSLVector3f) HSL_Vector3iCastToFloat(const HSLVector3i *v);

// HSLQuatf Methods

/// Construct a quaternion from raw w, x, y, and z components
HSL_PUBLIC_FUNCTION(HSLQuatf) HSL_QuatfCreate(float w, float x, float y, float z);
/// Construct a quaternion rotation from rotations about the X, Y, and Z axis
HSL_PUBLIC_FUNCTION(HSLQuatf) HSL_QuatfCreateFromAngles(const HSLVector3f *eulerAngles);
/// Component-wise add two quaternions together (used by numerical integration)
HSL_PUBLIC_FUNCTION(HSLQuatf) HSL_QuatfAdd(const HSLQuatf *a, const HSLQuatf *b);
/// Scale all components of a quaternion by a scalar (used by numerical integration)
HSL_PUBLIC_FUNCTION(HSLQuatf) HSL_QuatfScale(const HSLQuatf *q, const float s);
/// Compute the multiplication of two quaterions
HSL_PUBLIC_FUNCTION(HSLQuatf) HSL_QuatfMultiply(const HSLQuatf *a, const HSLQuatf *b);
/// Divide all components of a quaternion by a scalar without checking for divide by zero
HSL_PUBLIC_FUNCTION(HSLQuatf) HSL_QuatfUnsafeScalarDivide(const HSLQuatf *q, const float s);
/// Divide all components of a quaternion by a scalar, returning a default quaternion in case of a degenerate quaternion
HSL_PUBLIC_FUNCTION(HSLQuatf) HSL_QuatfSafeScalarDivide(const HSLQuatf *q, const float s, const HSLQuatf *default_result);
/// Compute the complex conjegate of a quaternion (negate imaginary components)
HSL_PUBLIC_FUNCTION(HSLQuatf) HSL_QuatfConjugate(const HSLQuatf *q);
/// Concatenate a second quaternion's rotation on to the end of a first quaternion's quaterion (just a quaternion multiplication)
HSL_PUBLIC_FUNCTION(HSLQuatf) HSL_QuatfConcat(const HSLQuatf *first, const HSLQuatf *second);
/// Rotate a vector by a given quaternion
HSL_PUBLIC_FUNCTION(HSLVector3f) HSL_QuatfRotateVector(const HSLQuatf *q, const HSLVector3f *v);
/// Compute the length of a quaternion (sum of squared components)
HSL_PUBLIC_FUNCTION(float) HSL_QuatfLength(const HSLQuatf *q);
/// Computes the normalized version of a quaternion, returning a default quaternion in the event of a near zero length quaternion
HSL_PUBLIC_FUNCTION(HSLQuatf) HSL_QuatfNormalizeWithDefault(const HSLQuatf *q, const HSLQuatf *default_result);

// HSLMatrix3f Methods
/// Create a 3x3 matrix from a set of 3 basis vectors (might not be ortho-normal)
HSL_PUBLIC_FUNCTION(HSLMatrix3f) HSL_Matrix3fCreate(const HSLVector3f *basis_x, const HSLVector3f *basis_y, const HSLVector3f *basis_z);
/// Create a 3x3 rotation matrix from a quaternion
HSL_PUBLIC_FUNCTION(HSLMatrix3f) HSL_Matrix3fCreateFromQuatf(const HSLQuatf *q);
/// Extract the x-axis basis vector from a 3x3 matrix
HSL_PUBLIC_FUNCTION(HSLVector3f) HSL_Matrix3fBasisX(const HSLMatrix3f *m);
/// Extract the y-axis basis vector from a 3x3 matrix
HSL_PUBLIC_FUNCTION(HSLVector3f) HSL_Matrix3fBasisY(const HSLMatrix3f *m);
/// Extract the z-axis basis vector from a 3x3 matrix
HSL_PUBLIC_FUNCTION(HSLVector3f) HSL_Matrix3fBasisZ(const HSLMatrix3f *m);

//-- constants -----
/// A 3D integer vector whose components are all 0
HSL_PUBLIC_CLASS extern const HSLVector3i *k_HSL_int_vector3_zero;
/// A 3D float vector whose components are all 0.0f
HSL_PUBLIC_CLASS extern const HSLVector3f *k_HSL_float_vector3_zero;
/// A 3D integer vector whose components are all 1
HSL_PUBLIC_CLASS extern const HSLVector3i *k_HSL_int_vector3_one;
/// A 3D float vector whose components are all 1.0f
HSL_PUBLIC_CLASS extern const HSLVector3f *k_HSL_float_vector3_one;
/// The 3D float vector <1.0f, 0.0f, 0.0f>
HSL_PUBLIC_CLASS extern const HSLVector3f *k_HSL_float_vector3_i;
/// The 3D float vector <0.0f, 1.0f, 0.0f>
HSL_PUBLIC_CLASS extern const HSLVector3f *k_HSL_float_vector3_j;
/// The 3D float vector <0.0f, 0.0f, 1.0f>
HSL_PUBLIC_CLASS extern const HSLVector3f *k_HSL_float_vector3_k;
/// A 3D float vector that represents the world origin <0.f, 0.f, 0.f>
HSL_PUBLIC_CLASS extern const HSLVector3f *k_HSL_position_origin;
/// The quaterion <1.f, 0.f, 0.f, 0.f> that represents no rotation
HSL_PUBLIC_CLASS extern const HSLQuatf *k_HSL_quaternion_identity;
/// The 3x3 matrix that represent no transform (diagonal values 1.f, off diagonal values 0.f)
HSL_PUBLIC_CLASS extern const HSLMatrix3f *k_HSL_matrix_identity;

/** 
@} 
*/ 

//cut_after
#endif
