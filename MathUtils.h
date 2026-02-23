/*OpenGLに依存しない純粋な計算処理と構造体だけを定義する。*/

#ifndef __MATHUTILS_H__
#define __MATHUTILS_H__


#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


typedef struct { float x, y, z; } Vec3;
typedef struct { int x, y; } Point;
typedef struct { float u, v; } UVPoint;


Vec3 rotate_point(Vec3 p, Vec3 axis, float theta);

int hit_sph(Vec3 org, Vec3 dir, float rad, Vec3 *hit);
Vec3 normalize(Vec3 v);
Vec3 sub(Vec3 a, Vec3 b);
Vec3 add(Vec3 a, Vec3 b);
float dot(Vec3 a, Vec3 b);

int clamp_val(int val, int min_v, int max_v);
void xyz2uv(Vec3 pos, float rad, float *u, float *v);
Vec3 uv2xyz(float u, float v, float rad);

#endif
