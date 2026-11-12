// Included within Editor. All mutations use a snapshot taken before the modal tool.
std::set<int> selectionIDs;std::map<size_t,int> nativeOverrides;bool xraySelect=false,boxArmed=false,boxActive=false,boxAdd=false,middleDrag=false;POINT boxStart{},boxEnd{};
enum class TransformTool { Move, Rotate, Scale };
struct Modal {
 bool active=false,keyboard=false,wasDirty=false,plane=false,local=false,valid=true,groupedUndo=false;
 TransformTool tool=TransformTool::Move;
 int axis=-1;
 POINT mouseStart{},mouseLast{};
 float dx=0,dy=0;
 Vec pivot{};
 Mat axes=identity();
 TrackDocument before;
 std::vector<int> ids;
 std::vector<TrackPlacement> starts,preview;
 TransformNumber number;
} modal;
Vec orbitPivot{};
float orbitDistance=20;
void rebuildOverrides(){nativeOverrides.clear();for(size_t i=0;i<doc.placements.size();i++)if(doc.placements[i].replace_native)nativeOverrides[doc.placements[i].source_prop]=int(i);}
TrackPlacement nativePlacement(size_t prop)const{const auto&p=geometry.props[prop];TrackPlacement a{prop,{p.transform[12],p.transform[13],p.transform[14]},0,p.id};a.replace_native=true;return a;}
int editNative(size_t prop){auto it=nativeOverrides.find(prop);if(it!=nativeOverrides.end())return it->second;int id=int(doc.placements.size());doc.placements.push_back(nativePlacement(prop));nativeOverrides[prop]=id;return id;}
void clearSelection(){selectionIDs.clear();selected=-1;}
void selectObject(int id,bool extend){if(!extend)clearSelection();if(id<0)return;if(extend&&selectionIDs.count(id)){selectionIDs.erase(id);selected=selectionIDs.empty()?-1:*selectionIDs.rbegin();}else{selectionIDs.insert(id);selected=id;}syncFields();status();}
Vec selectionPivot()const{Vec p{};size_t n=0;for(int id:selectionIDs)if(id>=0&&size_t(id)<doc.placements.size()){auto&a=doc.placements[id];p+=pivot_world(geometry.props[a.source_prop],a,pivotMode());++n;}return n?p/float(n):Vec{};}
int pick(int x,int y){auto ray=screenRay(x,y);if(!ray)return -1;float nearest=5000;int hit=-1;size_t native=SIZE_MAX;if(!xraySelect&&query)if(auto p=query->ray(ray->origin,ray->direction,5000))nearest=length(*p-ray->origin)+.02f;
for(size_t i=0;i<doc.placements.size();i++){auto&a=doc.placements[i];if(auto d=ray_prop_obb(*ray,geometry.props[a.source_prop],a);d&&*d<nearest){nearest=*d;hit=int(i);native=SIZE_MAX;}}
for(size_t i=0;i<geometry.props.size();i++)if(!track_template_only(geometry,i)&&!nativeOverrides.count(i)){auto a=nativePlacement(i);if(auto d=ray_prop_obb(*ray,geometry.props[i],a);d&&*d<nearest){nearest=*d;native=i;hit=-1;}}
return native!=SIZE_MAX?editNative(native):hit;}
void selectAll(bool none){finishDrag(false);clearSelection();if(!none){for(size_t i=0;i<geometry.props.size();i++)if(!track_template_only(geometry,i))selectionIDs.insert(editNative(i));for(size_t i=0;i<doc.placements.size();i++)selectionIDs.insert(int(i));if(!selectionIDs.empty())selected=*selectionIDs.rbegin();}message=none?"Вибір знято":"Вибрано всі динамічні об'єкти";syncFields();status();}
void finishBox(){if(!boxActive)return;boxActive=false;boxArmed=false;if(!boxAdd)clearSelection();int left=std::min(boxStart.x,boxEnd.x),right=std::max(boxStart.x,boxEnd.x),top=std::min(boxStart.y,boxEnd.y),bottom=std::max(boxStart.y,boxEnd.y);
auto inside=[&](const TrackPlacement&a){const auto&p=geometry.props[a.source_prop];auto matrix=placement_matrix(p,a);float loX=1e30f,loY=1e30f,hiX=-1e30f,hiY=-1e30f;bool projected=false;for(int i=0;i<8;i++){auto screen=project_editor_point(point(matrix,p.center+Vec{p.half.x*(i&1?1.f:-1.f),p.half.y*(i&2?1.f:-1.f),p.half.z*(i&4?1.f:-1.f)}),vp(),float(render->width),float(render->height));if(screen){projected=true;loX=std::min(loX,screen->x);loY=std::min(loY,screen->y);hiX=std::max(hiX,screen->x);hiY=std::max(hiY,screen->y);}}if(!projected||hiX<left||loX>right||hiY<top||loY>bottom)return false;if(!xraySelect){Vec c=point(matrix,p.center);auto ray=EditorRay{eye,unit(c-eye)};if(auto hit=query->ray(eye,ray.direction,length(c-eye));hit&&length(*hit-eye)+length(p.half)<length(c-eye))return false;}return true;};
for(size_t i=0;i<doc.placements.size();i++)if(inside(doc.placements[i]))selectionIDs.insert(int(i));for(size_t i=0;i<geometry.props.size();i++)if(!track_template_only(geometry,i)&&!nativeOverrides.count(i)&&inside(nativePlacement(i)))selectionIDs.insert(editNative(i));if(!selectionIDs.empty())selected=*selectionIDs.rbegin();ReleaseCapture();syncFields();status();}
void beginTransform(TransformTool tool,bool keyboard,int axis=-1){
 if(!selection()){message="Спочатку виберіть об’єкт ЛКМ";status();return;}
 finishDrag(true);if(modal.active)return;
 if(selectionIDs.empty())selectionIDs.insert(selected);
 modal={};modal.active=true;modal.keyboard=keyboard;modal.tool=tool;
 modal.wasDirty=dirty;modal.before=doc;modal.pivot=selectionPivot();modal.axis=axis;
 modal.local=axis>=0&&(tool==TransformTool::Scale||axisSpace()==EditorAxisSpace::Local);
 const auto& active=doc.placements[selected];
 modal.axes=placement_rigid_matrix(geometry.props[active.source_prop],active);
 POINT cursor;GetCursorPos(&cursor);ScreenToClient(viewport,&cursor);modal.mouseStart=modal.mouseLast=cursor;
 for(int id:selectionIDs){modal.ids.push_back(id);modal.starts.push_back(doc.placements[id]);}
 SetFocus(viewport);SetCapture(viewport);SendMessageW(modeBox,CB_SETCURSEL,1,0);
 SendMessageW(toolBox,CB_SETCURSEL,int(tool),0);
 message=tool==TransformTool::Rotate?"Поворот: X/Y/Z, градуси, Enter / Esc":
         tool==TransformTool::Scale?"Масштаб: локальні X/Y/Z, коефіцієнт, Enter / Esc":
         "Рух: X/Y/Z, Shift+вісь — площина, метри, Enter / Esc";
 status();
}
Vec modalAxis(int axis)const{
 if(axis<0)return forward();
 Vec unitAxis{};unitAxis[axis]=1;
 return modal.local?unit(direction(modal.axes,unitAxis),unitAxis):unitAxis;
}
bool scaledAxis(int axis)const{return modal.axis<0||(modal.plane?axis!=modal.axis:axis==modal.axis);}
float scaleFactor()const{
 const auto origin=project_editor_point(modal.pivot,vp(),float(render->width),float(render->height));
 if(origin){
  const float x=modal.mouseStart.x-origin->x,y=modal.mouseStart.y-origin->y;
  const float distance=std::hypot(x,y);
  if(distance>=12){
   if(modal.axis>=0&&!modal.plane){
    const auto tip=project_editor_point(modal.pivot+modalAxis(modal.axis)*gizmoSize(modal.pivot),vp(),float(render->width),float(render->height));
    if(tip){
     const float ax=tip->x-origin->x,ay=tip->y-origin->y;
     const float projected=x*ax+y*ay;
     if(std::abs(projected)>12*std::hypot(ax,ay))
      return 1+(modal.dx*ax+modal.dy*ay)/projected;
    }
   }
   return std::hypot(x+modal.dx,y+modal.dy)/distance;
  }
 }
 return std::exp(std::clamp((modal.dx-modal.dy)*.008f,-8.f,8.f));
}
void modalTo(int x,int y){
 if(!modal.active||!render)return;
 const float fine=(GetAsyncKeyState(VK_SHIFT)&0x8000)?.1f:1.f;
 modal.dx+=(x-modal.mouseLast.x)*fine;modal.dy+=(y-modal.mouseLast.y)*fine;
 modal.mouseLast={x,y};
 const auto number=modal.number.value();
 modal.valid=modal.number.empty()||number.has_value();
 const bool ctrl=(GetAsyncKeyState(VK_CONTROL)&0x8000)!=0;
 const float step=ctrl?1:snapStep();
 const Vec axis=modalAxis(modal.axis);
 const auto start=screenRay(modal.mouseStart.x,modal.mouseStart.y);
 const auto now=screenRay(int(modal.mouseStart.x+modal.dx),int(modal.mouseStart.y+modal.dy));
 if(!start||!now)return;
 Vec delta{},factors{1,1,1};
 float angle=0;
 if(modal.tool==TransformTool::Rotate){
  if(number)angle=*number*.0174532925f;
  else{
   const auto a=ray_plane(*start,modal.pivot,axis),b=ray_plane(*now,modal.pivot,axis);
   if(a&&b&&length(*a-modal.pivot)>.01f&&length(*b-modal.pivot)>.01f){
    const Vec u=unit(*a-modal.pivot),v=unit(*b-modal.pivot);
    angle=std::atan2(dot(axis,cross(u,v)),dot(u,v));
   }else angle=modal.dx*.01f;
   if(ctrl||step>0)angle=snap_editor_value(angle,ctrl?.08726646f:.2617994f);
  }
 }else if(modal.tool==TransformTool::Scale){
  float factor=number?*number:scaleFactor(),low=0,high=std::numeric_limits<float>::infinity();
  for(const auto& a:modal.starts)for(int i=0;i<3;++i)if(scaledAxis(i)){
   low=std::max(low,track_min_scale/a.scale[i]);
   high=std::min(high,track_max_scale/a.scale[i]);
  }
  if(number&&(factor<low||factor>high))modal.valid=false;
  if(!number&&(ctrl||step>0))factor=snap_editor_value(factor,.1f);
  factor=std::clamp(factor,low,high);
  for(int i=0;i<3;++i)if(scaledAxis(i))factors[i]=factor;
 }else if(modal.axis>=0&&!modal.plane){
  const auto a=ray_axis_parameter(*start,modal.pivot,axis),b=ray_axis_parameter(*now,modal.pivot,axis);
  const float amount=number?*number:a&&b?*b-*a:
   (modal.dx-modal.dy)*std::max(1.f,length(eye-modal.pivot))*2*std::tan(.525f)/std::max(1,render->height);
  delta=axis*(number?amount:snap_editor_value(amount,step));
 }else{
  const Vec normal=modal.axis>=0?axis:forward();
  const auto a=ray_plane(*start,modal.pivot,normal),b=ray_plane(*now,modal.pivot,normal);
  if(a&&b)delta=*b-*a;
  if(number){
   const Vec fallback=modal.axis>=0?modalAxis((modal.axis+1)%3):unit(cross({0,1,0},forward()));
   delta=unit(delta,fallback)*(*number);
  }else if(step>0){
   for(int i=0;i<3;++i)delta[i]=snap_editor_value(delta[i],step);
   if(modal.axis>=0)delta=delta-axis*dot(delta,axis);
  }
 }
 modal.preview.clear();
 if(modal.valid)for(const auto& original:modal.starts){
  auto a=original;
  const auto& source=geometry.props[a.source_prop];
  if(modal.tool==TransformTool::Scale)modal.valid=scale_placement_about_pivot(source,a,pivotMode(),factors,modal.pivot,modal.axes);
  else if(modal.tool==TransformTool::Rotate)modal.valid=rotate_placement_axis(source,a,axis,angle,modal.pivot);
  else a.position+=delta;
  for(int i=0;i<3;++i)modal.valid=modal.valid&&std::abs(a.position[i])<=1000000.f;
  if(!modal.valid)break;
  modal.preview.push_back(std::move(a));
 }
 if(modal.valid)for(size_t i=0;i<modal.ids.size();++i)doc.placements[modal.ids[i]]=modal.preview[i];
 dirty=modal.wasDirty||modal.valid;
 std::ostringstream text;
 text<<(modal.tool==TransformTool::Rotate?"ПОВОРОТ ":modal.tool==TransformTool::Scale?"МАСШТАБ ":"РУХ ")
     <<(modal.axis<0?(modal.tool==TransformTool::Scale?"XYZ":"ВИД"):modal.axis==0?"X":modal.axis==1?"Y":"Z")
     <<(modal.local?" ЛОКАЛЬНІ ":" ")<<(modal.plane?"ПЛОЩИНА ":"")<<modal.number.text();
 if(!modal.valid)text<<"  (поза допустимим діапазоном)";
 text<<"   Enter / ЛКМ: підтвердити   Esc / ПКМ: скасувати";
 message=text.str();syncFields();status();
}
void finishModal(bool commit){
 if(!modal.active)return;
 if(commit&&!modal.valid){message="Введіть допустиме число або натисніть Esc";status();return;}
 bool changed=false;
 if(commit)for(size_t i=0;i<modal.ids.size();++i){
  const auto& a=doc.placements[modal.ids[i]];const auto& b=modal.starts[i];
  changed=changed||a.position.x!=b.position.x||a.position.y!=b.position.y||a.position.z!=b.position.z||
          a.rotation!=b.rotation||a.scale.x!=b.scale.x||a.scale.y!=b.scale.y||a.scale.z!=b.scale.z;
 }
 if(changed)validate_track_document(doc);
 auto result=std::move(doc);doc=std::move(modal.before);dirty=modal.wasDirty;
 if(changed){
  if(!modal.groupedUndo)snapshot();
  doc=std::move(result);dirty=true;
 }
 modal.active=false;rebuildOverrides();
 if(GetCapture()==viewport)ReleaseCapture();
 message=commit?"Трансформацію підтверджено":"Трансформацію скасовано";
 syncFields();status();
}
void modalKey(WPARAM key){
 if(key==VK_ESCAPE){finishModal(false);return;}
 if(key==VK_RETURN||key==VK_SPACE){finishModal(true);return;}
 if(key=='X'||key=='Y'||key=='Z'){
  const int axis=key=='X'?0:key=='Y'?1:2;
  const bool plane=modal.tool!=TransformTool::Rotate&&(GetAsyncKeyState(VK_SHIFT)&0x8000);
  const bool preferLocal=modal.tool==TransformTool::Scale||axisSpace()==EditorAxisSpace::Local;
  if(modal.axis!=axis||modal.plane!=plane){modal.axis=axis;modal.local=preferLocal;}
  else if(modal.tool!=TransformTool::Scale&&modal.local==preferLocal)modal.local=!preferLocal;
  else{modal.axis=-1;modal.local=false;}
  modal.plane=modal.axis>=0&&plane;
 }else if(key==VK_BACK)modal.number.backspace();
 else if((key>='0'&&key<='9')||(key>=VK_NUMPAD0&&key<=VK_NUMPAD9))
  modal.number.input(char(key>=VK_NUMPAD0?'0'+key-VK_NUMPAD0:key));
 else if(key==VK_OEM_MINUS||key==VK_SUBTRACT)modal.number.input('-');
 else if(key==VK_OEM_PERIOD||key==VK_OEM_COMMA||key==VK_DECIMAL)modal.number.input('.');
 modalTo(modal.mouseLast.x,modal.mouseLast.y);
}
void duplicateSelection(){
 if(!selection())return;
 if(selectionIDs.empty())selectionIDs.insert(selected);
 const auto clones=std::count_if(doc.placements.begin(),doc.placements.end(),[](const auto& p){return !p.replace_native;});
 if(clones+selectionIDs.size()>4096){message="Досягнуто ліміту доданих об’єктів";status();return;}
 std::vector<TrackPlacement> copies;
 for(int id:selectionIDs){copies.push_back(doc.placements[id]);copies.back().replace_native=false;}
 snapshot();clearSelection();
 for(auto& a:copies){
  selected=int(doc.placements.size());doc.placements.push_back(std::move(a));selectionIDs.insert(selected);
 }
 beginTransform(TransformTool::Move,true);modal.groupedUndo=true;
}
void beginViewportOrbit(POINT cursor){
 middleDrag=true;last=cursor;
 // Keep the current viewing ray at the center during the first mouse delta.
 if(selection())orbitDistance=std::clamp(dot(selectionPivot()-eye,forward()),4.f,500.f);
 orbitPivot=eye+forward()*orbitDistance;
}
void updateViewportOrbit(POINT cursor,bool pan){
 if(pan){
  const Vec f=forward(),r=unit(cross({0,1,0},f)),u=cross(f,r);
  const float factor=orbitDistance*2*std::tan(.525f)/std::max(1,render->height);
  const Vec offset=r*(float(last.x-cursor.x)*factor)+u*(float(cursor.y-last.y)*factor);
  eye+=offset;orbitPivot+=offset;
 }else{
  yaw+=(cursor.x-last.x)*.005f;
  pitch=std::clamp(pitch-(cursor.y-last.y)*.005f,-1.5f,1.5f);
  eye=orbitPivot-forward()*orbitDistance;
 }
}
void dollyViewport(float delta){
 const float next=std::clamp(orbitDistance*std::pow(.84f,delta),1.f,5000.f);
 eye+=forward()*(orbitDistance-next);orbitDistance=next;
}
void frameSelection(){
 if(!selection())return;
 Vec center=selectionPivot();float radius=1;
 for(int id:selectionIDs){
  const auto& a=doc.placements[id];const auto& p=geometry.props[a.source_prop];
  const Vec half{p.half.x*a.scale.x,p.half.y*a.scale.y,p.half.z*a.scale.z};
  radius=std::max(radius,length(pivot_world(p,a,pivotMode())-center)+length(half));
 }
 orbitDistance=std::max(4.f,radius*3);orbitPivot=center;
 eye=center-forward()*orbitDistance;hoverDirty=true;
}
