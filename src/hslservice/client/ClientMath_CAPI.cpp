//-- includes -----
#include "ClientMath_CAPI.h"
#include "MathUtility.h"
#include <algorithm>

//-- constants -----
const HSLVector3f g_HSL_float_vector3_zero= {0.f, 0.f, 0.f};
const HSLVector3f *k_HSL_float_vector3_zero= &g_HSL_float_vector3_zero;

const HSLVector3f g_HSL_float_vector3_one= {1.f, 1.f, 1.f};
const HSLVector3f *k_HSL_float_vector3_one= &g_HSL_float_vector3_one;

const HSLVector3f g_HSL_float_vector3_i = { 1.f, 0.f, 0.f };
const HSLVector3f *k_HSL_float_vector3_i = &g_HSL_float_vector3_i;

const HSLVector3f g_HSL_float_vector3_j = { 0.f, 1.f, 0.f };
const HSLVector3f *k_HSL_float_vector3_j = &g_HSL_float_vector3_j;

const HSLVector3f g_HSL_float_vector3_k = { 0.f, 0.f, 1.f };
const HSLVector3f *k_HSL_float_vector3_k = &g_HSL_float_vector3_k;

const HSLVector3i g_HSL_int_vector3_zero= {0, 0, 0};
const HSLVector3i *k_HSL_int_vector3_zero= &g_HSL_int_vector3_zero;

const HSLVector3i g_HSL_int_vector3_one= {1, 1, 1};
const HSLVector3i *k_HSL_int_vector3_one= &g_HSL_int_vector3_one;

const HSLVector3f g_HSL_position_origin= {0.f, 0.f, 0.f};
const HSLVector3f *k_HSL_position_origin= &g_HSL_position_origin;

const HSLQuatf g_HSL_quaternion_identity= {1.f, 0.f, 0.f, 0.f};
const HSLQuatf *k_HSL_quaternion_identity= &g_HSL_quaternion_identity;

const HSLMatrix3f g_HSL_matrix_identity = { {{1.f, 0.f, 0.f} , {0.f, 1.f, 0.f}, {0.f, 0.f, 1.f}} };
const HSLMatrix3f *k_HSL_matrix_identity = &g_HSL_matrix_identity;

//-- methods -----
// HSLVector2f Methods
HSLVector2f HSL_Vector2fAdd(const HSLVector2f *a, const HSLVector2f *b)
{
	return {a->x + b->x, a->y + b->y};
}

HSLVector2f HSL_Vector2fSubtract(const HSLVector2f *a, const HSLVector2f *b)
{
	return {a->x - b->x, a->y - b->y};
}

HSLVector2f HSL_Vector2fScale(const HSLVector2f *v, const float s)
{
	return {v->x*s, v->y*s};
}

HSLVector2f HSL_Vector2fScaleAndAdd(const HSLVector2f *v, const float s, const HSLVector2f *b)
{
	return {v->x*s + b->x, v->y*s + b->y};
}

HSLVector2f HSL_Vector2fUnsafeScalarDivide(const HSLVector2f *numerator, const float divisor)
{
	return {numerator->x/divisor, numerator->y/divisor};
}

HSLVector2f HSL_Vector2fUnsafeVectorDivide(const HSLVector2f *numerator, const HSLVector2f *divisor)
{
	return {numerator->x/divisor->x, numerator->y/divisor->y};
}

HSLVector2f HSL_Vector2fSafeScalarDivide(const HSLVector2f *numerator, const float divisor, const HSLVector2f *default_result)
{
	return !is_nearly_zero(divisor) ? HSL_Vector2fUnsafeScalarDivide(numerator, divisor) : *default_result;
}

HSLVector2f HSL_Vector2fSafeVectorDivide(const HSLVector2f *numerator, const HSLVector2f *divisor, const HSLVector2f *default_result)
{
	return {!is_nearly_zero(divisor->x) ? (numerator->x / divisor->x) : default_result->x,
			!is_nearly_zero(divisor->y) ? (numerator->y / divisor->y) : default_result->y};
}

HSLVector2f HSL_Vector2fAbs(const HSLVector2f *v)
{
	return {fabsf(v->x), fabsf(v->y)};
}

HSLVector2f HSL_Vector2fSquare(const HSLVector2f *v)
{
	return {v->x*v->x, v->y*v->y};
}

float HSL_Vector2fLength(const HSLVector2f *v)
{
	return sqrtf(v->x*v->x + v->y*v->y);
}

HSLVector2f HSL_Vector2fNormalizeWithDefault(const HSLVector2f *v, const HSLVector2f *default_result)
{
    return HSL_Vector2fSafeScalarDivide(v, HSL_Vector2fLength(v), default_result);
}

float HSL_Vector2fMinValue(const HSLVector2f *v)
{
	return fminf(v->x, v->y);
}

