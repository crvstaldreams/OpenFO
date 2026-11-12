#pragma once
namespace ofo {
inline std::vector<unsigned char> dx9_decode_dxt(const unsigned char*data,int w,int h,D3DFORMAT format){
 std::vector<unsigned char>out(size_t(w)*h*4);size_t offset=0;const bool dxt1=format==D3DFMT_DXT1;const int block=dxt1?8:16;
 auto rgb=[](unsigned c){return std::array<unsigned,4>{((c>>11)&31)*255/31,((c>>5)&63)*255/63,(c&31)*255/31,255};};
 for(int by=0;by<h;by+=4)for(int bx=0;bx<w;bx+=4){auto*b=data+offset;auto*c=b+(dxt1?0:8);unsigned c0=c[0]|unsigned(c[1])<<8,c1=c[2]|unsigned(c[3])<<8;std::array<std::array<unsigned,4>,4>p{};p[0]=rgb(c0);p[1]=rgb(c1);if(c0>c1||!dxt1){for(int k=0;k<3;k++){p[2][k]=(2*p[0][k]+p[1][k])/3;p[3][k]=(p[0][k]+2*p[1][k])/3;}p[2][3]=p[3][3]=255;}else{for(int k=0;k<3;k++)p[2][k]=(p[0][k]+p[1][k])/2;p[2][3]=255;}
 uint32_t bits;std::memcpy(&bits,c+4,4);std::array<unsigned,8>a{};uint64_t ab=0;if(format==D3DFMT_DXT5){a[0]=b[0];a[1]=b[1];if(a[0]>a[1])for(int i=1;i<=6;i++)a[i+1]=((7-i)*a[0]+i*a[1])/7;else{for(int i=1;i<=4;i++)a[i+1]=((5-i)*a[0]+i*a[1])/5;a[6]=0;a[7]=255;}for(int i=0;i<6;i++)ab|=uint64_t(b[i+2])<<(i*8);}
 for(int k=0;k<16;k++){int x=bx+k%4,y=by+k/4;if(x>=w||y>=h)continue;auto pix=p[(bits>>(k*2))&3];if(format==D3DFMT_DXT3)pix[3]=((b[k/2]>>((k%2)*4))&15)*17;else if(format==D3DFMT_DXT5)pix[3]=a[(ab>>(k*3))&7];for(int j=0;j<4;j++)out[(size_t(y)*w+x)*4+j]=static_cast<unsigned char>(pix[j]);}offset+=block;}
 return out;
}
}
