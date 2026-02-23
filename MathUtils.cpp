#include "MathUtils.h"


// 任意の軸(axis)周りに点(p)を角度(theta)だけ回転させる関数
Vec3 rotate_point(Vec3 p, Vec3 axis, float theta) {
    // v_rot = v * cos(t) + (k x v) * sin(t) + k * (k . v) * (1 - cos(t))
    float c = cos(theta);
    float s = sin(theta);
    
    // 外積 (axis x p)
    Vec3 cross = {
        axis.y * p.z - axis.z * p.y,
        axis.z * p.x - axis.x * p.z,
        axis.x * p.y - axis.y * p.x
    };
    
    // 内積 (axis . p)
    float dot_val = axis.x * p.x + axis.y * p.y + axis.z * p.z;
    
    return {
        p.x * c + cross.x * s + axis.x * dot_val * (1 - c),
        p.y * c + cross.y * s + axis.y * dot_val * (1 - c),
        p.z * c + cross.z * s + axis.z * dot_val * (1 - c)
    };
}

int hit_sph(Vec3 org, Vec3 dir, float rad, Vec3 *hit) {
    Vec3 oc = org;
    float b = dot(oc, dir);
    float c = dot(oc, oc) - rad * rad;
    float h = b*b - c;
    if (h < 0.0) return 0;
    float t = -b - std::sqrt(h);
    if (t < 0.0) return 0;

    hit->x = org.x + t*dir.x;
    hit->y = org.y + t*dir.y;
    hit->z = org.z + t*dir.z;
    return 1;
}

Vec3 normalize(Vec3 v) {
    float l = std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);
    if(l==0) return v;
    return (Vec3){v.x/l, v.y/l, v.z/l};
}

Vec3 sub(Vec3 a, Vec3 b) {
    return (Vec3){a.x-b.x, a.y-b.y, a.z-b.z};
}
Vec3 add(Vec3 a, Vec3 b) {
    return (Vec3){a.x+b.x, a.y+b.y, a.z+b.z};
}
float dot(Vec3 a, Vec3 b) {
    return a.x*b.x + a.y*b.y + a.z*b.z;
}


int clamp_val(int val, int min_v, int max_v) {
    if (val < min_v) return min_v;
    if (val > max_v) return max_v;
    return val;
}

void xyz2uv(Vec3 pos, float rad, float *u, float *v) {
    float sin_val = pos.z / rad;
    if (sin_val > 1) sin_val = 1;
    if (sin_val < -1) sin_val = -1;
    float lat = std::asin(sin_val);
    float lon = std::atan2(pos.y, pos.x) + M_PI/2.0f;
    *u = 0.5f + lon/(2.0f*M_PI);
    *v = 0.5f + lat/M_PI; 
}

Vec3 uv2xyz(float u, float v, float rad) {
    float lat = (v - 0.5f) * M_PI;
    float lon = (u - 0.5f) * 2.0f * M_PI;
    float z = rad * std::sin(lat);
    float r = rad * std::cos(lat);
    float t = lon - M_PI/2.0f;
    return (Vec3){r * std::cos(t), r * std::sin(t), z};
}