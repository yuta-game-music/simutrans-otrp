
#include "simtool-script-generator.h"
#include "descriptor/ground_desc.h"
#include "gui/script_generator_frame.h"
#include "gui/simwin.h"
#include "obj/zeiger.h"
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


char const* tool_generate_script_t::do_work(player_t* , const koord3d &start, const koord3d &end) {
  printf("start: %s ", start.get_str());
  printf("end: %s\n", end.get_str());
  buf.clear();
  buf.append("include(\"hm_toolkit_v1\")\n\nfunction hm_build() {\n"); // header
  
  buf.append("print(\"Hello World!\")\n"); // sample
  
  buf.append("}\n"); // footer
  create_win(new script_generator_frame_t(this), w_info, magic_save_t);
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
  
  // write default offset
  fprintf(file, "%s", buf.get_str());
  fclose(file);
  return true;
}
