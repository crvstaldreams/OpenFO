#pragma once
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
namespace ofo {
struct MeterRect {float x,y,w,h;};
struct MeterInstruments {MeterRect speed,unit,gear;};
struct MeterNeedle {float angle,pivot_x,pivot_y;};
// Canonical 128px meter coordinates, measured against the native face.
// The two rectangular cells hold speed and units; the round cell holds gear.
inline MeterInstruments meter_instruments(){return{{17,108,38,14},{61,108,25,14},{105,104,17,17}};}
inline MeterNeedle meter_needle(float rpm){return{(135+270*std::clamp(rpm/10000.f,0.f,1.f))*3.14159265f/180,72,62};}
struct MeterLayer {std::string texture;float x=0,y=0,w=256,h=256;};
// Measured channel bounds in the supplied 256px faces; all pixels are original art.
inline std::vector<MeterLayer>meter_artwork(bool battle,bool stunt,float nitro,float damage){
 std::string stem=battle?"meters_based":stunt?"meters_base":"meters_basen";auto base="global/overlay/"+stem+".dds",lit="global/overlay/"+stem+"_lit.dds";
 float value=stunt?0:battle?damage:nitro;if(!std::isfinite(value))value=0;value=std::clamp(value,0.f,1.f);if(value<=0)return{{base,0,0,256,256}};
 float y=192-168*value,h=192-y;
 // Partition base around lit patch so transparent edge pixels are not drawn twice.
 return{{base,0,0,256,y},{base,0,192,256,64},{base,0,y,8,h},{base,72,y,184,h},{lit,8,y,64,h}};
}

// Shared geometry for the 512px cluster atlas and both renderers.
inline constexpr char muscle_meter_texture[]="hud_assets/muscle_meter.dds";
inline constexpr MeterRect muscle_meter_face{0,0,416,244};
inline constexpr MeterRect muscle_meter_needle_sprite{424,0,82,16};
inline constexpr float muscle_needle_axle_x=15, muscle_needle_axle_y=7.5f;
inline constexpr MeterRect muscle_meter_hub_sprite{424,29,16,16};
inline constexpr MeterRect muscle_meter_lamp_sprite{448,30,12,12};
inline constexpr MeterRect muscle_meter_bar_sprite{424,52,80,11};
inline constexpr MeterRect muscle_meter_nitro_label{424,69,80,15};
inline constexpr MeterRect muscle_meter_damage_label{424,88,80,15};
inline constexpr float muscle_meter_imperial_y=248;
inline constexpr float muscle_meter_speed_x=113,muscle_meter_speed_y=108;
inline constexpr float muscle_meter_rpm_x=305,muscle_meter_rpm_y=93;
inline constexpr int muscle_meter_segment_count=10;
inline constexpr MeterRect muscle_meter_digit(int digit){
 return{424.f+(digit%3)*27.f,113.f+(digit/3)*36.f,23,32};
}
inline float muscle_meter_angle(float reading,float maximum){
 const float ratio=std::isfinite(reading)?std::clamp(reading/maximum,0.f,1.f):0.f;
 return (135+270*ratio)*3.14159265f/180;
}
inline float meter_arc_fraction(float value,float minimum=0,float maximum=1){
 return std::isfinite(value)&&maximum>minimum?std::clamp((value-minimum)/(maximum-minimum),0.f,1.f):0.f;
}
struct MeterArcPoint {float x,y,angle;};
inline MeterArcPoint meter_arc_point(float cx,float cy,float radius,float fraction){
 const float angle=(132.f+96.f*meter_arc_fraction(fraction))*3.14159265f/180;
 return{cx+std::cos(angle)*radius,cy+std::sin(angle)*radius,angle};
}
struct MuscleMeterReadings {int speed,gear,segments;float speed_angle,rpm_angle;};
inline MuscleMeterReadings muscle_meter_readings(bool imperial,bool battle,bool stunt,
 float speed_mps,float rpm,int gear,float nitro,float damage){
 const float speed=std::isfinite(speed_mps)?std::min(std::abs(speed_mps)*(imperial?2.236936f:3.6f),999.f):0.f;
 const float gauge=stunt?0.f:battle?damage:nitro;
 const float fill=std::isfinite(gauge)?std::clamp(gauge,0.f,1.f):0.f;
 const int cell=gear<0?10:gear==0?11:std::min(9,gear);
 return{int(speed),cell,int(std::lround(fill*muscle_meter_segment_count)),
        muscle_meter_angle(speed,imperial?150.f:240.f),muscle_meter_angle(rpm,10000.f)};
}
}
