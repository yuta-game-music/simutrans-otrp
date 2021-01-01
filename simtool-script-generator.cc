
#include "simtool-script-generator.h"
#include "descriptor/building_desc.h"
#include "descriptor/ground_desc.h"
#include "gui/script_generator_frame.h"
#include "gui/simwin.h"
#include "obj/zeiger.h"
#include "obj/gebaeude.h"
#include "dataobj/koord3d.h"


#define dr_fopen fopen

void tool_generate_script_t::mark_tiles(  player_t *, const koord3d &start, const koord3d &end )
{
	koord k1, k2;
	k1.x = start.x < end.x ? start.x : end.x;
	k1.y = start.y < end.y ? start.y : end.y;
	k2.x = start.x + end.x - k1.x;
	k2.y = start.y + end.y - k1.y;
	koord k;
	for(  k.x = k1.x;  k.x <= k2.x;  k.x++  ) {
		for(  k.y = k1.y;  k.y <= k2.y;  k.y++  ) {
			grund_t *gr = welt->lookup( koord3d(k.x, k.y, start.z) );
			zeiger_t *marker = new zeiger_t(gr->get_pos(), NULL );
      
      const uint8 grund_hang = gr->get_grund_hang();
			const uint8 weg_hang = gr->get_weg_hang();
			const uint8 hang = max( corner_sw(grund_hang), corner_sw(weg_hang)) +
					3 * max( corner_se(grund_hang), corner_se(weg_hang)) +
					9 * max( corner_ne(grund_hang), corner_ne(weg_hang)) +
					27 * max( corner_nw(grund_hang), corner_nw(weg_hang));
			uint8 back_hang = (hang % 3) + 3 * ((uint8)(hang / 9)) + 27;
			marker->set_foreground_image( ground_desc_t::marker->get_image( grund_hang % 27 ) );
			marker->set_image( ground_desc_t::marker->get_image( back_hang ) );
      marker->mark_image_dirty( marker->get_image(), 0 );
			gr->obj_add( marker );
			marked.insert( marker );
		}
	}
}


void write_station_at(cbuffer_t &buf, const koord3d pos, const koord3d origin) {
  grund_t* gr = world()->lookup(pos);
  gebaeude_t* obj = gr ? obj_cast<gebaeude_t>(gr->first_obj()) : NULL;
  const building_desc_t* desc = obj ? obj->get_tile()->get_desc() : NULL;
  if(  !desc  ||  desc->get_type()!=building_desc_t::generic_stop  ) {
    return;
  }
  // now this pos has a stop.
  koord3d diff = pos - origin;
  buf.printf("\thm_station_tl(\"%s\",[%d,%d,%d])\n", desc->get_name(), diff.x, diff.y, diff.z);
}


void write_stations(cbuffer_t &buf, const koord start, const koord end, const koord3d origin) {
  for(sint8 z=-128;  z<127;  z++) { // iterate for all height
    for(sint16 x=start.x;  x<=end.x;  x++) {
      for(sint16 y=start.y;  y<=end.y;  y++) {
        write_station_at(buf, koord3d(x, y, z), origin);
      }
    }
  }
}


char const* tool_generate_script_t::do_work(player_t* , const koord3d &start, const koord3d &end) {
  koord3d e = end==koord3d::invalid ? start : end;
  koord k1 = koord(min(start.x, e.x), min(start.y, e.y));
  koord k2 = koord(max(start.x, e.x), max(start.y, e.y));
  buf.clear();
  buf.append("include(\"hm_toolkit_v1\")\n\nfunction hm_build() {\n"); // header
  
  write_stations(buf, k1, k2, start);
  
  buf.append("}\n"); // footer
  create_win(new script_generator_frame_t(this), w_info, magic_script_generator);
  return NULL;
}


bool tool_generate_script_t::save_script(const char* fullpath) const {
  FILE* file;
  file = dr_fopen(fullpath, "w");
  printf("save at %s\n", fullpath);
  if(  file==NULL  ) {
    printf("cannot open the file %s\n", fullpath);
    return false;
  }
  fprintf(file, "%s", buf.get_str());
  fclose(file);
  return true;
}