float HSL_Vector2fMaxValue(const HSLVector2f *v)
{
	return fmaxf(v->x, v->y);
}

float HSL_Vector2fDot(const HSLVector2f *a, const HSLVector2f *b)
{
	return a->x*b->x + a->x*b->y;
}

HSLVector2f HSL_Vector2fMin(const HSLVector2f *a, const HSLVector2f *b)
{
	return { fminf(a->x, b->x), fminf(a->y, b->y) };
}

HSLVector2f HSL_Vector2fMax(const HSLVector2f *a, const HSLVector2f *b)
{
	return { fmaxf(a->x, b->x), fmaxf(a->y, b->y) };
}

// HSLVector3f Methods
HSLVector3f HSL_Vector3fAdd(const HSLVector3f *a, const HSLVector3f *b)
{
	return {a->x + b->x, a->y + b->y, a->z + b->z};
}

HSLVector3f HSL_Vector3fSubtract(const HSLVector3f *a, const HSLVector3f *b)
{
	return {a->x - b->x, a->y - b->y, a->z - b->z};
}

HSLVector3f HSL_Vector3fScale(const HSLVector3f *v, const float s)
{
	return {v->x*s, v->y*s, v->z*s};
}

HSLVector3f HSL_Vector3fScaleAndAdd(const HSLVector3f *v, const float s, const HSLVector3f *b)
{
	return {v->x*s + b->x, v->y*s + b->y, v->z*s + b->z};
}

HSLVector3f HSL_Vector3fUnsafeScalarDivide(const HSLVector3f *numerator, const float divisor)
{
	return {numerator->x/divisor, numerator->y/divisor, numerator->z/divisor};
}

HSLVector3f HSL_Vector3fUnsafeVectorDivide(const HSLVector3f *numerator, const HSLVector3f *divisor)
{
	return {numerator->x/divisor->x, numerator->y/divisor->y, numerator->z/divisor->z};
}

HSLVector3f HSL_Vector3fSafeScalarDivide(const HSLVector3f *numerator, const float divisor, const HSLVector3f *default_result)
{
	return !is_nearly_zero(divisor) ? HSL_Vector3fUnsafeScalarDivide(numerator, divisor) : *default_result;
}

HSLVector3f HSL_Vector3fSafeVectorDivide(const HSLVector3f *numerator, const HSLVector3f *divisor, const HSLVector3f *default_result)
{
	return {!is_nearly_zero(divisor->x) ? (numerator->x / divisor->x) : default_result->x,
			!is_nearly_zero(divisor->y) ? (numerator->y / divisor->y) : default_result->y,
			!is_nearly_zero(divisor->z) ? (numerator->z / divisor->z) : default_result->z};
}

HSLVector3f HSL_Vector3fAbs(const HSLVector3f *v)
{
	return {fabsf(v->x), fabsf(v->y), fabsf(v->z)};
}

HSLVector3f HSL_Vector3fSquare(const HSLVector3f *v)
{
	return {v->x*v->x, v->y*v->y, v->z*v->z};
}

float HSL_Vector3fLength(const HSLVector3f *v)
{
	return sqrtf(v->x*v->x + v->y*v->y + v->z*v->z);
}

HSLVector3f HSL_Vector3fNormalizeWithDefault(const HSLVector3f *v, const HSLVector3f *default_result)
{
	return HSL_Vector3fSafeScalarDivide(v, HSL_Vector3fLength(v), default_result);
}

HSLVector3f HSL_Vector3fNormalizeWithDefaultGetLength(const HSLVector3f *v, const HSLVector3f *default_result, float *out_length)
{
	const float length= HSL_Vector3fLength(v);
		
	if (out_length)
		*out_length= length;

	return HSL_Vector3fSafeScalarDivide(v, length, default_result);
}

float HSL_Vector3fMinValue(const HSLVector3f *v)
{
	return fminf(fminf(v->x, v->y), v->z);
}

float HSL_Vector3fMaxValue(const HSLVector3f *v)
{
	return fmaxf(fmaxf(v->x, v->y), v->z);
}

float HSL_Vector3fDot(const HSLVector3f *a, const HSLVector3f *b)
{
	return a->x*b->x + a->y*b->y + a->z*b->z;
}

HSLVector3f HSL_Vector3fCross(const HSLVector3f *a, const HSLVector3f *b)
{
	return {a->y*b->z - b->y*a->z, a->x*b->z - b->x*a->z, a->x*b->y - b->x*a->y};
}

HSLVector3f HSL_Vector3fMin(const HSLVector3f *a, const HSLVector3f *b)
{
	return {fminf(a->x, b->x), fminf(a->y, b->y), fminf(a->z, b->z)};
}

