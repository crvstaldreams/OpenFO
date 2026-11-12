#include "render_quality.hpp"
#include <string>
#include <cassert>
int main(){using namespace ofo;auto l=render_quality(0),m=render_quality(1),h=render_quality(2),u=render_quality(3);assert(l.geometry_error>m.geometry_error&&m.geometry_error>h.geometry_error&&u.geometry_error==0);assert(l.grass_distance<m.grass_distance&&m.grass_distance<h.grass_distance);assert(h.grass_distance==1&&u.grass_distance==1);assert(l.particle_stride==4&&m.particle_stride==2&&h.particle_stride==1);assert(render_quality(-20).level==0&&render_quality(99).level==3);assert(std::string(render_quality_name(2))=="HIGH");}
