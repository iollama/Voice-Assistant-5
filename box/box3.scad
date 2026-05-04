/* ==========================================================================
   DOCUMENTATION & BILL OF MATERIALS (CIRCULAR EDITION - REV 36)
   ==========================================================================
   1. RING: 145mm OD. Internal ledges at top AND bottom.
   2. TRAY: Sits inside bottom ledge. Holds viewports & components facing floor.
   3. LID: Sits inside top ledge. 
   4. LAYOUT: Spaced grid at x=100 to clear cantilever tongue projection.
   ========================================================================== */

/* ==========================================
   1. CONFIGURABLE PARAMETERS & COORDINATES
   ========================================= */

// --- INLINE BUTTON FRAME ---
inline_btn_body   = 12.0;   // 12x12 button body size
inline_btn_clr    = 0.4;    // fit clearance each side
inline_btn_act_d  = 7.0;    // actuator stem hole diameter
inline_btn_h      = 4.0;    // frame height
inline_btn_top_t  = 1.2;    // top wall thickness
inline_btn_frame_t  = 1.5;    // frame wall thickness
inline_btn_ear_ext = 8.0;  // how far each ear extends from the wall face
inline_btn_ear_w   = 10.0;   // ear width (along the wall)
inline_btn_ear_t   = 2.5;   // ear thickness
inline_btn_ear_screw_d = 3.4; // r=1.7 clearance hole
inline_btn_bracket_w  = 8.0;  // bracket bar width (ears stay at inline_btn_ear_w)
inline_btn_tray_x = 50.0;   // X position on tray
inline_btn_tray_y = 5.0;   // Y position on tray
btn_access_l   = 27.0;     // total length of U-groove
btn_access_w   = 10.0;     // width of U-groove (inner span between arms)
btn_access_ext = 10.0;     // closed end extends this far past button center
btn_groove_w   = 1.0;      // groove line width
btn_groove_x   = 50.0;     // groove center X on tray
btn_groove_y   = 15.0;      // groove center Y on tray

// --- SHELL RING ---
enclosure_d = 145.0;
wall = 3.0;
inner_h_ring = 35; //height of ring without lids

// --- LID ---
lid_t = 2.0;

// --- COMPONENTS  COORDINATES ---
spk_x = 1.0;
spk_y = -8.0;
tft_y = 42.0; 
esp_x = -44.0;
esp_y =   4.0;
mic_y = -60.0;
amp_pos = 32.0;

// --- GENERIC BOSS HEIGHTS ---
boss_h = 6.0;


// --- ESP DIMENTIONS---
esp_w = 28.5; 
esp_l = 57.5; 
esp_usb_w = 22.0;

//ESP32 buttons and LED window (we wanna see the hardware)
esp_win_w = 21.0; 
esp_win_l = 35.0; 
esp_win_dist = 8;
esp_boss_off_x = 14.5;
esp_boss_y_pos = (esp_l/2) + 4.0;
esp_bottom_boss_x_offset = 3;

// --- ESP ENCLOSURE---
esp_wall_height = 10.0;       // ESP wall height
esp_raiser_height = 4.0;      // inner ledge raiser height
esp_height_inc_back_pins = 3.5; // extra height on bosses to clear back pins
esp_boss_height = esp_raiser_height + esp_height_inc_back_pins; // boss height
esp_rail_delta = 10; //diffrence between ESP length and rail length
rail_w = 2.0; //rail width
raiser_t = 2.0; //raiser width

tft_hole_x = 30.0; tft_hole_y = 30.0; tft_view_d = 32.4;
amp_hole_x = 12.6; amp_hole_y_off = 7.15;
spk_d = 56.5; 

/* --- DYNAMIC TOTALS --- */
inner_d = enclosure_d - (wall * 2);
lid_screw_r = (enclosure_d/2) - 6;
lid_angles = [45, 135, 225, 315];
spk_boss_r = (spk_d/2) + 3.5;
outer_h_ring = lid_t*2 + inner_h_ring;

/* ==========================================
   2. MAIN ASSEMBLY CALL (ZERO-OVERLAP GRID)
   ========================================== */

render_component_tray();

// Cantilever bracket needs extra x clearance: origin is top-right boss,
// bracket extends (esp_boss_off_x*2 + 5)mm to the left of origin
translate([enclosure_d*0.8, 30, 0]) cantilever_bracket();