HSLVector3f HSL_Vector3fMax(const HSLVector3f *a, const HSLVector3f *b)
{
	return {fmaxf(a->x, b->x), fmaxf(a->y, b->y), fmaxf(a->z, b->z)};
}

// HSLVector3i Methods
HSLVector3i HSL_Vector3iAdd(const HSLVector3i *a, const HSLVector3i *b)
{
	return {a->x + b->x, a->y + b->y, a->z + b->z};
}

HSLVector3i HSL_Vector3iSubtract(const HSLVector3i *a, const HSLVector3i *b)
{
	return {a->x - b->x, a->y - b->y, a->z - b->z};
}

HSLVector3i HSL_Vector3iUnsafeScalarDivide(const HSLVector3i *numerator, const int divisor)
{
	return {numerator->x/divisor, numerator->y/divisor, numerator->z/divisor};
}

HSLVector3i HSL_Vector3iUnsafeVectorDivide(const HSLVector3i *numerator, const HSLVector3i *divisor)
{
	return {numerator->x/divisor->x, numerator->y/divisor->y, numerator->z/divisor->z};
}

HSLVector3i HSL_Vector3iSafeScalarDivide(const HSLVector3i *numerator, const int divisor, const HSLVector3i *default_result)
{
	return divisor != 0 ? HSL_Vector3iUnsafeScalarDivide(numerator, divisor) : *default_result;
}

HSLVector3i HSL_Vector3iSafeVectorDivide(const HSLVector3i *numerator, const HSLVector3i *divisor, const HSLVector3i *default_result)
{
	return {divisor->x != 0 ? (numerator->x / divisor->x) : default_result->x,
			divisor->y != 0 ? (numerator->y / divisor->y) : default_result->y,
			divisor->z != 0 ? (numerator->z / divisor->z) : default_result->z};
}

HSLVector3i HSL_Vector3iAbs(const HSLVector3i *v)
{
	return {std::abs(v->x), std::abs(v->y), std::abs(v->z)};
}

HSLVector3i HSL_Vector3iSquare(const HSLVector3i *v)
{
	return {v->x*v->x, v->y*v->y, v->z*v->z};
}

int HSL_Vector3iLengthSquared(const HSLVector3i *v)
{
	return v->x*v->x + v->y*v->y + v->z*v->z;
}

int HSL_Vector3iMinValue(const HSLVector3i *v)
{
	return std::min(std::min(v->x, v->y), v->z);
}

int HSL_Vector3iMaxValue(const HSLVector3i *v)
{
	return std::max(std::max(v->x, v->y), v->z);
}

HSLVector3i HSL_Vector3iMin(const HSLVector3i *a, const HSLVector3i *b)
{
	return {std::min(a->x, b->x), std::min(a->y, b->y), std::min(a->z, b->z)};
}

HSLVector3i HSL_Vector3iMax(const HSLVector3i *a, const HSLVector3i *b)
{
	return {std::max(a->x, b->x), std::max(a->y, b->y), std::max(a->z, b->z)};
}

HSLVector3f HSL_Vector3iCastToFloat(const HSLVector3i *v)
{
	return { static_cast<float>(v->x), static_cast<float>(v->y), static_cast<float>(v->z) };
}

// HSLQuatf Methods
HSLQuatf HSL_QuatfCreate(float w, float x, float y, float z)
{
	return {w, x, y, z};
}

HSLQuatf HSL_QuatfCreateFromAngles(const HSLVector3f *eulerAngles)
{
	HSLQuatf q;

	// Assuming the angles are in radians.
	float c1 = cosf(eulerAngles->y / 2.f);
	float s1 = sinf(eulerAngles->y / 2.f);
	float c2 = cosf(eulerAngles->z / 2.f);
	float s2 = sinf(eulerAngles->z / 2.f);
	float c3 = cosf(eulerAngles->x / 2.f);
	float s3 = sinf(eulerAngles->x / 2.f);
	float c1c2 = c1*c2;
	float s1s2 = s1*s2;
	q.w = c1c2*c3 - s1s2*s3;
	q.x = c1c2*s3 + s1s2*c3;
	q.y = s1*c2*c3 + c1*s2*s3;
	q.z = c1*s2*c3 - s1*c2*s3;

	return q;
}

HSLQuatf HSL_QuatfAdd(const HSLQuatf *a, const HSLQuatf *b)
{
	return {a->w + b->w, a->x + b->x, a->y + b->y, a->z + b->z};
}

HSLQuatf HSL_QuatfScale(const HSLQuatf *q, const float s)
{
	return {q->w*s, q->x*s, q->y*s, q->z*s};
}

