#include "menu_assets.hpp"
#include <regex>
#include "car_config.hpp"
#include <cmath>
#include <functional>
#include <codecvt>
#include <locale>
namespace ofo {
std::map<std::string,std::string> menu_strings(const Resources&r){std::map<std::string,std::string> out;try{auto names=parse_data(text_decode(r.read("data/language/languages.bed")));auto bytes=r.read("data/language/language0.dat");auto u=[&](size_t p){if(p+4>bytes.size())throw std::runtime_error("language bounds");return uint32_t(bytes[p])|uint32_t(bytes[p+1])<<8|uint32_t(bytes[p+2])<<16|uint32_t(bytes[p+3])<<24;};auto count=u(0);if(count>10000||4ull+count*8ull>bytes.size())return out;std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,char16_t> convert;
 for(auto&it:names.items()){if(!it.value().is_number())continue;int id=it.value().get<int>();if(id<0||uint32_t(id)>=count)continue;size_t at=u(4+id*8),len=u(8+id*8);if(len>10000||at+len*2ull>bytes.size())continue;std::u16string word;for(size_t i=0;i<len;++i){char16_t c=bytes[at+i*2]|uint16_t(bytes[at+i*2+1])<<8;if(!c)break;word+=c;}out[it.key()]=convert.to_bytes(word);}
 }catch(const std::exception&){}return out;}
std::vector<MenuUpgrade> menu_upgrades(const Resources&r){std::vector<MenuUpgrade> result;try{auto source=text_decode(r.read("data/equipmnt/tuning_data.bed"));auto at=source.find("tuning_data =");if(at==source.npos)return result;source=std::regex_replace(source.substr(at),std::regex(R"(noimage\(\))"),"nil");auto parsed=parse_data(source).at("tuning_data").at("submenus");auto strings=menu_strings(r);auto symbol=[](const Json&v){return v.is_object()?v.value("symbol",std::string()):v.is_string()?v.get<std::string>():std::string();};auto title=[&](const Json&v){auto key=symbol(v);auto found=strings.find(key);return found==strings.end()?key:found->second;};for(auto category:{"engine","drivetrain","exhaust","suspension","tires","body"}){if(!parsed.contains(category))continue;auto items=parsed.at(category).at("items");for(int i=1;items.contains(std::to_string(i));++i){auto&v=items.at(std::to_string(i));MenuUpgrade u;u.category=category;u.title=title(v.value("1",Json()));u.description=title(v.value("Description",Json()));u.id=symbol(v.value("Upgrade",Json()));u.required=symbol(v.value("RequiredUpgrade",Json()));u.icon=v.value("Icon",std::string());u.dummy=v.value("Dummy",0);u.fov=v.value("Fov",25.);u.racing=v.contains("UpgradeLevel");for(int k=0;k<3;++k)u.prices[k]=v.at("Price").value(std::to_string(k+1),0);u.effects=Json::object();for(auto section:{"Engine","Gearbox","Body","Suspension","Tires"})if(v.contains(section))u.effects[section]=v[section];result.push_back(std::move(u));}}
 }catch(const std::exception&){}return result;}
std::vector<MenuCar> menu_car_catalog(const Resources&r){
 std::vector<MenuCar> out;
 try{auto source=text_decode(r.read("data/scripts/cardealer.bed"));std::regex record(R"rx(\{\s*car\s*=\s*([0-9]+),\s*name\s*=\s*"([^"]+)"\s*,\s*image\s*=\s*"([^"]+)"\s*,\s*class\s*=\s*([0-9]+))rx");for(std::sregex_iterator i(source.begin(),source.end(),record),end;i!=end;++i){int n=std::stoi((*i)[1]);if(!r.exists("data/cars/car_"+std::to_string(n)+"/body.bgm"))continue;auto path="data/menu/"+(*i)[3].str();if(!r.exists(path))path=fs::path(path).replace_extension(".dds").generic_string();size_t index=0;for(int j=1;j<n;j++)if(r.exists("data/cars/car_"+std::to_string(j)+"/body.bgm"))++index;out.push_back({n,std::stoi((*i)[4]),(*i)[2],path,index,menu_artwork(r,"car_icons","car"+std::to_string(n)),menu_artwork(r,"car_logos","car"+std::to_string(n))});}}
 catch(const std::exception&){}
 Bytes palette;try{palette=r.read("data/global/overlay/palette.act");}catch(const std::exception&){}
 std::string dealer;try{dealer=text_decode(r.read("data/scripts/cardealer.bed"));}catch(const std::exception&){}
 const std::map<std::string,std::string> descriptions={
 {"PEPPER","A compact starter with modest power and a light body. Keep your speed through the bends and choose your battles in traffic."},
 {"BLOCKER","A sturdy starter built for close racing. Use its weight to hold your line, then build momentum before the next straight."},
 {"GRINDER","A rough and ready racer for the novice class. Balanced pace and a tough body reward a steady approach through the pack."},
 {"BLOCKHEAD","A solid novice contender with a taste for contact. Carry momentum through corners and make every opening count."},
 {"BULLET","A lively novice racer that rewards clean exits. Stay out of heavy traffic and save your nitro for a clear run."},
 {"OVERRUNNER","A step up in pace for the intermediate class. Plan your braking early and put the power down on the way out."},
 {"SLIDER","An intermediate racer for drivers who enjoy working the corners. Balance the throttle and keep the car pointing toward the exit."},
 {"ROAD KING","A determined contender for the intermediate class. Build speed on open sections and defend your place through traffic."},
 {"FASTHAMMER","An intermediate machine with an aggressive streak. Keep it settled through the bends before attacking the next straight."},
 {"SPEEDEVIL","A fast intermediate contender for committed drivers. A clear line and well timed nitro make the most of its pace."},
 {"THUNDERBOLT","An advanced racer built for a quicker field. Smooth steering and controlled braking keep its speed working for you."},
 {"TRASHER","A strong advanced contender that can race in the thick of the pack. Hold your line and make room for a fast exit."},
 {"MACHINE","An advanced machine for precise driving. Pick your line early and keep the throttle smooth when the road gets tight."},
 {"BONECRACKER","An advanced contender that rewards clean racing. Protect your momentum and time each burst of nitro for open road."},
 {"RANKER1","A powerful advanced racer for the front of the field. Control the corner entry and unleash its pace on the exit."},
 {"BLADE","A sharp advanced contender for fast racing. Keep your inputs measured and look ahead to the next clear stretch of road."}};
 for(auto& car:out){auto description=descriptions.find(car.name);car.description=description==descriptions.end()?"A bonus racer with its own character. Take it for a run and find the line that suits your driving.":description->second;
  const auto folder="data/cars/car_"+std::to_string(car.number);
  for(int skin=1;skin<=5;++skin){auto base=folder+"/skin"+std::to_string(skin);if(r.exists(base+".dds"))car.skins.push_back({skin,base+".dds",r.exists(base+"_damaged.dds")?base+"_damaged.dds":""});}
  for(int ai=1;ai<=2;++ai){auto base=folder+"/skinai"+std::to_string(ai);if(r.exists(base+".dds"))car.skins.push_back({100+ai,base+".dds",r.exists(base+"_damaged.dds")?base+"_damaged.dds":""});}
  if(palette.size()>=768){try{std::regex start("\\bcar\\s*=\\s*"+std::to_string(car.number)+"\\s*,");std::smatch hit;if(std::regex_search(dealer,hit,start)){auto tail=hit.suffix().str();auto end=tail.find("car=");if(end!=std::string::npos)tail.resize(end);auto colors=tail.find("colors");if(colors!=std::string::npos){tail=tail.substr(colors);std::regex pair(R"(GetPaletteColor\(\s*([0-9]+)\s*\)\s*,\s*GetPaletteColor\(\s*([0-9]+)\s*\))");size_t skin=0;for(std::sregex_iterator i(tail.begin(),tail.end(),pair),last;i!=last&&skin<car.skins.size();++i,++skin){int a=std::stoi((*i)[1]),b=std::stoi((*i)[2]);if(a<0||a>255||b<0||b>255)continue;for(int k=0;k<3;++k){car.skins[skin].colors[0][k]=palette[a*3+k]/255.f;car.skins[skin].colors[1][k]=palette[b*3+k]/255.f;}car.skins[skin].has_colors=true;}}}}catch(const std::exception&){} }
  try{auto config=car_configuration(r,folder);car.stats.horsepower=config.value("ce.Horsepower",0.);car.stats.weight=config.value("cb.Mass",config.value("cd.Mass",0.));car.stats.torque=config.value("ce.TorqueMax",0.);
   auto body=text_decode(r.read("data/equipmnt/car"+std::to_string(car.number)+"/body_car"+std::to_string(car.number)+".bed"));
   auto value=[&](const std::string& name){std::smatch m;if(std::regex_search(body,m,std::regex("\\b"+name+"\\s*=\\s*([0-9]+)")))return std::stoi(m[1]);return 0;};
   car.stats.price=value("Price");car.stats.speed=value("Speed");car.stats.acceleration=value("Acceleration");car.stats.strength=value("Strength");
  }catch(const std::exception&){}
 }
 for(int index=1;index<=4;++index){
  const auto number=std::to_string(index);
  const auto folder="alpha_content/cars/car_"+number+"/";
  if(!r.exists(folder+"model.car"))continue;
  try{
   const auto config=load_vehicle(r,16+index);
   MenuCar car;
   car.number=config.id;
   car.car_class=alpha_car_class;
   car.scene_index=out.size();
   car.name=config.name;
   car.image="alpha_content/menu/car/car"+number+".tga";
   if(r.exists(car.image))car.icon={car.image,512,128,0,0,224,104};
   car.stats.horsepower=config.provenance.value("horsepower",0.);
   car.stats.weight=config.mass;
   car.stats.torque=config.engine_torque;
   car.description="An original prototype from Bugbear Retro Demo 2002.";
   car.skins.push_back({1,folder+"body.dds",{}});
   out.push_back(std::move(car));
  }catch(const std::exception&){/* Unavailable prototype does not affect the retail dealer. */}
 }
 return out;
}
std::vector<MenuTrack> alpha_track_catalog(const Resources& r){
 std::vector<MenuTrack> result;
 for(int index=1;index<=2;++index){
  const auto number=std::to_string(index);
  MenuTrack track;
  track.menu_class=alpha_car_class;
  track.name="ALPHA TRACK "+number;
  track.stage="alpha_content/tracks/track"+number;
  track.track=stage_geometry(track.stage);
  track.preview="alpha_content/menu/track/track"+number+"a.tga";
  track.laps=1;
  track.event="FREEROAM";
  if(r.exists(track.track))result.push_back(std::move(track));
 }
 return result;
}
std::vector<MenuTrack> menu_track_catalog(const Resources&r){
 std::vector<MenuTrack> result;
 try {
  auto source=text_decode(r.read("data/scripts/levels.bed"));
  std::map<int,MenuTrack> levels;
  std::regex record(R"rx(\[([0-9]+)\]\s*=\s*\{\s*Name\s*=\s*"([^"]+)"[\s\S]*?Laps\s*=\s*([0-9]+)[\s\S]*?TrackPath\s*=\s*BaseLevelPath\s*\.\.\s*"([^"]+)"[\s\S]*?StagePath\s*=\s*BaseLevelPath\s*\.\.\s*"([^"]+)")rx");
  for(std::sregex_iterator i(source.begin(),source.end(),record),end;i!=end;++i){MenuTrack t;t.level=std::stoi((*i)[1]);t.name=(*i)[2];t.laps=std::stoi((*i)[3]);auto record=(*i)[0].str();auto triplet=[&](const char*key,auto&values){std::smatch m;if(std::regex_search(record,m,std::regex(std::string(key)+R"(\s*=\s*\{\s*\[1\]\s*=\s*([0-9]+)\s*,\s*\[2\]\s*=\s*([0-9]+)\s*,\s*\[3\]\s*=\s*([0-9]+))")))for(int k=0;k<3;++k)values[k]=std::stoi(m[k+1]);};triplet("Winnings",t.prizes);triplet("Score",t.scores);t.track=ui_canonical("data/tracks/"+(*i)[4].str()+"geometry/track_geom.w32");t.stage=ui_canonical("data/tracks/"+(*i)[5].str());levels[t.level]=t;}
  auto mapping=parse_data(text_decode(r.read("data/scripts/trackmapping.bed"))).at("Classes");
  auto layoutSource=text_decode(r.read("data/scripts/tracklayout.bed"));
  layoutSource=std::regex_replace(layoutSource,std::regex(R"(POS\(\s*([0-9]+)\s*,\s*([0-9]+)\s*\))"),"{$1,$2}");
  auto layout=parse_data(layoutSource).at("trackselection_layout");
  for(int c=1;c<=4;++c){auto key=std::to_string(c);if(!layout.contains(key)||!mapping.contains(key))continue;auto grid=layout.at(key);for(int y=1;grid.contains(std::to_string(y));++y){auto line=grid.at(std::to_string(y));for(int x=1;line.contains(std::to_string(x));++x){auto cell=line.at(std::to_string(x));auto course=cell.at("3");auto item=mapping.at(key).at("Courses").at(std::to_string(course.at("1").get<int>())).at(std::to_string(course.at("2").get<int>()));int id=item.at("Level");if(!levels.count(id))continue;auto t=levels.at(id);if(!r.exists(t.track))continue;t.menu_class=c;t.column=x;t.row=y;t.x=48+cell.at("1").at("1").get<float>();t.y=36+cell.at("1").at("2").get<float>();t.icon=cell.at("2");t.name_icon=item.value("NameIcon",std::string());t.event=item.value("EventType",std::string("RACE"));t.subevent=item.value("SubEventType",std::string());if(item.contains("Name"))t.name=item.at("Name");if(item.contains("Filename"))t.preview=ui_canonical("data/menu/bg/"+item.at("Filename").get<std::string>());result.push_back(t);}}}
 }catch(const std::exception&){}
 return result;
}
bool apply_menu_skin(Json& node,const Resources&r,int car,int skin){
 const auto folder="data/cars/car_"+std::to_string(car)+"/";
 const auto stem=(skin==101||skin==102)?"skinai"+std::to_string(skin-100):"skin"+std::to_string(skin);
 const auto selected=folder+stem+".dds";
 if(!((skin>=1&&skin<=5)||skin==101||skin==102)||!r.exists(selected))return false;
 bool changed=false;
 std::function<void(Json&)> visit=[&](Json& j){
  if(j.is_object()){
   if(j.contains("materials")&&j["materials"].is_array())for(auto& material:j["materials"])if(material.contains("textures"))for(auto& ref:material["textures"]){
    if(!ref.is_string()||ref.get<std::string>().empty())continue;auto path=ui_canonical(ref.get<std::string>()),name=fs::path(path).filename().string();
    // Ignore explicit references belonging to another car in a multi-car scene.
    if(path.find("data/cars/car_")!=std::string::npos&&path.find(folder)!=0)continue;
    if(name=="body.tga"||name=="body.dds"||std::regex_match(name,std::regex("skin(ai)?[0-9]+(_damaged)?\\.(dds|tga)"))){auto replacement=folder+stem+(name.find("_damaged")!=std::string::npos?"_damaged.dds":".dds");if(r.exists(replacement)){ref=replacement;changed=true;}}
   }
   for(auto it=j.begin();it!=j.end();++it)if(it.key()!="materials")visit(it.value());
  }else if(j.is_array())for(auto& child:j)visit(child);
 };visit(node);return changed;
}
MenuArtwork menu_artwork(const Resources&r,const std::string&item){return menu_artwork(r,"race_start_items",item);}
MenuArtwork menu_artwork(const Resources&r,const std::string&atlas,const std::string&item){
 // BED data defines both the source dimensions and each named rectangle. Parsing
 // data rather than matching text also ignores disabled (-- commented) slices.
 try{
  const auto stem=ui_canonical("data/menu/"+atlas);
  auto data=parse_data(text_decode(r.read(stem+".bed")));
  auto size=data.at(atlas+"_size"),rect=data.at(atlas).at(item);
  auto value=[](const Json&j,int n){return j.at(std::to_string(n)).get<int>();};
  MenuArtwork art{stem+".dds",value(size,1),value(size,2),value(rect,1),value(rect,2),value(rect,3),value(rect,4)};
  if(art.atlas_width<=0||art.atlas_height<=0||art.x<0||art.y<0||!art)return {};
  // Native third-column track-name rectangles extend three pixels beyond
  // their atlas. Clip to the texture edge before emitting normalized UVs.
  art.width=std::min(art.width,art.atlas_width-art.x);
  art.height=std::min(art.height,art.atlas_height-art.y);
  if(!art)return {};
  if(!r.exists(art.texture)){art.texture=stem+".tga";if(!r.exists(art.texture))return {};}
  return art;
 }catch(const std::exception&){return {};}
}
}
