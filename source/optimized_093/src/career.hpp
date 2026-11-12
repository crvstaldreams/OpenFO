#pragma once
#include "legacy_ui/menu_assets.hpp"
namespace ofo {
struct CareerEvent {int level=0,car_class=1,course=0,stage=0;std::array<int,7> prizes{};};
// Literal career graph from trackmapping.bed. No BED callbacks are executed.
struct CareerRules {std::vector<CareerEvent> events;std::array<int,3> completion_prizes{{4000,8000,25000}};explicit CareerRules(const Resources&);const CareerEvent* event(int)const;};
struct CareerProgress {
 bool initialized=false,pending=false;int car=0,car_price=0,skin=1,highest_class=1,pending_level=0;uint64_t attempt=0;
 std::map<int,int> positions;std::map<int,double> best_times;std::array<bool,3> classes_paid{{false,false,false}};
 void initialize();bool unlocked(const CareerRules&,int)const;int position(int)const;Json json()const;void load(const Json&);
 // Returns race + first class completion payout; clears pending even on DNF.
 int settle(const CareerRules&,int place,double seconds,bool finished,bool dnf=false);
};
}
