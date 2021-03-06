/**
* Project: VSXu Engine: Realtime modular visual programming engine.
*
* This file is part of Vovoid VSXu Engine.
*
* @author Jonatan Wallmander, Robert Wenzel, Vovoid Media Technologies AB Copyright (C) 2003-2013
* @see The GNU Lesser General Public License (LGPL)
*
* VSXu Engine is free software; you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
* for more details.
*
* You should have received a copy of the GNU Lesser General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#ifndef VSX_DEMO_MINI
    // ***************************************
    // Set time loop point
    // ***************************************
    // 0=seq_pool 1=time_set_loop_point 2=[time:float]
    if (cmd == "time_set_loop_point")
    {
      loop_point_end = s2f(c->parts[1]);
    } else

    if (cmd == "play") {
      time_play();
    } else
    if (cmd == "stop") {
      current_state = VSX_ENGINE_STOPPED;
    } else
    if (cmd == "rewind") {
      current_state = VSX_ENGINE_REWIND;
    } else
#ifndef VSXU_NO_CLIENT
    if (cmd == "fps_d" || cmd == "fps") {
      cmd_out->add("fps_d",f2s(frame_delta_fps));
    }
    else
    if (cmd == "time_set") {
      float dd = engine_info.vtime - s2f(c->parts[1]);
      //engine_info.vtime = s2f(c->parts[1]);
      if (dd > 0) {
        engine_info.dtime = -dd;
      } else {
        engine_info.dtime = fabs(dd);
      }
    } else
#endif
#endif
