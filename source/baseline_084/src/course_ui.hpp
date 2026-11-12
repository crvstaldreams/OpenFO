#pragma once
#include "legacy_ui/compat.hpp"

#include <optional>
namespace ofo {
// Portable prototype route/checkpoint logic from v0.23 race.hpp/.cpp.
// This module loads authored routes; it does not implement the native AI runtime.
struct CoursePose {std::array<float,16>chassis{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};double speed_kmh=0;bool driver_ejected=false;};
struct CourseInput {double throttle=0,steer=0,brake=0;};
Json race_course(const Resources&, const std::string& base);
struct CourseGuidance {
 bool available=false;
 std::array<double,3> target{};
 // Relative bearing in radians: zero forward, positive right.
 double yaw=0,distance=0;
};
class CourseDirector {
 struct Runner { int next=0,lap=1; double time=0,best=0,last=0,completed_lap=0,stuck=0,reverse=0,ejected=0; bool ready=false,finished=false,route_ready=false;size_t nearest=0; std::array<double,3> previous{}; };
 std::vector<std::array<double,3>>gate_normals_;std::vector<double>gate_widths_,route_arc_;std::vector<size_t>gate_route_;Json course_; std::vector<std::array<double,3>> points_,gates_; std::vector<Runner> runners_; int laps_,bots_;
 struct Projection { std::array<float,16> transform; double distance,arc,half_width; };
 std::vector<uint8_t> reset_map_;
 size_t reset_map_side_=0;
 std::array<double,2> reset_map_origin_{},reset_map_span_{};
 std::vector<double> corridor_widths_;
 std::optional<CoursePose> last_on_route_;
 std::optional<Projection> departure_anchor_;
 std::array<double,2> departure_point_{};
 bool reset_map_contains(double x,double z) const;
 std::optional<Projection> project_route(const CoursePose&) const;
 void update_off_route(const CoursePose&);
public:
 CourseDirector(const Json&,size_t cars,int laps,int bots);
 std::vector<CourseInput> controls(const std::vector<CoursePose>&);
 void update(double,const std::vector<CoursePose>&);
 void reset(size_t); Json hud() const;
 Json summary()const{return hud();}
 bool finished(size_t i=0)const{return i<runners_.size()&&runners_[i].finished;}
 size_t runner_count()const{return runners_.size();}
 std::optional<std::array<float,16>> recovery_transform(const CoursePose&) const;
 bool off_route() const{return departure_anchor_.has_value()&&!finished();}
 // Distance in horizontal world metres to the fixed departure event.
 double off_route_distance(const CoursePose&) const;
 std::optional<std::array<float,16>> departure_transform() const;
 bool recovery_candidate_valid(const std::array<float,16>&) const;
 void relocated(size_t,const CoursePose&);
 bool recovery_requested(size_t i)const{return i<runners_.size()&&runners_[i].ejected>=4;}
 CourseGuidance guidance(size_t,const CoursePose&) const;
 Json leaderboard(const std::vector<CoursePose>&) const;
 bool available() const{return points_.size()>3;}
};
}