HSLQuatf HSL_QuatfMultiply(const HSLQuatf *a, const HSLQuatf *b)
{
	return {a->w*b->w - a->x*b->x - a->y*b->y - a->z*b->z,
			a->w*b->x + a->x*b->w + a->y*b->z - a->z*b->y,
			a->w*b->y - a->x*b->z + a->y*b->w + a->z*b->x,
			a->w*b->z + a->x*b->y - a->y*b->x + a->z*b->w};
}

HSLQuatf HSL_QuatfUnsafeScalarDivide(const HSLQuatf *q, const float s)
{
	return {q->w / s, q->x / s, q->y / s, q->z / s};
}

HSLQuatf HSL_QuatfSafeScalarDivide(const HSLQuatf *q, const float s, const HSLQuatf *default_result)
{
	return !is_nearly_zero(s) ? HSL_QuatfUnsafeScalarDivide(q, s) : *default_result;
}

HSLQuatf HSL_QuatfConjugate(const HSLQuatf *q)
{
	return {q->w, -q->x, -q->y, -q->z};
}

HSLQuatf HSL_QuatfConcat(const HSLQuatf *first, const HSLQuatf *second)
{
	return HSL_QuatfMultiply(second, first);
}

HSLVector3f HSL_QuatfRotateVector(const HSLQuatf *q, const HSLVector3f *v)
{
	return {q->w*q->w*v->x + 2*q->y*q->w*v->z - 2*q->z*q->w*v->y + q->x*q->x*v->x + 2*q->y*q->x*v->y + 2*q->z*q->x*v->z - q->z*q->z*v->x - q->y*q->y*v->x,
			2*q->x*q->y*v->x + q->y*q->y*v->y + 2*q->z*q->y*v->z + 2*q->w*q->z*v->x - q->z*q->z*v->y + q->w*q->w*v->y - 2*q->x*q->w*v->z - q->x*q->x*v->y,
			2*q->x*q->z*v->x + 2*q->y*q->z*v->y + q->z*q->z*v->z - 2*q->w*q->y*v->x - q->y*q->y*v->z + 2*q->w*q->x*v->y - q->x*q->x*v->z + q->w*q->w*v->z};
}

float HSL_QuatfLength(const HSLQuatf *q)
{
    return sqrtf(q->w*q->w + q->x*q->x + q->y*q->y + q->z*q->z);
}

HSLQuatf HSL_QuatfNormalizeWithDefault(const HSLQuatf *q, const HSLQuatf *default_result)
{
	return HSL_QuatfSafeScalarDivide(q, HSL_QuatfLength(q), default_result);
}

// HSLMatrix3f Methods
HSLMatrix3f HSL_Matrix3fCreate(const HSLVector3f *basis_x, const HSLVector3f *basis_y, const HSLVector3f *basis_z)
{
    HSLMatrix3f mat;

    mat.m[0][0] = basis_x->x; mat.m[0][1] = basis_x->y; mat.m[0][2] = basis_x->z;
    mat.m[1][0] = basis_y->x; mat.m[1][1] = basis_y->y; mat.m[1][2] = basis_y->z;
    mat.m[2][0] = basis_z->x; mat.m[2][1] = basis_z->y; mat.m[2][2] = basis_z->z;

    return mat;
}

HSLMatrix3f HSL_Matrix3fCreateFromQuatf(const HSLQuatf *q)
{
	HSLMatrix3f mat;

	const float qw = q->w;
	const float qx = q->x;
	const float qy = q->y;
	const float qz = q->z;

	const float qx2 = qx*qx;
	const float qy2 = qy*qy;
	const float qz2 = qz*qz;

	mat.m[0][0] = 1.f - 2.f*qy2 - 2.f*qz2; mat.m[0][1] = 2.f*qx*qy - 2.f*qz*qw;   mat.m[0][2] = 2.f*qx*qz + 2.f*qy*qw;
	mat.m[1][0] = 2.f*qx*qy + 2.f*qz*qw;   mat.m[1][1] = 1.f - 2.f*qx2 - 2.f*qz2; mat.m[1][2] = 2.f*qy*qz - 2.f*qx*qw;
	mat.m[2][0] = 2.f*qx*qz - 2.f*qy*qw;   mat.m[2][1] = 2.f * qy*qz + 2.f*qx*qw; mat.m[2][2] = 1.f - 2.f*qx2 - 2.f*qy2;

	return mat;
}

HSLVector3f HSL_Matrix3fBasisX(const HSLMatrix3f *mat)
{
	return {mat->m[0][0], mat->m[0][1], mat->m[0][2]};
}

HSLVector3f HSL_Matrix3fBasisY(const HSLMatrix3f *mat)
{
	return {mat->m[1][0], mat->m[1][1], mat->m[1][2]};
}

HSLVector3f HSL_Matrix3fBasisZ(const HSLMatrix3f *mat)
{
	return {mat->m[2][0], mat->m[2][1], mat->m[2][2]};
}
