#include "car_config.hpp"
#include "menu_assets.hpp"
#include <algorithm>
#include <cmath>
#include <regex>
#include <stdexcept>
namespace ofo {
namespace {
// Preserve quoted strings while removing both original comment syntaxes.
std::string uncomment(const std::string &s) {
    std::string out; char quote=0;
    for(size_t i=0;i<s.size();++i) {
        char c=s[i];
        if(quote){out+=c;if(c=='\\'&&i+1<s.size())out+=s[++i];else if(c==quote)quote=0;continue;}
        if(c=='\''||c=='"'){quote=c;out+=c;continue;}
        if(c=='#'||s.compare(i,2,"--")==0){
            if(s.compare(i,4,"--[[")==0){auto end=s.find("]]",i+4);i=end==s.npos?s.size():end+1;}
            else {auto end=s.find('\n',i);i=end==s.npos?s.size():end;}
            out+='\n';continue;
        }
        out+=c;
    }return out;
}
Json literal(const std::string &s,const std::string &symbol) {
    auto at=s.find(symbol); if(at==s.npos)return Json::object();
    auto eq=s.find('=',at+symbol.size());if(eq==s.npos)return Json::object();
    auto first=s.find_first_not_of(" \r\n\t",eq+1);if(first==s.npos||s[first]!='{')return Json::object();
    unsigned depth=0;char quote=0;
    for(size_t i=first;i<s.size();++i){char c=s[i];if(quote){if(c=='\\')++i;else if(c==quote)quote=0;continue;}
        if(c=='\''||c=='"'){quote=c;continue;}if(c=='{')++depth;if(c=='}'&&!--depth)return parse_data("value="+s.substr(first,i-first+1)).at("value");}
    throw std::runtime_error("Unterminated equipment literal: "+symbol);
}
void flatten(Json &out,Json &prov,const Json &node,const std::string &prefix,const std::string &source,const std::string &sourcekey) {
    if(!node.is_object())return;
    for(auto it=node.begin();it!=node.end();++it){
        // Numbered tables represent vectors/curves, not child setting names.
        if(it.value().is_object()&&!it.value().contains("1")&&!it.value().contains("symbol"))flatten(out,prov,it.value(),prefix+it.key(),source,sourcekey+"."+it.key());
        else {out[prefix+it.key()]=it.value();prov[prefix+it.key()]={{"source",source},{"key",sourcekey+"."+it.key()}};}
    }
}
// Upgrade effect tables mirror Settings/MaxSettings, including axle and tire
// LocalDynamics children. Accumulate leaf weights before interpolating once.
void accumulate_effects(Json&weights,const Json&effects){
    if(!effects.is_object())return;
    for(const auto&field:effects.items()){
        if(field.value().is_number())weights[field.key()]=(weights.contains(field.key())&&weights[field.key()].is_number()?weights[field.key()].get<double>():0.)+field.value().get<double>();
        else if(field.value().is_object()){if(!weights.contains(field.key())||!weights[field.key()].is_object())weights[field.key()]=Json::object();accumulate_effects(weights[field.key()],field.value());}
    }
}
void interpolate_effects(Json&data,const Json&maximum,const Json&weights){
    if(!data.is_object()||!maximum.is_object()||!weights.is_object())return;
    for(const auto&field:weights.items()){
        const auto&key=field.key();if(!data.contains(key)||!maximum.contains(key))continue;
        if(field.value().is_object())interpolate_effects(data[key],maximum[key],field.value());
        else if(field.value().is_number()&&data[key].is_number()&&maximum[key].is_number()){double a=data[key],b=maximum[key];data[key]=a+(b-a)*std::clamp(field.value().get<double>(),0.,1.);}
    }
}
double number(const Json &v,double fallback=0){return v.is_number()?v.get<double>():fallback;}
}
Json car_configuration(const Resources &r,const std::string &folder,const std::vector<std::string>& upgrades) {
    Json out=Json::object(), settings=Json::object(), provenance=Json::object(), sources=Json::array();
    auto load=[&](const std::string &path){auto p=ui_canonical(path);if(!r.exists(p))return std::string();sources.push_back(p);return uncomment(text_decode(r.read(p)));};
    Json weights=Json::object();if(!upgrades.empty())for(const auto&part:menu_upgrades(r))if(std::find(upgrades.begin(),upgrades.end(),part.id)!=upgrades.end())accumulate_effects(weights,part.effects);
    auto upgrade=[&](Json&data,const Json&maximum,const std::string&section){if(weights.contains(section))interpolate_effects(data,maximum,weights[section]);};
    std::smatch match;std::string normalized=ui_canonical(folder);
    if(!std::regex_search(normalized,match,std::regex("car_?([0-9]+)(?:/|$)")))return out;
    std::string n=match[1],stem="data/equipmnt/car"+n+"/";
    for(auto item:{std::make_pair("body","cb."),std::make_pair("engine","ce."),std::make_pair("gearbox","cg."),std::make_pair("suspension","cs.")}){
        std::string path=stem+item.first+"_car"+n+".bed", text=load(path),symbol;
        Json root=Json::object();
        if(std::string(item.first)=="body")root=literal(text,"Car.Body.Car"+n);
        else {
            std::smatch ref;
            if(std::regex_search(text,ref,std::regex("Settings\\s*=\\s*(Car\\.[A-Za-z]+\\.[A-Za-z0-9_]+)\\.Settings"))){
                symbol=ref[1];path="data/equipmnt/custom/"+std::string(item.first)+"_"+n+".bed";root=literal(load(path),symbol);
            }
        }
        Json data=root.value("Settings",Json::object());std::string section=item.first;section[0]=char(std::toupper(section[0]));upgrade(data,root.value("MaxSettings",Json::object()),section);settings[item.first]=data;
        flatten(out,provenance,data,item.second,ui_canonical(path),"Settings");
        if(std::string(item.first)=="body")flatten(out,provenance,data,"cd.",ui_canonical(path),"Settings");
        if(std::string(item.first)=="body"&&root.contains("Name"))out["name"]=root["Name"];
    }
    // The game explicitly resolves TIRE1 to Car.Tires.DefaultTire1.Settings.
    std::string tireWrapper=load(stem+"tires_car"+n+".bed");
    std::smatch tireRef;std::string tireDefault="DefaultTire1";
    if(std::regex_search(tireWrapper,tireRef,std::regex("Settings\\s*=\\s*GetDefaults\\(\\s*TIRE([0-9]+)\\s*\\)")))tireDefault="DefaultTire"+tireRef[1].str();
    load("data/equipmnt/default/defaults.bed");
    std::string tirepath="data/equipmnt/default/tires_default.bed";
    Json tireRoot=literal(load(tirepath),"Car.Tires."+tireDefault),tire=tireRoot.value("Settings",Json::object());
    upgrade(tire,tireRoot.value("MaxSettings",Json::object()),"Tires");settings["tires"]=tire;flatten(out,provenance,tire,"ct.",tirepath,"Settings");
    auto alias=[&](std::string target,std::string from){if(out.contains(from)){out[target]=out[from];provenance[target]=provenance[from];}};
    for(auto side:{"Front","Rear"})for(auto dim:{"Radius","Width"})alias(std::string("ct.")+side+dim,std::string("ct.")+dim);
    alias("ct.XFriction","ct.LocalDynamicsXFriction");alias("ct.ZFriction","ct.LocalDynamicsZFriction");
    for(int i=1;i<=8;++i){std::string from="cg.Gear"+std::to_string(i)+"Ratio",to="cg.Gear"+std::to_string(i);if(out.contains(from)&&out[from].is_object()&&out[from].contains("2")){out[to]=out[from]["2"];provenance[to]=provenance[from];provenance[to]["component"]=2;}}
    // Original 41c87a..41c89c: body + 2*front tire mass + 2*rear tire mass.
    // Body.Mass already includes the engine; adding ce.Mass counted it twice.
    if(out.contains("cb.Mass")){
        const double tireMass=number(out.value("ct.Mass",Json(0)));
        out["cd.Mass"]=number(out["cb.Mass"])+4*tireMass;
        provenance["cd.Mass"]={{"method","body mass + four tire masses; original 0x41c87a..0x41c89c"},{"sources",Json::array({provenance["cb.Mass"],provenance.value("ct.Mass",Json::object())})}};
    }
    Json curve=Json::array();double power=0, powerRpm=0, peakTorque=0;
    int active=std::clamp(int(number(out.value("ce.TorqueCurveParams",Json(0)))),0,64);
    double scale=number(out.value("ce.TorqueMax",Json(0)));
    for(int i=0;i<active;++i){std::string key="ce.Torque"+(i<10?std::string("0"):std::string())+std::to_string(i);auto p=out.value(key,Json::object());if(!p.is_object()||!p.contains("1")||!p.contains("2"))continue;double rpm=number(p["1"]),nm=number(p["2"])*scale;if(rpm<0||nm<0)continue;curve.push_back({rpm,nm});peakTorque=std::max(peakTorque,nm);double kw=rpm*nm*2.*3.141592653589793/60000.;if(kw>power){power=kw;powerRpm=rpm;}}
    if(!curve.empty()){out["torque_curve"]=curve;out["ce.PeakTorque"]=peakTorque;out["peak_power_kw"]=power;out["ce.PeakPowerRpm"]=powerRpm;out["ce.RedLineRpm"]=curve.back()[0];provenance["ce.RedLineRpm"]={{"method","last active torque sample RPM; explicit engine limiter not authored"}};provenance["torque_curve"]={{"method","TorqueNN[2] * TorqueMax; only TorqueCurveParams active samples"},{"source","data/equipmnt/custom/engine_"+n+".bed"}};}
    // Legacy flattened profiles remain a supported explicit override.
    std::string legacy=ui_canonical(folder+"/car.bed");if(r.exists(legacy)){auto values=parse_data(text_decode(r.read(legacy)));for(auto it=values.begin();it!=values.end();++it){out[it.key()]=it.value();provenance[it.key()]={{"source",legacy},{"key",it.key()}};}sources.push_back(legacy);}
    out["source_settings"]=settings;out["configuration_sources"]=sources;out["field_provenance"]=provenance;
    return out;
}
}