// Inline button frame (separate part)
translate([enclosure_d*0.8+5 , -50, inline_btn_h/2]) inline_button_frame();

// Inline button bracket (separate part, printed flat)
translate([enclosure_d*0.8+5 , 0, inline_btn_ear_t/2]) inline_button_bracket();

// Speaker Y-bracket (separate part, printed flat)
translate([enclosure_d*0.8+35, 0, 0]) speaker_y_bracket();

// Outer ring
translate([0, enclosure_d + 15, 0]) render_shell_ring();

// Lid
translate([enclosure_d + 15, (enclosure_d + 15), 0]) render_circular_lid();


/* ==========================================
   3. MODULE: THE COMPONENT TRAY (THE FACE)
   ========================================== */
module render_component_tray() {
    difference() {
        // Tray floor
        cylinder(d=inner_d + 1.3, h=lid_t, $fn=120);
        // Tray-to-Ring mounting holes
        for(a = lid_angles) rotate([0, 0, a]) translate([lid_screw_r, 0, -1])
            cylinder(r=1.7, h=10, $fn=32);
        // Viewports & Grilles
        translate([0, tft_y, -1]) cylinder(d=tft_view_d, h=lid_t + 2, $fn=80);
        translate([spk_x, spk_y, -1]) speaker_grille(spk_d - 6);
        // Speaker deboss: 57mm circle, 1mm into tray interior face
        translate([spk_x, spk_y, lid_t - 1.0]) cylinder(d=57, h=1.0, $fn=120);
        // Mic grille holes
        mic_grille();
        // ESP WiFi window through tray floor
        translate([esp_x - esp_win_w/2, esp_y - esp_l/2 + esp_win_dist, -1])
            cube([esp_win_w, esp_win_l, lid_t + 2]);
        // U-shaped groove: cuts through entire tray thickness
        translate([0, 0, -1]) {
            // Left arm
            translate([btn_groove_x - btn_access_w/2 - btn_groove_w, btn_groove_y - btn_access_ext, 0])
                cube([btn_groove_w, btn_access_l, lid_t + 2]);
            // Right arm
            translate([btn_groove_x + btn_access_w/2, btn_groove_y - btn_access_ext, 0])
                cube([btn_groove_w, btn_access_l, lid_t + 2]);
            // Bottom semicircle (annular ring, -y half)
            translate([btn_groove_x, btn_groove_y - btn_access_ext, 0])
                intersection() {
                    difference() {
                        cylinder(r=btn_access_w/2 + btn_groove_w, h=lid_t+2, $fn=32);
                        cylinder(r=btn_access_w/2,                 h=lid_t+3, $fn=32);
                    }
                    translate([-(btn_access_w + btn_groove_w), -(btn_access_w + btn_groove_w), 0])
                        cube([(btn_access_w + btn_groove_w)*2, btn_access_w + btn_groove_w, lid_t+3]);
                }
        }
    }
    translate([0, 0, lid_t]) {
        component_esp32();
        component_tft();
        component_speaker();
        component_amp();
        component_mic();
        component_inline_button();
    }
}

/* ==========================================
   4. MODULE: THE SHELL RING (THE TUBE)
   ========================================== */
module render_shell_ring() {
    difference() {
        union() {
            difference() {
                cylinder(d=enclosure_d, h=outer_h_ring, $fn=120);
                translate([0, 0, -1]) cylinder(d=inner_d, h=outer_h_ring + 2, $fn=120);
                // Top ledge (Lid) and Bottom ledge (Tray)
                translate([0, 0, outer_h_ring - lid_t]) cylinder(d=inner_d + 1.5, h=lid_t + 1, $fn=120);
                translate([0, 0, -1]) cylinder(d=inner_d + 1.5, h=lid_t + 1, $fn=120);
            }
            // Shared screw bosses — start after bottom cover, end before top cover
            for(a = lid_angles) rotate([0, 0, a]) translate([lid_screw_r, 0, lid_t])
                cylinder(r=4.5, h=outer_h_ring - (lid_t * 2), $fn=32);
        }
        // Pilot Holes (through-all)
        for(a = lid_angles) rotate([0, 0, a]) translate([lid_screw_r, 0, -1]) 
            cylinder(r=1.4, h=outer_h_ring + 2, $fn=16);
            
        // Interface Cutouts
        translate([esp_x, -enclosure_d/2, lid_t + inner_h_ring/2]) cube([esp_usb_w + 2, 15, 12], center=true);
        // USB cable hole between amp (45°) and screen (90°)
        rotate([0, 0, 67.5]) translate([enclosure_d/2 - 5, 0, lid_t + inner_h_ring/2]) rotate([0, 90, 0]) cylinder(d=13, h=10, $fn=40);
    }
}

