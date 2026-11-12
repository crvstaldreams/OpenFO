#pragma once
namespace ofo {
// Omit every player pane in the seated cockpit, including fracture/dirt/tint.
// Exterior photo, countdown and ejected-driver cameras retain the authored glass.
inline bool draw_player_glass(int camera,bool photo,bool cinematic,bool ejected){
 return camera!=2||photo||cinematic||ejected;
}
}
