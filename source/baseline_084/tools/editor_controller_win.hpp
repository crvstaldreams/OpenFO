#pragma once
// Native host, modal Blender-style object tools and per-stage v3 overrides.
namespace {
enum {ID_STAGE=101,ID_PREFAB,ID_MODE,ID_SAVE,ID_LOAD,ID_UNDO,ID_CLEAR,ID_MAP,ID_DELETE,ID_ROTLEFT,ID_ROTRIGHT,ID_MOVE,ID_CLOSED,ID_HEIGHTUP,ID_HEIGHTDOWN,ID_STATUS,ID_SEARCH,ID_TOOL,ID_PIVOT,ID_SPACE,ID_SNAP,ID_X,ID_Y,ID_Z,ID_YAW,ID_APPLY,ID_PREVIEW_RESET,ID_SAVE_AS,ID_PROJECT_OPEN,ID_PROJECT_PLAY,ID_GLOBAL_PROPS};
struct Editor {
 static constexpr int panelWidth=340;
 int sidebarScroll=0,sidebarHeight=1350;std::vector<HWND>sidebarControls;
 HWND window=nullptr,viewport=nullptr,stageBox=nullptr,prefabBox=nullptr,modeBox=nullptr,statusBox=nullptr,searchBox=nullptr,toolBox=nullptr,pivotBox=nullptr,spaceBox=nullptr,snapBox=nullptr,caption=nullptr,statsBox=nullptr;
 std::array<HWND,4> fields{};HFONT font=nullptr,titleFont=nullptr;HBRUSH panelBrush=CreateSolidBrush(RGB(27,30,35)),inputBrush=CreateSolidBrush(RGB(39,43,50));
 fs::path output=exe_dir();std::unique_ptr<Resources>resources;std::unique_ptr<EditorRender>render;std::unique_ptr<EditorSceneQuery>query;
 Geometry geometry;std::vector<TrackTemplate>templates;std::vector<size_t>filtered;std::vector<std::string>stages;TrackDocument doc;std::vector<TrackDocument>history;
 int selected=-1;bool renderFailed=false;bool moveNext=false,dirty=false,rightDrag=false,previewDrag=false,hoverDirty=true,hoverValid=false;POINT last{};Vec eye{0,35,-40},hover{};float yaw=0,pitch=-.4f,previewYaw=0,previewOrbit=.6f,previewPitch=.15f,previewZoom=1;std::string message="Готово";ULONGLONG statsTime=0;int overlayWidth=0,overlayHeight=0;
 struct Drag {bool active=false,rotation=false,wasDirty=false;int axis=0;TrackPlacement start;Vec pivot{},direction{};float initial=0;};Drag drag;
#include "editor_modal_win.inc"
 static constexpr const wchar_t* project_player_exe=L"OpenFO_D3D9.exe";
#include "editor_tracks_win.inc"
 ~Editor(){if(font)DeleteObject(font);if(titleFont)DeleteObject(titleFont);DeleteObject(panelBrush);DeleteObject(inputBrush);}
 Vec forward()const{return {std::sin(yaw)*std::cos(pitch),std::sin(pitch),std::cos(yaw)*std::cos(pitch)};}
 int choice(HWND c)const{auto i=SendMessageW(c,CB_GETCURSEL,0,0);return i<0?0:int(i);}
 int mode()const{return choice(modeBox);}size_t prefab()const{return filtered.empty()?0:filtered[std::min(size_t(choice(prefabBox)),filtered.size()-1)];}
 EditorPivot pivotMode()const{return EditorPivot(choice(pivotBox));}EditorAxisSpace axisSpace()const{return EditorAxisSpace(choice(spaceBox));}
 float snapStep()const{const float steps[]={0,.25f,1,5};return steps[std::clamp(choice(snapBox),0,3)];}
 bool selection()const{return selected>=0&&size_t(selected)<doc.placements.size();}
 HWND control(const wchar_t*klass,const wchar_t*text,DWORD style,int id,int x,int y,int w,int h){HWND c=CreateWindowW(klass,text,WS_CHILD|WS_VISIBLE|style,x,y,w,h,window,(HMENU)(INT_PTR)id,GetModuleHandleW(nullptr),nullptr);SendMessageW(c,WM_SETFONT,(WPARAM)font,TRUE);sidebarControls.push_back(c);return c;}
 HWND combo(int id,int y,std::initializer_list<const wchar_t*>items){auto c=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,id,16,y,306,300);for(auto s:items)SendMessageW(c,CB_ADDSTRING,0,(LPARAM)s);if(items.size())SendMessageW(c,CB_SETCURSEL,0,0);return c;}
 void label(const wchar_t*t,int y){control(L"STATIC",t,0,0,16,y,306,22);}
 void section(const wchar_t*t,int y){auto c=control(L"STATIC",t,0,0,16,y,306,24);SendMessageW(c,WM_SETFONT,(WPARAM)titleFont,TRUE);}
 void button(const wchar_t*t,int id,int y,int x=16,int width=306){control(L"BUTTON",t,BS_OWNERDRAW|WS_TABSTOP,id,x,y,width,30);}
 void ui(){
 font=CreateFontW(-15,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");titleFont=CreateFontW(-17,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
 section(L"OPENFO  /  TRACK STUDIO",14);label(L"Траса",46);stageBox=combo(ID_STAGE,70,{});
 section(L"01  /  PICKER",110);label(L"Оригінальні динамічні моделі",140);
 searchBox=control(L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,ID_SEARCH,16,166,306,26);SendMessageW(searchBox,EM_SETCUEBANNER,TRUE,(LPARAM)L"Пошук моделі…");prefabBox=combo(ID_PREFAB,202,{});button(L"Скинути 3D-передперегляд",ID_PREVIEW_RESET,238);
 modeBox=combo(ID_MODE,282,{L"Додати об'єкт",L"Вибрати об'єкт",L"Малювати маршрут"});
 section(L"02  /  TRANSFORM",326);toolBox=combo(ID_TOOL,358,{L"G  /  Переміщення XYZ",L"R  /  Поворот XYZ",L"S  /  Масштаб XYZ"});pivotBox=combo(ID_PIVOT,394,{L"Pivot: початок моделі",L"Pivot: центр габаритів",L"Pivot: низ габаритів"});spaceBox=combo(ID_SPACE,430,{L"Осі: світові",L"Осі: локальні"});snapBox=combo(ID_SNAP,466,{L"Прив'язка: вимкнена",L"Прив'язка: 0.25 м / 15°",L"Прив'язка: 1 м / 15°",L"Прив'язка: 5 м / 15°"});
 label(L"Координати pivot у світі  /  Yaw (°)",506);
 const wchar_t* names[]={L"X",L"Y",L"Z",L"Yaw"};for(int i=0;i<4;i++){control(L"STATIC",names[i],0,0,16+i*78,536,72,20);fields[i]=control(L"EDIT",L"0",WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,ID_X+i,16+i*78,558,72,26);}button(L"Застосувати XYZ / Yaw",ID_APPLY,596);
 button(L"↶ −15°",ID_ROTLEFT,634,16,148);button(L"↷ +15°",ID_ROTRIGHT,634,174,148);button(L"Вище +0.25",ID_HEIGHTUP,672,16,148);button(L"Нижче −0.25",ID_HEIGHTDOWN,672,174,148);button(L"Перенести на поверхню: наступний клік",ID_MOVE,710);button(L"Скинути рідний / видалити клон",ID_DELETE,748);
 section(L"03  /  ROUTE",794);control(L"BUTTON",L"Замкнений маршрут",BS_AUTOCHECKBOX|WS_TABSTOP,ID_CLOSED,16,826,306,26);CheckDlgButton(window,ID_CLOSED,BST_CHECKED);button(L"Очистити маршрут",ID_CLEAR,862);button(L"Створити мінікарту + зберегти",ID_MAP,900);
 section(L"04  /  PROJECT",944);button(L"Зберегти  /  Ctrl+S",ID_SAVE,976,16,148);button(L"Зберегти як…",ID_SAVE_AS,976,174,148);
 button(L"Відкрити…",ID_PROJECT_OPEN,1014,16,148);button(L"Тест траси",ID_PROJECT_PLAY,1014,174,148);
 button(L"Скасувати  /  Ctrl+Z",ID_UNDO,1052,16,148);button(L"Об’єкти всіх трас",ID_GLOBAL_PROPS,1052,174,148);
 label(L"СКМ: орбіта · Shift+СКМ: панорама",1110);label(L"G/R/S · X/Y/Z · число · Enter / Esc",1136);label(L"Shift+D: дубль · ПКМ+WASD: огляд",1162);statusBox=control(L"STATIC",L"",SS_LEFT,ID_STATUS,16,1198,306,130);
 viewport=CreateWindowW(L"OpenFOEditorViewport",L"",WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|WS_CLIPCHILDREN,panelWidth,0,980,900,window,nullptr,GetModuleHandleW(nullptr),this);
 caption=CreateWindowW(L"STATIC",L"3D PICKER  /  обертання + масштаб",WS_CHILD|WS_VISIBLE|SS_CENTER,0,0,250,24,viewport,nullptr,GetModuleHandleW(nullptr),nullptr);statsBox=CreateWindowW(L"STATIC",L"",WS_CHILD|WS_VISIBLE,8,0,400,24,viewport,nullptr,GetModuleHandleW(nullptr),nullptr);for(HWND c:{caption,statsBox})SendMessageW(c,WM_SETFONT,(WPARAM)font,TRUE);
 }
 void scrollSidebar(int requested){RECT client;GetClientRect(window,&client);int next=std::clamp(requested,0,std::max(0,sidebarHeight-int(client.bottom))),delta=sidebarScroll-next;sidebarScroll=next;if(delta){for(HWND c:sidebarControls){SendMessageW(c,CB_SHOWDROPDOWN,FALSE,0);RECT r;GetWindowRect(c,&r);POINT p{r.left,r.top};ScreenToClient(window,&p);SetWindowPos(c,nullptr,p.x,p.y+delta,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);}RECT p{0,0,panelWidth,client.bottom};InvalidateRect(window,&p,TRUE);}SCROLLINFO si{sizeof(si),SIF_RANGE|SIF_PAGE|SIF_POS};si.nMax=sidebarHeight-1;si.nPage=UINT(std::max(1L,client.bottom));si.nPos=sidebarScroll;SetScrollInfo(window,SB_VERT,&si,TRUE);}
 void scrollCommand(WPARAM w){SCROLLINFO si{sizeof(si),SIF_ALL};GetScrollInfo(window,SB_VERT,&si);int n=sidebarScroll;switch(LOWORD(w)){case SB_LINEUP:n-=32;break;case SB_LINEDOWN:n+=32;break;case SB_PAGEUP:n-=int(si.nPage)-32;break;case SB_PAGEDOWN:n+=int(si.nPage)-32;break;case SB_THUMBTRACK:case SB_THUMBPOSITION:n=si.nTrackPos;break;case SB_TOP:n=0;break;case SB_BOTTOM:n=sidebarHeight;break;}scrollSidebar(n);}
 void resize(){RECT c;GetClientRect(window,&c);scrollSidebar(sidebarScroll);GetClientRect(window,&c);if(viewport)MoveWindow(viewport,panelWidth,0,std::max(1,int(c.right)-panelWidth),std::max(1,int(c.bottom)),TRUE);hoverDirty=true;}
 void snapshot(){history.push_back(doc);if(history.size()>100)history.erase(history.begin());dirty=true;}
 void status(){std::ostringstream s;s<<message<<"\n"<<(dirty?"* ":"")<<"Objects: "<<doc.placements.size()<<"  Route: "<<doc.route.size()<<"\nSelected: "<<(selection()?std::to_string(selected+1):"none")<<" ("<<selectionIDs.size()<<")  Models: "<<filtered.size()<<"/"<<templates.size();if(templates.empty())s<<"\nУ цій сцені немає динамічних шаблонів.";SetWindowTextW(statusBox,wide(s.str()).c_str());}
 void syncFields(){if(!selection())return;auto&p=doc.placements[selected];Vec pivot=pivot_world(geometry.props[p.source_prop],p,pivotMode());float v[]={pivot.x,pivot.y,pivot.z,std::atan2(direction(multiply(p.rotation,rotate_y(p.yaw)),{0,0,1}).x,direction(multiply(p.rotation,rotate_y(p.yaw)),{0,0,1}).z)*57.2957795f};for(int i=0;i<4;i++){wchar_t t[64];swprintf(t,64,L"%.3f",double(v[i]));SetWindowTextW(fields[i],t);}}
 void filter(){size_t old=filtered.empty()?SIZE_MAX:prefab();wchar_t text[256];GetWindowTextW(searchBox,text,256);std::wstring term=text;for(auto&c:term)c=std::towlower(c);filtered.clear();SendMessageW(prefabBox,CB_RESETCONTENT,0,0);int selectedItem=0;for(size_t i=0;i<templates.size();i++){auto name=wide(templates[i].name),key=name;for(auto&c:key)c=std::towlower(c);if(key.find(term)==std::wstring::npos)continue;if(i==old)selectedItem=int(filtered.size());filtered.push_back(i);SendMessageW(prefabBox,CB_ADDSTRING,0,(LPARAM)name.c_str());}if(!filtered.empty())SendMessageW(prefabBox,CB_SETCURSEL,selectedItem,0);EnableWindow(prefabBox,!filtered.empty());message=filtered.empty()?"Моделей за цим запитом немає":"Picker готовий";status();}
 void validateLoaded(const TrackDocument&d,const Geometry&g){validate_track_document(d);for(const auto&p:d.placements){if(p.source_prop>=g.props.size())throw std::runtime_error("Saved track references unavailable native prop");bool captured=false;for(const auto&m:g.meshes)if(m.prop_index==int(p.source_prop)&&m.captured)captured=true;if(!captured)throw std::runtime_error("Saved track references missing captured prefab");}}
 void home(){auto spawn=stage_spawn(geometry,doc.stage);eye=point(spawn,{0,20,-30});auto f=direction(spawn,{0,0,1});yaw=std::atan2(f.x,f.z);pitch=-.4f;hoverDirty=true;}
 void loadStage(size_t i){if(i>=stages.size())return;finishDrag(false);SetWindowTextW(statusBox,L"Завантаження оригінальної геометрії…");auto next=load_geometry(*resources,stage_geometry(stages[i]),stages[i]);append_captured_scene(next,*resources);TrackDocument nd;nd.stage=stages[i];std::string sidecarError;try{if(opening_project)nd=*opening_project;else if(auto x=load_track_document(output,nd.stage))nd=*x;bind_track_document(nd,next,*resources);validateLoaded(nd,next);}catch(const std::exception&e){if(opening_project)throw;sidecarError=e.what();auto file=track_edit_path(output,stages[i]);try{if(fs::exists(file)){auto backup=file;backup+=std::string(".recovery_")+std::to_string(GetTickCount64())+".json";fs::copy_file(file,backup);}}catch(...){}nd=TrackDocument{};nd.stage=stages[i];}auto nq=std::make_unique<EditorSceneQuery>(next);auto nt=asset_library?asset_library->templates(next):track_templates(next);for(auto& t:nt)if(!t.source_geometry.empty())t.source_prop=SIZE_MAX;auto spawn=stage_spawn(next,nd.stage);auto nextEye=point(spawn,{0,20,-30});auto f=direction(spawn,{0,0,1});auto nr=std::make_unique<EditorRender>(viewport,*resources,render.get());nr->upload(next,nd.stage);geometry=std::move(next);query=std::move(nq);templates=std::move(nt);doc=std::move(nd);if(!opening_project)project_path.clear();render.swap(nr);renderFailed=false;history.clear();clearSelection();rebuildOverrides();dirty=false;moveNext=false;filter();CheckDlgButton(window,ID_CLOSED,doc.closed?BST_CHECKED:BST_UNCHECKED);eye=nextEye;yaw=std::atan2(f.x,f.z);pitch=-.4f;hoverDirty=true;message=sidecarError.empty()?"Траса завантажена":std::string("Старий sidecar пропущено: ")+sidecarError;status();try{write_file_atomic(output/"editor_status.json",Json{{"stage",doc.stage},{"templates",templates.size()},{"renderer",render->performanceJson()},{"ground_triangles",query->triangle_count()},{"missing_textures",render->missing},{"native_shaders",render->native.report}}.dump(2));}catch(...){message="Траса готова; журнал недоступний для запису";status();}}

 std::optional<EditorRay> screenRay(int x,int y)const{return render?editor_screen_ray(float(x),float(y),float(render->width),float(render->height),eye,forward()):std::nullopt;}
 bool ground(int x,int y,Vec&hit){auto ray=screenRay(x,y);if(!query||!ray)return false;auto h=query->ray(ray->origin,ray->direction,5000);if(!h)return false;hit=*h;return true;}
 Vec snap(size_t prop,Vec p,float angle){if(prop>=geometry.props.size())return p;TrackPlacement placed{prop,{},angle};float bottom=1e30f;for(auto i:render->prop_meshes(prop)){auto matrix=placement_mesh_matrix(geometry.props[prop],placed,geometry.meshes[i].transform);for(const auto&v:geometry.meshes[i].vertices)bottom=std::min(bottom,point(matrix,v.position).y);}if(bottom!=1e30f)p.y-=bottom;return p;}
 RECT previewRect()const{int w=std::min(270,std::max(150,render->width/3)),h=std::min(210,std::max(130,render->height/3));return {std::max(8,render->width-w-12),38,std::max(8,render->width-w-12)+w,38+h};}
 bool inPreview(int x,int y)const{if(!render||filtered.empty())return false;auto r=previewRect();return x>=r.left&&x<r.right&&y>=r.top&&y<r.bottom;}
 Mat vp()const{return multiply(render->projection,render->view);}
 float gizmoSize(Vec pivot)const{return std::max(.25f,length(pivot-eye)*2*std::tan(.525f)*90/std::max(1,render->height));}
 bool beginGizmo(int x,int y){
  if(!selection()||mode()!=1)return false;
  const auto ray=screenRay(x,y);
  if(!ray)return false;
  const auto& placement=doc.placements[selected];const auto& prop=geometry.props[placement.source_prop];
  const auto tool=TransformTool(choice(toolBox));
  const auto space=tool==TransformTool::Scale?EditorAxisSpace::Local:axisSpace();
  const Vec origin=selectionPivot();const float size=gizmoSize(origin);
  int axis=-1;float best=1e30f;
  if(tool!=TransformTool::Rotate){
   const auto screen=project_editor_point(origin,vp(),float(render->width),float(render->height));
   if(!screen)return false;
   if(std::hypot(screen->x-x,screen->y-y)<8){beginTransform(tool,false);return true;}
   for(int i=0;i<3;++i){
    const auto normal=editor_axis(prop,placement,i,space);
    const auto u=editor_axis(prop,placement,(i+1)%3,space),v=editor_axis(prop,placement,(i+2)%3,space);
    if(const auto hit=ray_plane(*ray,origin,normal)){
     const auto delta=(*hit-origin)/size;const float a=dot(delta,u),b=dot(delta,v);
     if(a>.18f&&a<.36f&&b>.18f&&b<.36f){
      beginTransform(tool,false,i);modal.plane=true;return true;
     }
    }
   }
   for(int i=0;i<3;++i){
    const auto direction=editor_axis(prop,placement,i,space);
    const auto tip=project_editor_point(origin+direction*size,vp(),float(render->width),float(render->height));
    if(tip&&hit_axis_segment({float(x),float(y),0},*screen,*tip,9)){
     const float distance=std::hypot(tip->x-x,tip->y-y);
     if(distance<best){best=distance;axis=i;}
    }
   }
  }else{
   const auto viewHit=ray_plane(*ray,origin,forward());
   if(viewHit&&std::abs(length(*viewHit-origin)-size*1.12f)<size*.08f){beginTransform(tool,false);return true;}
   for(int i=0;i<3;++i){
    if(const auto hit=ray_plane(*ray,origin,editor_axis(prop,placement,i,space))){
     const float distance=std::abs(length(*hit-origin)-size);
     if(distance<size*.12f&&distance<best){best=distance;axis=i;}
    }
   }
  }
  if(axis<0)return false;
  beginTransform(tool,false,axis);return true;
 }
 void dragTo(int x,int y){modalTo(x,y);}
 void finishDrag(bool commit){finishModal(commit);}
 void click(int x,int y){if(mode()==1&&!moveNext){selectObject(pick(x,y),(GetAsyncKeyState(VK_SHIFT)&0x8000)!=0);message=selection()?"Вибрано: G — рух, R — поворот, S — масштаб":"Об'єкт не знайдено";syncFields();status();return;}
 Vec p;if(!ground(x,y,p)){message="Клік не перетнув поверхню траси";status();return;}if(mode()==2){if(doc.route.size()>=8192)return;snapshot();doc.route.push_back(p);message="Точку маршруту додано";}else if(moveNext){if(selection()){snapshot();auto&v=doc.placements[selected];v.position=snap(v.source_prop,p,v.yaw);syncFields();}moveNext=false;}else if(mode()==0&&!filtered.empty()&&std::count_if(doc.placements.begin(),doc.placements.end(),[](const auto&p){return !p.replace_native;})<4096){auto prop=prefab_prop();snapshot();doc.placements.push_back({prop,snap(prop,p,previewYaw),previewYaw});selectObject(int(doc.placements.size()-1),false);syncFields();message="Об'єкт додано";}status();}
 void save(bool map){finishDrag(true);if(modal.active)return;bind_track_document(doc,geometry,*resources);if(project_path.empty()){
  save_track_document(output,doc);
  if(map)save_track_minimap(output,doc,512);
 }else{
  save_track_project(project_path,doc);
  if(map){
   auto image=make_track_minimap(doc,512);
   auto base=project_path.parent_path()/(project_path.stem().wstring()+L"_minimap");
   auto file=base;file+=L".bmp";write_file_atomic(file,std::string(image.bmp.begin(),image.bmp.end()));
   file=base;file+=L".tga";write_file_atomic(file,std::string(image.tga.begin(),image.tga.end()));
   file=base;file+=L".json";write_file_atomic(file,image.metadata.dump(2));
  }
 }dirty=false;message=map?"Збережено трасу та мінікарту":"Трасу збережено";status();}
 void applyFields(){if(!selection())return;float v[4];for(int i=0;i<4;i++){wchar_t text[128],*end;GetWindowTextW(fields[i],text,128);v[i]=wcstof(text,&end);bool converted=end!=text;while(iswspace(*end))++end;if(!converted||*end||!std::isfinite(v[i])||std::abs(v[i])>1000000)throw std::runtime_error("Enter finite numeric XYZ / Yaw values");}auto next=doc.placements[selected];auto&prop=geometry.props[next.source_prop];Vec dir=direction(multiply(next.rotation,rotate_y(next.yaw)),{0,0,1});rotate_placement_about_pivot(prop,next,pivotMode(),v[3]*.0174532925f-std::atan2(dir.x,dir.z));next.position+=Vec{v[0],v[1],v[2]}-pivot_world(prop,next,pivotMode());auto candidate=doc;candidate.placements[selected]=next;validate_track_document(candidate);snapshot();doc.placements[selected]=next;syncFields();}
 void command(int id){finishDrag(true);if(modal.active)return;switch(id){case ID_SAVE:save(false);break;
 case ID_SAVE_AS:save_as();break;
 case ID_PROJECT_OPEN:open_project(project_dialog(false));break;
 case ID_PROJECT_PLAY:play_project();break;
 case ID_GLOBAL_PROPS:
  if(!asset_library){
   SetWindowTextW(statusBox,L"Індексація динамічних об’єктів усіх трас…");
   auto library=std::make_unique<TrackAssetLibrary>(*resources);
   auto entries=library->templates(geometry);
   for(auto& t:entries)if(!t.source_geometry.empty())t.source_prop=SIZE_MAX;
   asset_library=std::move(library);templates=std::move(entries);filter();
  }
  SetFocus(searchBox);break;case ID_MAP:save(true);break;case ID_APPLY:applyFields();break;case ID_PREVIEW_RESET:previewOrbit=.6f;previewPitch=.15f;previewZoom=1;break;case ID_LOAD:if(auto x=load_track_document(output,doc.stage)){bind_track_document(*x,geometry,*resources);validateLoaded(*x,geometry);snapshot();doc=*x;clearSelection();rebuildOverrides();dirty=false;CheckDlgButton(window,ID_CLOSED,doc.closed?BST_CHECKED:BST_UNCHECKED);message="Завантажено";}break;case ID_UNDO:if(!history.empty()){doc=history.back();history.pop_back();clearSelection();rebuildOverrides();dirty=true;CheckDlgButton(window,ID_CLOSED,doc.closed?BST_CHECKED:BST_UNCHECKED);}break;case ID_CLEAR:snapshot();doc.route.clear();doc.start_index=0;break;case ID_CLOSED:snapshot();doc.closed=IsDlgButtonChecked(window,ID_CLOSED)==BST_CHECKED;break;case ID_DELETE:if(selection()){snapshot();std::vector<TrackPlacement>keep;for(size_t i=0;i<doc.placements.size();i++)if(!selectionIDs.count(int(i)))keep.push_back(doc.placements[i]);doc.placements=std::move(keep);clearSelection();rebuildOverrides();}break;case ID_MOVE:moveNext=selection();if(moveNext)SendMessageW(modeBox,CB_SETCURSEL,1,0);message=moveNext?"Клікніть нове місце":"Спочатку виберіть доданий об'єкт";break;case ID_ROTLEFT:case ID_ROTRIGHT:{float a=(id==ID_ROTLEFT?-1:1)*.2617994f;if(selection()){snapshot();auto&p=doc.placements[selected];rotate_placement_about_pivot(geometry.props[p.source_prop],p,pivotMode(),a);}else previewYaw+=a;break;}case ID_HEIGHTUP:case ID_HEIGHTDOWN:if(selection()){snapshot();doc.placements[selected].position.y+=id==ID_HEIGHTUP?.25f:-.25f;}break;}syncFields();status();}
 void placement(const TrackPlacement&p,bool ghost){if(p.source_prop>=geometry.props.size())return;for(auto i:render->prop_meshes(p.source_prop))render->mesh(geometry,i,placement_mesh_matrix(geometry.props[p.source_prop],p,geometry.meshes[i].transform),ghost);}
 void objectOutline(const TrackPlacement&p){const auto&prop=geometry.props[p.source_prop];auto matrix=placement_matrix(prop,p);Vec corners[8];for(int i=0;i<8;i++)corners[i]=point(matrix,prop.center+Vec{prop.half.x*(i&1?1.f:-1.f),prop.half.y*(i&2?1.f:-1.f),prop.half.z*(i&4?1.f:-1.f)});std::vector<Vec>line;for(int i:{0,1,3,2,0,4,5,1,5,7,3,7,6,2,6,4})line.push_back(corners[i]);render->lines(line,0xffffa040,false,0);}
 void gizmo(){
  if(!selection()||mode()!=1)return;
  struct Restore {EditorRender& r;~Restore(){r.state.SetRenderState(D3DRS_ZENABLE,TRUE);}} restore{*render};
  render->state.SetRenderState(D3DRS_ZENABLE,FALSE);
  const auto& placement=doc.placements[selected];
  const auto& prop=geometry.props[placement.source_prop];
  const auto tool=TransformTool(choice(toolBox));
  const auto space=tool==TransformTool::Scale?EditorAxisSpace::Local:axisSpace();
  const Vec origin=modal.active?modal.pivot:selectionPivot();
  const float size=gizmoSize(origin);
  const Vec viewRight=unit(cross({0,1,0},forward())),viewUp=cross(forward(),viewRight);
  const auto axis=[&](int i){return modal.active&&modal.axis>=0?modalAxis(i):editor_axis(prop,placement,i,space);};
  const auto ring=[&](Vec u,Vec v,float radius,DWORD color){
   std::vector<Vec> points;points.reserve(49);
   for(int k=0;k<=48;++k){
    const float a=k*6.2831853f/48;
    points.push_back(origin+(u*std::cos(a)+v*std::sin(a))*radius);
   }
   render->lines(points,color,false,0);
  };
  ring(viewRight,viewUp,size*(tool==TransformTool::Rotate?1.12f:.075f),0xffeeeeee);
  constexpr DWORD colors[]={0xffff5656,0xff62db86,0xff599fff};
  for(int i=0;i<3;++i){
   const Vec d=axis(i);
   const DWORD color=modal.active&&modal.axis>=0&&(modal.plane?i!=modal.axis:i==modal.axis)?0xffffcc44:colors[i];
   if(tool==TransformTool::Rotate){
    const Vec u=unit(cross(d,std::abs(d.y)<.8f?Vec{0,1,0}:Vec{1,0,0}));
    ring(u,cross(d,u),size,color);
    continue;
   }
   const Vec tip=origin+d*size;
   render->lines({origin,tip},color,false,0);
   if(tool==TransformTool::Scale){
    const Vec h=viewRight*size*.055f,v=viewUp*size*.055f;
    render->lines({tip-h-v,tip+h-v,tip+h+v,tip-h+v},color,true,0);
   }else{
    const Vec side=unit(cross(d,forward()),{0,1,0})*size*.07f;
    render->lines({tip-d*size*.15f+side,tip,tip-d*size*.15f-side},color,false,0);
   }
   const Vec u=axis((i+1)%3),v=axis((i+2)%3);
   render->lines({origin+(u+v)*size*.18f,origin+(u*.36f+v*.18f)*size,origin+(u+v)*size*.36f,
                  origin+(u*.18f+v*.36f)*size},color,true,0);
  }
 }
 void frame(float dt){RECT rc;GetClientRect(viewport,&rc);if(rc.right<1||rc.bottom<1)return;if(render->width!=rc.right||render->height!=rc.bottom){finishDrag(false);render.reset();render=std::make_unique<EditorRender>(viewport,*resources);render->upload(geometry,doc.stage);hoverDirty=true;}
 if(GetForegroundWindow()==window&&GetFocus()==viewport&&!modal.active&&!previewDrag&&rightDrag&&!(GetAsyncKeyState(VK_CONTROL)&0x8000)){float speed=dt*((GetAsyncKeyState(VK_SHIFT)&0x8000)?100.f:25.f);Vec f=forward(),right=unit(cross({0,1,0},f));Vec before=eye;if(GetAsyncKeyState('W')&0x8000)eye+=f*speed;if(GetAsyncKeyState('S')&0x8000)eye+=f*-speed;if(GetAsyncKeyState('A')&0x8000)eye+=right*-speed;if(GetAsyncKeyState('D')&0x8000)eye+=right*speed;if(GetAsyncKeyState('Q')&0x8000)eye.y-=speed;if(GetAsyncKeyState('E')&0x8000)eye.y+=speed;if(length(eye-before)>0)hoverDirty=true;}
 if(hoverDirty){POINT p;GetCursorPos(&p);ScreenToClient(viewport,&p);hoverValid=p.x>=0&&p.y>=0&&p.x<rc.right&&p.y<rc.bottom&&!inPreview(p.x,p.y)&&ground(p.x,p.y,hover);hoverDirty=false;}
 const auto preview_prop=filtered.empty()?SIZE_MAX:prefab_prop();
 render->begin(eye,forward());std::set<size_t> replaced;
 for(const auto& pair:geometry.editor_template_props)replaced.insert(pair.first);for(const auto&p:doc.placements)if(p.replace_native){auto m=placement_matrix(geometry.props[p.source_prop],p);bool changed=false;for(int k=0;k<16;k++)changed=changed||std::abs(m[k]-geometry.props[p.source_prop].transform[k])>1e-5f;if(changed)replaced.insert(p.source_prop);}render->draw_world(geometry,eye,replaced);RenderFrustum frustum(vp(),eye,5000);for(size_t i=0;i<doc.placements.size();i++){auto&p=doc.placements[i];auto&prop=geometry.props[p.source_prop];auto m=placement_matrix(prop,p);float factor=0;for(int c=0;c<3;c++)for(int r=0;r<3;r++)factor+=m[c*4+r]*m[c*4+r];float radius=length(prop.half)*std::sqrt(factor);if(!frustum.visible(point(m,prop.center),radius))continue;if(!p.replace_native||replaced.count(p.source_prop))placement(p,false);if(selectionIDs.count(int(i))&&(selectionIDs.size()<65||int(i)==selected))objectOutline(p);}if(hoverValid&&mode()==0&&!filtered.empty()){auto p=preview_prop;placement({p,snap(p,hover,previewYaw),previewYaw},true);}render->draw_water(geometry,eye);render->lines(doc.route,0xff62db86,doc.closed);for(size_t i=0;i<doc.route.size();i++){auto p=doc.route[i];render->lines({p,p+Vec{0,i==doc.start_index?3.f:1.2f,0}},i==doc.start_index?0xffffcc44:0xff62db86);}if(hoverValid&&mode()!=1){render->lines({hover+Vec{-1.5f,0,0},hover+Vec{1.5f,0,0}},0xffffcc44);render->lines({hover+Vec{0,0,-1.5f},hover+Vec{0,0,1.5f}},0xffffcc44);}gizmo();if(boxActive)render->screen_box(boxStart,boxEnd);if(!filtered.empty())render->preview(geometry,preview_prop,previewRect(),previewOrbit,previewPitch,previewZoom);render->end();
 if(overlayWidth!=render->width||overlayHeight!=render->height){auto r=previewRect();MoveWindow(caption,r.left,10,r.right-r.left,24,TRUE);MoveWindow(statsBox,10,std::max(0,render->height-28),std::max(1,render->width-20),24,TRUE);overlayWidth=render->width;overlayHeight=render->height;}ShowWindow(caption,filtered.empty()?SW_HIDE:SW_SHOW);if(GetTickCount64()-statsTime>300){statsTime=GetTickCount64();std::wostringstream s;s<<(modal.active?wide(message):L"G / R / S · Shift+D · Shift+ЛКМ · B · Numpad .")<<L"   DRAW "<<render->draws<<L"   CULLED "<<render->culled<<L"   BVH "<<(query?query->last_triangle_tests():0)<<L"   |   Ctrl: прив’язка   Esc: скасувати";SetWindowTextW(statsBox,s.str().c_str());}}
 void key(WPARAM k){if(rightDrag&&(k=='W'||k=='A'||k=='S'||k=='D'||k=='Q'||k=='E'))return;if(modal.active){modalKey(k);return;}if(k==VK_ESCAPE){boxArmed=boxActive=false;clearSelection();if(GetCapture()==viewport)ReleaseCapture();}else if(k=='G'||k=='R'||(k=='S'&&!(GetAsyncKeyState(VK_CONTROL)&0x8000)))beginTransform(k=='R'?TransformTool::Rotate:k=='S'?TransformTool::Scale:TransformTool::Move,true);else if(k=='D'&&(GetAsyncKeyState(VK_SHIFT)&0x8000))duplicateSelection();else if(k=='A')selectAll((GetAsyncKeyState(VK_MENU)&0x8000)!=0);else if(k=='B'){boxArmed=true;message="B: протягніть рамку ЛКМ; Shift — додати";}else if(k=='Z'&&(GetAsyncKeyState(VK_MENU)&0x8000)){xraySelect=!xraySelect;message=xraySelect?"Вибір крізь геометрію":"Вибір видимих об'єктів";}else if(k==VK_DECIMAL)frameSelection();else if(k==VK_HOME)home();else if(k==VK_DELETE)command(ID_DELETE);else if(k=='Z'&&(GetAsyncKeyState(VK_CONTROL)&0x8000))command(ID_UNDO);else if(k=='S'&&(GetAsyncKeyState(VK_CONTROL)&0x8000))command(ID_SAVE);else if(k==VK_OEM_4)command(ID_ROTLEFT);else if(k==VK_OEM_6)command(ID_ROTRIGHT);syncFields();status();}

};
}