/* ==========================================
   5. BOSS UTILITY
   ========================================= */

// Solid boss with chamfer at top and optional gussets.
// od          = outer diameter of boss
// pilot_d     = pilot hole diameter (subtracted internally)
// h           = boss height
// n_gussets   = number of gussets (0 = none)
// gusset_ang  = angular spread between gussets (unused when n_gussets=0)
module make_boss(od, pilot_d, h, n_gussets=0, gusset_ang=0) {
    chamfer = 0.6;
    difference() {
        union() {
            cylinder(d=od, h=h, $fn=32);
            if (n_gussets > 0) {
                gusset_h = h * 0.6;
                gusset_w = 1.2;
                gusset_l = od * 0.8;
                for(i = [0 : n_gussets - 1])
                    rotate([0, 0, i * (360 / n_gussets) + gusset_ang])
                        hull() {
                            // Base: full rectangle at z=0
                            translate([od/2, -gusset_w/2, 0])
                                cube([gusset_l, gusset_w, 0.01]);
                            // Tip: thin sliver at z=gusset_h, flush with boss
                            translate([od/2, -gusset_w/2, gusset_h])
                                cube([0.01, gusset_w, 0.01]);
                        }
            }
        }
        // Pilot hole
        translate([0, 0, -1]) cylinder(d=pilot_d, h=h+2, $fn=16);
        // Chamfer at top: 1mm around the hole only
        translate([0, 0, h - chamfer])
            cylinder(d1=pilot_d, d2=pilot_d + 0.8, h=chamfer + 0.01, $fn=32);
    }
}


/* ==========================================
   6. COMPONENT LOGIC
   ========================================= */
module component_esp32() {
    translate([esp_x, esp_y, 0]) difference() {
        union() {
            translate([-esp_boss_off_x+esp_bottom_boss_x_offset, -esp_boss_y_pos, 0]) make_boss(od=5, pilot_d=1.9, h=esp_boss_height, n_gussets=1, gusset_ang=250);
            translate([esp_boss_off_x,  esp_boss_y_pos, 0])  make_boss(od=5, pilot_d=1.9, h=esp_boss_height, n_gussets=1, gusset_ang=90);
            
            rail_l=esp_l-esp_rail_delta; //shortening the rails by delta
            rail_y_start = -(esp_l/2)+esp_rail_delta/2;
            translate([-(esp_w/2 + rail_w), rail_y_start, 0]) cube([rail_w, rail_l, esp_wall_height]);
            translate([esp_w/2,             rail_y_start, 0]) cube([rail_w, rail_l, esp_wall_height]);
            translate([-(esp_w/2),          rail_y_start, 0]) cube([raiser_t, rail_l, esp_raiser_height]);
            translate([esp_w/2 - raiser_t,  rail_y_start, 0]) cube([raiser_t, rail_l, esp_raiser_height]);
        }
        translate([-esp_w/2, -esp_l/2, 4.5]) cube([esp_w, esp_l, 10]);
        translate([-esp_win_w/2, -esp_l/2 + esp_win_dist, -lid_t-1]) cube([esp_win_w, esp_win_l, 10]);
    }
}

module component_tft() {
    translate([0, tft_y, 0])
        for(ix=[-tft_hole_x/2, tft_hole_x/2], iy=[-tft_hole_y/2, tft_hole_y/2])
            translate([ix, iy, 0]) make_boss(od=5, pilot_d=1.9, h=4, n_gussets=0);
}

module component_speaker() {
    translate([spk_x, spk_y, 0])
        for(a = [0, 120, 240]) rotate([0, 0, a]) translate([32.0, 0, 0])
            make_boss(od=5, pilot_d=1.9, h=12, n_gussets=3, gusset_ang=0);
}

