 fs::path project_path,data_directory;
 std::optional<TrackDocument> opening_project;
 std::unique_ptr<TrackAssetLibrary> asset_library;
 size_t prefab_prop(){
  auto& entry=templates[prefab()];
  if(entry.source_geometry.empty()||entry.source_prop!=SIZE_MAX)return entry.source_prop;
  const auto previous=geometry.meshes.size();
  auto index=asset_library->materialize(geometry,entry);
  if(geometry.meshes.size()!=previous)render->upload(geometry,doc.stage);
  entry.source_prop=index;
  return index;
 }
 Mat stage_spawn(const Geometry& g,const std::string& stage){
  return alpha_track_path(stage)?alpha_track_spawn(*resources,stage,g):load_spawn(*resources,stage);
 }
 fs::path project_dialog(bool saving){
  const auto dir=output/"track_projects";
  fs::create_directories(dir);
  std::array<wchar_t,32768> buffer{};
  if(saving){
   const auto name=project_path.empty()?L"New_track.oftrack":project_path.filename().wstring();
   if(name.size()<buffer.size())std::copy(name.begin(),name.end(),buffer.begin());
  }
  OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=window;
  dialog.lpstrFilter=L"OpenFO tracks (*.oftrack)\0*.oftrack\0\0";
  dialog.lpstrFile=buffer.data();dialog.nMaxFile=DWORD(buffer.size());
  dialog.lpstrInitialDir=dir.c_str();dialog.lpstrDefExt=L"oftrack";
  dialog.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|(saving?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);
  if(saving?GetSaveFileNameW(&dialog):GetOpenFileNameW(&dialog))return fs::path(buffer.data());
  if(CommDlgExtendedError())throw std::runtime_error("Cannot open track project dialog");
  return {};
 }
 bool save_as(){
  auto path=project_dialog(true);
  if(path.empty())return false;
  bind_track_document(doc,geometry,*resources);
  auto copy=doc;copy.name=path.stem().u8string();
  save_track_project(path,copy);
  doc=std::move(copy);project_path=std::move(path);dirty=false;
  message="Трасу збережено окремо";status();
  return true;
 }
 void open_project(fs::path path){
  if(path.empty())return;
  if(dirty){
   const auto answer=MessageBoxW(window,L"Зберегти поточну трасу?",L"OpenFO Track Studio",MB_YESNOCANCEL|MB_ICONQUESTION);
   if(answer==IDCANCEL)return;
   if(answer==IDYES)save(false);
  }
  auto next=load_track_project(path);
  auto found=std::find(stages.begin(),stages.end(),next.stage);
  if(found==stages.end())throw std::runtime_error("Base stage of this track project is unavailable");
  opening_project=std::move(next);
  try{loadStage(size_t(found-stages.begin()));}
  catch(...){opening_project.reset();throw;}
  opening_project.reset();
  project_path=std::move(path);
  SendMessageW(stageBox,CB_SETCURSEL,found-stages.begin(),0);
  message="Відкрито окрему трасу";status();
 }
 void play_project(){
  if(project_path.empty()){if(!save_as())return;}
  else if(dirty)save(false);
  const auto exe=output/project_player_exe;
  const auto args=L"--data \""+data_directory.wstring()+L"\" --track-project \""+project_path.wstring()+L"\"";
  if(INT_PTR(ShellExecuteW(window,L"open",exe.c_str(),args.c_str(),output.c_str(),SW_SHOWNORMAL))<=32)
   throw std::runtime_error("Cannot launch OpenFO for this track project");
 }
