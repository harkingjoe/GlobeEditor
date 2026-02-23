/*3Dベクトル計算、座標変換、当たり判定など、状態を持たない純粋な計算処理*/

#ifndef __MATHUTILS_H__
#define __MATHUTILS_H__


#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


const float SPH_RAD  = 2.0f;

typedef struct { float x, y, z; } Vec3;
typedef struct { int x, y; } Point;
typedef struct { float u, v; } UVPoint;


// ベクトルの基本演算
Vec3 add(Vec3 a, Vec3 b);
Vec3 sub(Vec3 a, Vec3 b);
float dot(Vec3 a, Vec3 b);

// ベクトルの正規化
Vec3 normalize(Vec3 v);

// 任意の軸周りの回転 (ロドリゲスの回転公式)
Vec3 rotate_point(Vec3 p, Vec3 axis, float theta);

// 球とレイ（光線）の衝突判定
// 戻り値: 当たった場合は1、外れた場合は0
int hit_sph(Vec3 org, Vec3 dir, float rad, Vec3 *hit);

// 3D座標(XYZ) と 球面座標(UV) の相互変換
void xyz2uv(Vec3 pos, float rad, float *u, float *v);
Vec3 uv2xyz(float u, float v, float rad);

// 値を最小値～最大値の範囲に収める (UIやブラシサイズ用)
int clamp_val(int val, int min_v, int max_v);

#endif
