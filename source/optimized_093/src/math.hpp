#pragma once
#include <array>
#include <cmath>
#include <algorithm>
namespace ofo {
struct Vec {float x=0,y=0,z=0;float& operator[](int i){return i==0?x:i==1?y:z;}float operator[](int i)const{return i==0?x:i==1?y:z;}Vec operator+(Vec b)const{return{x+b.x,y+b.y,z+b.z};}Vec operator-(Vec b)const{return{x-b.x,y-b.y,z-b.z};}Vec operator*(float s)const{return{x*s,y*s,z*s};}Vec operator/(float s)const{return *this*(1/s);}Vec&operator+=(Vec b){*this=*this+b;return *this;}};
inline float dot(Vec a,Vec b){return a.x*b.x+a.y*b.y+a.z*b.z;}inline Vec cross(Vec a,Vec b){return{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}inline float length(Vec a){return std::sqrt(dot(a,a));}inline Vec unit(Vec a,Vec fallback={0,1,0}){float n=length(a);return n>1e-7f?a/n:fallback;}
using Mat=std::array<float,16>;inline Mat identity(){return{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};}inline Vec point(const Mat&m,Vec p){return{m[0]*p.x+m[4]*p.y+m[8]*p.z+m[12],m[1]*p.x+m[5]*p.y+m[9]*p.z+m[13],m[2]*p.x+m[6]*p.y+m[10]*p.z+m[14]};}inline Vec direction(const Mat&m,Vec p){auto a=m;a[12]=a[13]=a[14]=0;return point(a,p);}inline Mat multiply(const Mat&a,const Mat&b){Mat r{};for(int c=0;c<4;c++)for(int row=0;row<4;row++)for(int k=0;k<4;k++)r[c*4+row]+=a[k*4+row]*b[c*4+k];return r;}
inline Mat translate(Vec p){auto m=identity();m[12]=p.x;m[13]=p.y;m[14]=p.z;return m;}
inline Mat rotate_y(float a){auto m=identity();m[0]=m[10]=std::cos(a);m[8]=std::sin(a);m[2]=-m[8];return m;}inline Mat rotate_x(float a){auto m=identity();m[5]=m[10]=std::cos(a);m[6]=std::sin(a);m[9]=-m[6];return m;}
inline Mat look_at(Vec eye,Vec target){Vec f=unit(target-eye,{0,0,1}),r=unit(cross({0,1,0},f),{1,0,0}),u=cross(f,r);return{r.x,u.x,f.x,0,r.y,u.y,f.y,0,r.z,u.z,f.z,0,-dot(r,eye),-dot(u,eye),-dot(f,eye),1};}
inline Mat perspective(float fov,float aspect,float n,float f){Mat m{};m[5]=1/std::tan(fov*.5f);m[0]=m[5]/aspect;m[10]=f/(f-n);m[11]=1;m[14]=-n*f/(f-n);return m;}
}