module component_amp() {
    translate([amp_pos, amp_pos, 0]) {
        translate([-amp_hole_x/2, amp_hole_y_off, 0]) make_boss(od=5, pilot_d=1.9, h=11, n_gussets=3, gusset_ang=60);
        translate([amp_hole_x/2,  amp_hole_y_off, 0]) make_boss(od=5, pilot_d=1.9, h=11, n_gussets=3, gusset_ang=0);
    }
}

module component_inline_button() {
    inner = inline_btn_body + inline_btn_clr * 2;
    outer = inner + wall * 2;
    screw_offset = outer/2 + inline_btn_ear_ext - inline_btn_ear_w/2;
    bh = inline_btn_h - inline_btn_ear_t + 2;
    translate([inline_btn_tray_x, inline_btn_tray_y, 0])
        for(s = [1, -1])
            translate([s * screw_offset, 0, 0])
                make_boss(od=5, pilot_d=1.9, h=bh+1);
}

module component_mic() {
    translate([0, mic_y, 0])
        for(ix=[-8, 8]) translate([ix, 0, 0]) make_boss(od=5, pilot_d=1.9, h=boss_h, n_gussets=0);
}

module mic_grille() {
    // 2-3-2 honeycomb grille holes through tray floor
    mic_gr = 1.0; mic_xs = 3.0; mic_ys = 2.6;
    translate([0, mic_y, 0])
        for(p = [[-mic_xs/2, mic_ys], [mic_xs/2, mic_ys],
                 [-mic_xs,   0],      [0, 0],      [mic_xs, 0],
                 [-mic_xs/2, -mic_ys],[mic_xs/2, -mic_ys]])
            translate([p[0], p[1], -1]) cylinder(r=mic_gr, h=lid_t+2, $fn=16);
}

/* ==========================================
   6. MODULE: THE LID (THE BACK)
   ========================================== */
module render_circular_lid() {
    difference() {
        cylinder(d=inner_d + 1.3, h=lid_t, $fn=120);
        for(a = lid_angles) rotate([0, 0, a]) translate([lid_screw_r, 0, -1]) cylinder(r=1.7, h=10, $fn=32);
    }
}

/* ==========================================
   7. UTILITY BRACKETS (NO OVERLAP)
   ========================================== */
module cantilever_bracket() {
    // Origin = top-right boss. Goes left → down → left to bottom-left boss.
    // Boss separation: dx = esp_boss_off_x*2, dy = esp_boss_y_pos*2
    bw = 10.0; bh = 1.2;
    cantilever_bracket_width = esp_boss_off_x * 2-esp_bottom_boss_x_offset ;
    difference() {
        union() {
            // Top arm: top-right boss → spine (going left), ends at boss center
            translate([-(esp_boss_off_x + bw/2), -bw/2, 0])
                cube([esp_boss_off_x + bw/2, bw, bh]);
            // Spine: full height at x = -esp_boss_off_x (going down)
            translate([-(esp_boss_off_x + bw/2), -(esp_boss_y_pos*2 + bw/2), 0])
                cube([bw, esp_boss_y_pos*2 + bw, bh]);
            // Bottom arm: spine → bottom-left boss (going left), ends at boss center
            translate([-(cantilever_bracket_width), -(esp_boss_y_pos*2 + bw/2), 0])
                cube([esp_boss_off_x + bw/2-esp_bottom_boss_x_offset, bw, bh]);
            // Semicircle caps at each arm end
            //top semicircle
            cylinder(r=bw/2, h=bh, $fn=32);
            //bottom semicircle
            translate([-(cantilever_bracket_width), -(esp_boss_y_pos*2), 0]) cylinder(r=bw/2, h=bh, $fn=32);
        }
        // Screw holes at boss positions
        translate([0, 0, -1])                                    cylinder(r=1.7, h=bh+2, $fn=20);
        translate([-(cantilever_bracket_width), -(esp_boss_y_pos*2), -1]) cylinder(r=1.7, h=bh+2, $fn=20);
    }
}

module speaker_y_bracket() {
    // Y-bracket matching the 3 speaker bosses at r=32mm, angles 0/120/240
    // 1.2mm thick, 8mm wide arms, hub r=8mm, screw holes r=1.2mm
    bt = 1.2; arm_w = 8.0; hub_r = 8.0; boss_r = 34.0;
    difference() {
        union() {
            cylinder(r=hub_r, h=bt, $fn=48);
            for(a = [0, 120, 240]) rotate([0, 0, a])
                translate([0, -arm_w/2, 0]) cube([boss_r, arm_w, bt]);
            // Tip pads around each boss hole
            for(a = [0, 120, 240]) rotate([0, 0, a]) translate([boss_r, 0, 0])
                cylinder(r=arm_w/2, h=bt, $fn=32);
        }
        // Pill-shaped slots for M3 screws (3.2mm wide, 2mm slot leeway, radially oriented)
        for(a = [0, 120, 240]) rotate([0, 0, a]) translate([boss_r, 0, -1])
            hull() {
                translate([-2.4, 0, 0]) cylinder(r=1.6, h=bt+2, $fn=16);
                translate([ 0.4, 0, 0]) cylinder(r=1.6, h=bt+2, $fn=16);
            }
    }
}
module speaker_grille(d) { spacing = 6; range_limit = floor((d/2)/spacing) * spacing; for(i = [-range_limit:spacing:range_limit], j = [-range_limit:spacing:range_limit]) if(sqrt(i*i + j*j) < d/2) translate([i, j, -1]) cylinder(h=lid_t+5, r=2.2, $fn=12); }

/* ==========================================
   8. INLINE BUTTON FRAME
   ==========================================
   Surrounds a standard 12×12 tactile button.
   Open bottom, top wall with actuator hole.
   ========================================== */
module inline_button_frame() {
    inner = inline_btn_body + inline_btn_clr * 2;  // internal cavity XY
    outer = inner + wall * 2;
    outer_frame = inner + inline_btn_frame_t * 2;

    // Ear sits flush with the bottom of the frame
    ear_z = -inline_btn_h/2;
    // Screw hole center: midway along the extension, centered on ear width
    screw_offset = outer/2 + inline_btn_ear_ext - inline_btn_ear_w/2;

    difference() {
        union() {
            // Outer shell: top at +inline_btn_h/2, bottom flush with ear bottom
            translate([-outer_frame/2, -outer_frame/2, -(inline_btn_h/2 + inline_btn_ear_t/2)])
                cube([outer_frame, outer_frame, inline_btn_h + inline_btn_ear_t/2]);
            // Ears on +Y and -Y faces
            for(s = [1, -1])
                translate([0, s * (outer_frame/2 + inline_btn_ear_ext/2), ear_z])
                    cube([inline_btn_ear_w, inline_btn_ear_ext, inline_btn_ear_t], center=true);
        }
        // Inner cavity — open bottom, top wall of inline_btn_top_t remains
        translate([-inner/2, -inner/2, -(inline_btn_h/2 + inline_btn_ear_t/2) - 1])
            cube([inner, inner, inline_btn_h + inline_btn_ear_t/2 - inline_btn_top_t + 1]);
        // Actuator hole through top wall
        translate([0, 0, inline_btn_h/2 - inline_btn_top_t - 1])
            cylinder(d=inline_btn_act_d, h=inline_btn_top_t + 2, $fn=32);
        // M3 screw holes through ears
        for(s = [1, -1])
            translate([0, s * screw_offset, ear_z - inline_btn_ear_t/2 - 1])
                cylinder(d=inline_btn_ear_screw_d, h=inline_btn_ear_t + 2, $fn=24);
    }
}

/* Flat bracket that bridges the two ear screw holes.
   Printed flat on the bed. Hole spacing matches inline_button_frame ears. */
module inline_button_bracket() {
    inner = inline_btn_body + inline_btn_clr * 2;
    outer = inner + wall * 2;
    screw_offset = outer/2 + inline_btn_ear_ext - inline_btn_ear_w/2;

    span   = screw_offset * 2;          // center-to-center hole distance
    bar_l  = span + inline_btn_ear_w;   // end pad beyond each hole
    bar_w  = inline_btn_bracket_w;
    bar_t  = inline_btn_ear_t;

    difference() {
        cube([bar_w, bar_l, bar_t], center=true);
        // M3 holes at matching ear positions
        for(s = [1, -1])
            translate([0, s * screw_offset, -bar_t/2 - 1])
                cylinder(d=inline_btn_ear_screw_d, h=bar_t + 2, $fn=24);
    }
}