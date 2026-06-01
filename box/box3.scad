/* ==========================================================================
   DOCUMENTATION & BILL OF MATERIALS (CIRCULAR EDITION - REV 36)
   ==========================================================================
   1. RING: 145mm OD. Internal ledges at top AND bottom.
   2. TRAY: Sits inside bottom ledge. Holds viewports & components facing floor.
   3. LID: Sits inside top ledge. 
   4. LAYOUT: Spaced grid at x=100 to clear cantilever tongue projection.
   ========================================================================== */

/* ==========================================
   0. RENDER CONTROL
   ========================================= */
RING        = 1;
TRAY        = 1;
LID         = 1;
ACCESORIES  = 1;
TRAY_TYPE  = "3DPRINT"; // [3DPRINT, PCB]
BUILD_TYPE = "HOUSING"; // [HOUSING, PCB_EDGE_CUTS, PCB_MARKING, PCB_SILKSCREEN]

pcb_mode    = (BUILD_TYPE == "PCB_EDGE_CUTS" || BUILD_TYPE == "PCB_MARKING" || BUILD_TYPE == "PCB_SILKSCREEN");
_RING       = pcb_mode ? 0 : RING;
_TRAY       = pcb_mode ? 0 : TRAY;
_LID        = pcb_mode ? 0 : LID;
_ACCESORIES = pcb_mode ? 0 : ACCESORIES;
_PCB        = pcb_mode ? 1 : 0;

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
inline_btn_ear_ext = 6.0;  // how far each ear extends from the wall face
inline_btn_ear_w   = 10.0;   // ear width (along the wall)
inline_btn_ear_t   = 2.5;   // ear thickness
inline_btn_ear_screw_d = 3.4; // r=1.7 clearance hole
inline_btn_bracket_w  = 8.0;  // bracket bar width (ears stay at inline_btn_ear_w)
inline_btn_tray_x = 50.0;   // X position on tray
inline_btn_tray_y = 10.0;   // Y position on tray
btn_access_l   = 17.0;     // total length of U-groove
btn_access_w   = 10.0;     // width of U-groove (inner span between arms)
btn_access_ext = 10.0;     // closed end extends this far past button center
btn_groove_w   = 1.0;      // groove line width
btn_groove_x   = 50.0;     // groove center X on tray
btn_groove_y   = 20.0;      // groove center Y on tray

// --- SHELL RING ---
enclosure_d = 145.0;
wall = 3.0;
ring_inner_h_3dprint = 35;  // cavity height (no lids) for the 3DPRINT tray — needs room for jumpers above the modules
ring_inner_h_pcb     = 20;  // cavity height (no lids) for the PCB tray — no jumpers, can be much shorter. Tune after first print.

// --- LID ---
lid_t = 2.0;

// --- PCB ---
pcb_height = 1.6;
pcb_offset = 2.0;   // inset from ring inner wall
pcb_line_w = 0.4;   // blue reference line width
pcb_boss_r   = 63.0; // radius of PCB mounting bosses (inward from ring bosses at 66.5mm)
pcb_boss_rot = -10.0; // clockwise rotation offset in degrees (negative = clockwise)

// --- COMPONENTS  COORDINATES ---
spk_x = 1.0;
spk_y = -8.0;
tft_y = 42.0; 
esp_x = -44.0;
esp_y =   4.0;
mic_y = -60.0;
mic_boss_x = 8.0;
amp_pos = 30.0;

// --- PCM5102A DAC ---
pcm5102a_x      = 51.5;
pcm5102a_y      = -22.0;
pcm5102a_long   = 32.0;
pcm5102a_short  = 17.33;
pcm5102a_rot    = -23;     // rotation around (pcm5102a_x, pcm5102a_y), degrees CCW

// --- HEADPHONES HOLE (PCM5102A jack passthrough on ring) ---
headphones_hole_d    = 6.5;  // hole diameter (typical 3.5mm panel-mount jack)
headphones_above_pcb = 3.0;  // jack center height above PCB top — a property of the jack itself, not of the housing

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

// --- EXPANSION HEADER ---
// Three parallel through-hole rows in the lower band of the PCB
// (below the speaker bracket arm at 240°, above the mic):
//   * Data row  (20 pads, 2.54mm): every ESP32-S3 GPIO not driven by a peripheral.
//                          "*" suffix marks boot-strapping pins (0, 3, 45, 46).
//                          GPIO 48 is included even though the firmware drives
//                          the status LED on it — repurposing means releasing
//                          it in firmware first.
//                          35/36/37 are the trailing spare GPIOs.
//   * Power row (11 pads, 2.54mm): 3V3 (×3), GND (×5), 5V (×3) — grouped left-to-right.
//   * Aux row    (2 pads, 5.08mm): module config jumpers — PCM5102A SCK (J7.1) and
//                          MAX98357A GAIN (J5.4). Wider pitch so the two labels fit
//                          on silkscreen. Left pad → SCK, right pad → GAIN.
expansion_hdr_x          = 0.0;
expansion_data_y         = -42.0;
expansion_power_y        = -47.0;
expansion_aux_y          = -52.0;
expansion_hdr_pitch      = 2.54;
expansion_aux_pitch      = 5.08;
expansion_hdr_text_dy    = -1.5;    // label top offset below each pad row
expansion_hdr_text_size  = 1.4;

// --- PCB branding (appears on both pcb_marking.dxf and pcb_silkscreen.dxf) ---
// Placed near the speaker hole, top-right, oriented along the tangent (~45deg).
// Tweak freely — version/date are the values from when this PCB was authored.
brand_url       = "www.iollama.com";
brand_name      = "Voice Agent 5";
brand_version   = "v0.8";
brand_date      = "2026-05-31";   // PCB authoring date
brand_angle     = 45;             // direction from speaker center (deg; top-right)
brand_offset_r  = 34.0;           // radial distance of the text block from speaker center
brand_line_h    = 2.4;            // line spacing (mm)
brand_text_size = 1.8;            // glyph size (mm)

expansion_data_labels = [
    "0*", "3*",
    "8", "9", "10", "11", "12", "13", "14", "15", "16", "18", "19", "20",
    "45*", "46*", "48",
    "35", "36", "37"
];
expansion_power_labels = [
    "3V", "3V", "3V",
    "G", "G", "G", "G", "G",
    "5V", "5V", "5V"
];
expansion_aux_labels = [
    "SCK", "GAIN"
];

/* --- DYNAMIC TOTALS --- */
inner_d = enclosure_d - (wall * 2);
lid_screw_r = (enclosure_d/2) - 6;
lid_angles = [45, 135, 225, 315];
spk_boss_r = (spk_d/2) + 3.5;
// Active cavity height depends on tray type — PCB build is shorter (no jumpers).
inner_h_ring = (TRAY_TYPE == "PCB") ? ring_inner_h_pcb : ring_inner_h_3dprint;
outer_h_ring = lid_t*2 + inner_h_ring;
// PCB-tray standoffs hang from the lid; their length is set so the PCB still
// sits at z = lid_t + esp_boss_height (same plane as the 3DPRINT case) so the
// components on the PCB bottom continue to reach the tray viewports.
pcb_lid_standoff_h = inner_h_ring - esp_boss_height - pcb_height;
// PCB top plane (z, world coordinates). Used by the headphone hole and any
// other feature anchored to the PCB rather than to the housing.
pcb_top_z = lid_t + esp_boss_height + pcb_height;
// Headphone hole: angle = direction from origin to the PCM5102A corner closest
// to the ring; z = a fixed offset above the PCB top so the hole tracks the
// PCB regardless of standoff side or ring height.
// Local frame: long axis along Y (jack on +Y short edge), short axis along X
// (right side at +X). Top-right corner = (+short/2, +long/2).
headphones_corner_x = pcm5102a_x + (pcm5102a_short/2) * cos(pcm5102a_rot) - (pcm5102a_long/2) * sin(pcm5102a_rot);
headphones_corner_y = pcm5102a_y + (pcm5102a_short/2) * sin(pcm5102a_rot) + (pcm5102a_long/2) * cos(pcm5102a_rot);
headphones_angle    = atan2(headphones_corner_y, headphones_corner_x);
headphones_hole_z   = pcb_top_z + headphones_above_pcb;

/* ==========================================
   2. MAIN ASSEMBLY CALL (ZERO-OVERLAP GRID)
   ========================================== */

if (1==_TRAY)       render_component_tray();

if (1==_ACCESORIES) {
    if (TRAY_TYPE == "3DPRINT") {
        // Cantilever bracket needs extra x clearance: origin is top-right boss,
        // bracket extends (esp_boss_off_x*2 + 5)mm to the left of origin
        translate([enclosure_d*0.8, 30, 0]) cantilever_bracket();
        // Inline button frame (separate part)
        translate([enclosure_d*0.8+5 , -50, inline_btn_h/2]) inline_button_frame();
        // Inline button bracket (separate part, printed flat)
        translate([enclosure_d*0.8+5 , 0, inline_btn_ear_t/2]) inline_button_bracket();
    }
    // Speaker Y-bracket (separate part, printed flat)
    translate([enclosure_d*0.8+35, 0, 0]) speaker_y_bracket();
}

if (1==_PCB) translate([0, -(enclosure_d + 15), 0]) {
    if (BUILD_TYPE != "PCB_MARKING" && BUILD_TYPE != "PCB_SILKSCREEN") projection() render_pcb();
    pcb_reference_overlay();
}

if (1==_RING)       translate([0, enclosure_d + 15, 0]) render_shell_ring();

if (1==_LID)        translate([enclosure_d + 15, (enclosure_d + 15), 0]) render_circular_lid();

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
        component_speaker();
        if (TRAY_TYPE == "3DPRINT") component_3dprint_mount();
        // PCB tray: PCB standoffs live on the lid, not here.
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
        // Headphone jack hole — only when the PCM5102A is present (PCB tray)
        if (TRAY_TYPE == "PCB")
            rotate([0, 0, headphones_angle]) translate([enclosure_d/2 - 5, 0, headphones_hole_z])
                rotate([0, 90, 0]) cylinder(d=headphones_hole_d, h=10, $fn=40);
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
                make_boss(od=5, pilot_d=1.9, h=bh+1, n_gussets=2, gusset_ang=90);
}

module component_mic() {
    translate([0, mic_y, 0])
        for(ix=[-mic_boss_x, mic_boss_x]) translate([ix, 0, 0]) make_boss(od=5, pilot_d=1.9, h=boss_h, n_gussets=2, gusset_ang=270);
}

module component_pcb_mount(h) {
    for(a = lid_angles) rotate([0, 0, a + pcb_boss_rot]) translate([pcb_boss_r, 0, 0])
        make_boss(od=5, pilot_d=1.9, h=h, n_gussets=1, gusset_ang=0);
}

module component_3dprint_mount() {
    component_esp32();
    component_tft();
    component_amp();
    component_mic();
    component_inline_button();
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
   6. MODULE: THE PCB DISK
   ========================================== */
module pcb_disk() {
    cylinder(d=inner_d - pcb_offset * 2, h=pcb_height, $fn=120);
}

// 2.2mm standoff clearance hole at every boss that physically touches the
// PCB tray variant. The TFT, Amp, Mic, and Button-bracket bosses only exist
// on the 3DPRINT tray (they hold those modules to the tray floor); on the
// PCB tray those modules ride on their headers, so drilling clearance holes
// there would just remove copper. Those four boss groups still appear as
// marks in pcb_marking.dxf so the assembler can see the 3DPRINT-tray
// positions; they are intentionally NOT cut here.
module _boss_holes() {
    d = 2.2; h = pcb_height + 2;
    // Speaker (3 bosses at r=32, 120° apart) — sit under the Y-bracket
    // clearance cutout anyway, kept here for symmetry of intent.
    translate([spk_x, spk_y, -1])
        for(a = [0, 120, 240]) rotate([0, 0, a]) translate([32.0, 0, 0])
            cylinder(d=d, h=h, $fn=24);
    // PCB mount (4 bosses at r=pcb_boss_r) — how the PCB hangs from the lid.
    for(a = lid_angles) rotate([0, 0, a + pcb_boss_rot]) translate([pcb_boss_r, 0, -1])
        cylinder(d=d, h=h, $fn=24);
}

// Semi-circle clearance notches where the four ring bosses meet the PCB edge.
module _ring_boss_notches() {
    for(a = lid_angles) rotate([0, 0, a]) translate([lid_screw_r, 0, -1])
        cylinder(r=4.5 + pcb_offset, h=pcb_height + 2, $fn=32);
}

module render_pcb() {
    difference() {
        pcb_disk();
        _boss_holes();
        _ring_boss_notches();
        // Speaker clearance: deboss + Y-bracket footprint + speaker bosses
        translate([spk_x, spk_y, -1]) {
            cylinder(d=57, h=pcb_height + 2, $fn=120);        // deboss circle
            cylinder(r=8,  h=pcb_height + 2, $fn=48);         // bracket hub
            for(a = [0, 120, 240]) rotate([0, 0, a]) {
                translate([0, -4, 0]) cube([32, 8, pcb_height + 2]); // bracket arm
                translate([32, 0, 0]) cylinder(r=4, h=pcb_height + 2, $fn=30); // bracket tip pad
                translate([30, 0, 0]) cylinder(d=5, h=pcb_height + 2, $fn=30); // speaker boss
            }
        }
    }
}

module _marking_shapes(include_refs=true) {
    _marking_branding();
    // TFT viewport circle
    projection() translate([0, tft_y, 0])
        difference() {
            cylinder(d=tft_view_d + pcb_line_w, h=1, $fn=80);
            cylinder(d=tft_view_d - pcb_line_w, h=2, $fn=80);
        }
    // TFT bosses
    if (include_refs) projection() for(ix=[-tft_hole_x/2, tft_hole_x/2], iy=[-tft_hole_y/2, tft_hole_y/2])
        translate([ix, tft_y + iy, 0])
            difference() {
                cylinder(d=5 + pcb_line_w, h=1, $fn=32);
                cylinder(d=5 - pcb_line_w, h=2, $fn=32);
            }
    // TFT header row (GC9A01: 8 pins at 2.54mm pitch, ~6mm above top mounting hole)
    projection() translate([0, tft_y + 21, 0])
        for(i = [0:7]) translate([(i - 3.5) * 2.54, 0, 0])
            cylinder(d=1, h=1, $fn=20);
    // Mic bosses
    if (include_refs) projection() for(ix=[-mic_boss_x, mic_boss_x])
        translate([ix, mic_y, 0])
            difference() {
                cylinder(d=5 + pcb_line_w, h=1, $fn=32);
                cylinder(d=5 - pcb_line_w, h=2, $fn=32);
            }
    // INMP441 mic board outline (14mm circle)
    projection() translate([0, mic_y, 0])
        difference() {
            cylinder(d=14 + pcb_line_w, h=1, $fn=80);
            cylinder(d=14 - pcb_line_w, h=2, $fn=80);
        }
    // INMP441 pin holes (3 pads on each side at 2.54mm pitch)
    projection() translate([0, mic_y, 0])
        for(ix=[-4, 4], iy=[-2.54, 0, 2.54]) translate([ix, iy, 0])
            cylinder(d=1, h=1, $fn=20);
    // Amp bosses
    if (include_refs) projection() translate([amp_pos, amp_pos, 0])
        for(sx=[-1, 1]) translate([sx * amp_hole_x/2, amp_hole_y_off, 0])
            difference() {
                cylinder(d=5 + pcb_line_w, h=1, $fn=32);
                cylinder(d=5 - pcb_line_w, h=2, $fn=32);
            }
    // AMP board outline (MAX98357A 17.7x19.1mm; speaker output edge faces -Y)
    projection() translate([amp_pos, amp_pos + amp_hole_y_off, 0])
        difference() {
            translate([-17.7/2 - pcb_line_w, -2.4 - pcb_line_w, 0])
                cube([17.7 + pcb_line_w*2, 19.1 + pcb_line_w*2, 1]);
            translate([-17.7/2, -2.4, -1])
                cube([17.7, 19.1, 3]);
        }
    // AMP header row (7 pins at 2.54mm pitch, near +Y edge opposite speaker output)
    projection() translate([amp_pos, amp_pos + amp_hole_y_off + 16.7 - 2.54, 0])
        for(i = [-3:3]) translate([i * 2.54, 0, 0])
            cylinder(d=1, h=1, $fn=20);
    // Button groove inner outline
    if (include_refs) projection() {
        translate([btn_groove_x - btn_access_w/2 - pcb_line_w, btn_groove_y - btn_access_ext, 0])
            cube([pcb_line_w, btn_access_l, 1]);
        translate([btn_groove_x + btn_access_w/2, btn_groove_y - btn_access_ext, 0])
            cube([pcb_line_w, btn_access_l, 1]);
        translate([btn_groove_x, btn_groove_y - btn_access_ext, 0])
            intersection() {
                difference() {
                    cylinder(r=btn_access_w/2 + pcb_line_w, h=1, $fn=32);
                    cylinder(r=btn_access_w/2,               h=2, $fn=32);
                }
                translate([-(btn_access_w + pcb_line_w), -(btn_access_w + pcb_line_w), 0])
                    cube([(btn_access_w + pcb_line_w)*2, btn_access_w + pcb_line_w, 2]);
            }
    }
    // Button bracket bosses
    if (include_refs) projection() translate([inline_btn_tray_x, inline_btn_tray_y, 0]) {
        _inner = inline_btn_body + inline_btn_clr * 2;
        _screw_off = (_inner + wall*2)/2 + inline_btn_ear_ext - inline_btn_ear_w/2;
        for(s=[1,-1]) translate([s * _screw_off, 0, 0])
            difference() {
                cylinder(d=5 + pcb_line_w, h=1, $fn=32);
                cylinder(d=5 - pcb_line_w, h=2, $fn=32);
            }
    }
    // Button center dot
    if (include_refs) projection() translate([inline_btn_tray_x, inline_btn_tray_y, 0])
        cylinder(d=2, h=1, $fn=24);
    // Button pins (4 pins in a square, 5.08mm apart)
    projection() translate([inline_btn_tray_x, inline_btn_tray_y, 0])
        for(ix=[-2.54, 2.54], iy=[-2.54, 2.54]) translate([ix, iy, 0])
            cylinder(d=1, h=1, $fn=20);
    // PTT paired-contact links: each row of two contacts is internally shorted,
    // so draw a bar joining them (top pair = PTT_BTN, bottom pair = GND).
    projection() translate([inline_btn_tray_x, inline_btn_tray_y, 0])
        for(sy = [-2.54, 2.54])
            translate([-2.54, sy - pcb_line_w/2, 0])
                cube([5.08, pcb_line_w, 1]);
    // Button body outline (12mm square)
    projection() translate([inline_btn_tray_x, inline_btn_tray_y, 0])
        difference() {
            translate([-(inline_btn_body + pcb_line_w)/2, -(inline_btn_body + pcb_line_w)/2, 0])
                cube([inline_btn_body + pcb_line_w, inline_btn_body + pcb_line_w, 1]);
            translate([-(inline_btn_body - pcb_line_w)/2, -(inline_btn_body - pcb_line_w)/2, -1])
                cube([inline_btn_body - pcb_line_w, inline_btn_body - pcb_line_w, 3]);
        }
    // PCM5102A board outline. Local frame: long axis along Y (jack on +Y short
    // edge facing the ring), short axis along X. 9-pin row on LEFT long edge
    // (x = -short/2), 6-pin row on BOTTOM short edge (y = -long/2).
    projection() translate([pcm5102a_x, pcm5102a_y, 0]) rotate([0, 0, pcm5102a_rot])
        difference() {
            translate([-pcm5102a_short/2 - pcb_line_w, -pcm5102a_long/2 - pcb_line_w, 0])
                cube([pcm5102a_short + pcb_line_w*2, pcm5102a_long + pcb_line_w*2, 1]);
            translate([-pcm5102a_short/2, -pcm5102a_long/2, -1])
                cube([pcm5102a_short, pcm5102a_long, 3]);
        }
    // PCM5102A 9-pin header (LEFT long edge, 2.54mm pitch, pads along Y)
    projection() translate([pcm5102a_x, pcm5102a_y, 0]) rotate([0, 0, pcm5102a_rot])
        translate([-(pcm5102a_short/2 - 1.27), 0, 0])
            for(i = [0:8]) translate([0, (i - 4) * 2.54, 0])
                cylinder(d=1, h=1, $fn=20);
    // PCM5102A 6-pin header (BOTTOM short edge, 2.54mm pitch, pads along X)
    projection() translate([pcm5102a_x, pcm5102a_y, 0]) rotate([0, 0, pcm5102a_rot])
        translate([0, -(pcm5102a_long/2 - 1.27), 0])
            for(i = [0:5]) translate([(i - 2.5) * 2.54, 0, 0])
                cylinder(d=1, h=1, $fn=20);
    // ESP32 board outline
    projection() translate([esp_x, esp_y, 0])
        difference() {
            translate([-esp_w/2 - pcb_line_w, -esp_l/2 - pcb_line_w, 0])
                cube([esp_w + pcb_line_w*2, esp_l + pcb_line_w*2, 1]);
            translate([-esp_w/2, -esp_l/2, -1])
                cube([esp_w, esp_l, 3]);
        }
    // ESP32 pin headers (22 pins per side at 2.54mm pitch / 0.1in)
    projection() translate([esp_x, esp_y, 0])
        for(ix=[-esp_w/2 + 1.27, esp_w/2 - 1.27], i=[0:21])
            translate([ix, (i - 10.5) * 2.54, 0])
                cylinder(d=1, h=1, $fn=20);
    // Expansion header — DATA row (17 GPIO pads at 2.54mm pitch)
    projection() translate([expansion_hdr_x, expansion_data_y, 0])
        for(i = [0 : len(expansion_data_labels) - 1])
            translate([(i - (len(expansion_data_labels) - 1)/2) * expansion_hdr_pitch, 0, 0])
                cylinder(d=1, h=1, $fn=20);
    translate([expansion_hdr_x, expansion_data_y + expansion_hdr_text_dy, 0])
        for(i = [0 : len(expansion_data_labels) - 1])
            translate([(i - (len(expansion_data_labels) - 1)/2) * expansion_hdr_pitch, 0, 0])
                text(expansion_data_labels[i], size=expansion_hdr_text_size,
                     halign="center", valign="top");
    // Expansion header — POWER row (11 pads at 2.54mm pitch)
    projection() translate([expansion_hdr_x, expansion_power_y, 0])
        for(i = [0 : len(expansion_power_labels) - 1])
            translate([(i - (len(expansion_power_labels) - 1)/2) * expansion_hdr_pitch, 0, 0])
                cylinder(d=1, h=1, $fn=20);
    translate([expansion_hdr_x, expansion_power_y + expansion_hdr_text_dy, 0])
        for(i = [0 : len(expansion_power_labels) - 1])
            translate([(i - (len(expansion_power_labels) - 1)/2) * expansion_hdr_pitch, 0, 0])
                text(expansion_power_labels[i], size=expansion_hdr_text_size,
                     halign="center", valign="top");
    // Expansion header — AUX row (2 pads at 5.08mm pitch — PCM5102A SCK + MAX98357A GAIN)
    projection() translate([expansion_hdr_x, expansion_aux_y, 0])
        for(i = [0 : len(expansion_aux_labels) - 1])
            translate([(i - (len(expansion_aux_labels) - 1)/2) * expansion_aux_pitch, 0, 0])
                cylinder(d=1, h=1, $fn=20);
    // AUX labels flank outward with the module owner prefixed, so this SCK reads as
    // the PCM5102A's (distinct from the mic's I2S SCK) and GAIN as the MAX98357A's.
    translate([expansion_hdr_x, expansion_aux_y + expansion_hdr_text_dy, 0]) {
        translate([-expansion_aux_pitch/2 - 0.5, 0, 0])
            text(str("PCM ", expansion_aux_labels[0]), size=1.0, halign="right", valign="top");
        translate([expansion_aux_pitch/2 + 0.5, 0, 0])
            text(str("MAX ", expansion_aux_labels[1]), size=1.0, halign="left", valign="top");
    }
    // ============================================================
    // Module silkscreen marks (assembly aid: name + module pin labels)
    // ============================================================

    // --- ESP32-S3 host (J1 + J2) ---
    // Name centered inside the board outline (hidden by the dev board once seated).
    translate([esp_x, esp_y, 0])
        text("ESP32-S3", size=expansion_hdr_text_size, halign="center", valign="center");
    // J1 left row pad labels (outside the LEFT edge of the ESP board).
    // i=0 is the USB-end pad (KiCad pad 22 = GND); i=21 is the antenna-end pad (KiCad pad 1 = 3V3).
    for(i = [0:21])
        translate([esp_x - esp_w/2 - 1.5, esp_y + (i - 10.5) * 2.54, 0])
            text(["G","5V","14","13","12","11","10","9","46","3","8","18","17","16","15","7","6","5","4","EN","3V","3V"][i],
                 size=expansion_hdr_text_size, halign="right", valign="center");
    // J2 right row pad labels (outside the RIGHT edge of the ESP board).
    for(i = [0:21])
        translate([esp_x + esp_w/2 + 1.5, esp_y + (i - 10.5) * 2.54, 0])
            text(["G","G","19","20","21","47","48","45","0","35","36","37","38","39","40","41","42","2","1","RX","TX","G"][i],
                 size=expansion_hdr_text_size, halign="left", valign="center");

    // --- INMP441 mic (J3 + J4) ---
    // Name centered inside the mic outline; pad labels just outside each column.
    translate([0, mic_y, 0])
        rotate([0, 0, 90])
            text("INMP441", size=1.0, halign="center", valign="center");
    for(i = [0:2]) {
        translate([-8, mic_y + (i-1)*2.54, 0])
            text(["SCK","WS","L/R"][i], size=expansion_hdr_text_size, halign="right", valign="center");
        translate([+8, mic_y + (i-1)*2.54, 0])
            text(["SD","VDD","GND"][i], size=expansion_hdr_text_size, halign="left", valign="center");
    }

    // --- MAX98357A amp (J5) ---
    // Name CENTERED INSIDE the amp board outline (gets hidden when the amp
    // module is plugged in — visible during assembly). Pad labels rotated 90°
    // OUTSIDE the +Y edge so they remain visible after the module is seated.
    // Note: the GND/VIN labels at the +X end may slightly overflow the PCB
    // outline (amp board sits close to the PCB edge); marking DXF will clip
    // anything past the outline. Trade-off accepted for outside-the-frame style.
    translate([amp_pos, amp_pos + amp_hole_y_off + 19.1/2 - 2.4, 0])
        text("MAX98357A", size=expansion_hdr_text_size, halign="center", valign="center");
    for(i = [-3:3])
        translate([amp_pos + i*2.54, amp_pos + amp_hole_y_off + 16.7 + 0.5, 0])
            rotate([0, 0, 90])
                text(["LRC","BCLK","DIN","GAIN","SD","GND","VIN"][i+3],
                     size=expansion_hdr_text_size, halign="left", valign="center");

    // --- PCM5102A DAC (J6 + J7) ---
    // Labels rotate with the board (matched to pcm5102a_rot).
    // Name CENTERED INSIDE the board outline (hidden by the module after assembly).
    // 9-pin labels OUTSIDE the LEFT long edge (horizontal text, no overlap at
    // 2.54mm vertical pitch). 6-pin labels OUTSIDE the BOTTOM short edge,
    // rotated 90° so multi-char labels don't collide at 2.54mm pitch.
    translate([pcm5102a_x, pcm5102a_y, 0]) rotate([0, 0, pcm5102a_rot]) {
        // Module name centered on the board outline.
        text("PCM5102A", size=expansion_hdr_text_size, halign="center", valign="center");
        // 9-pin row: i=0 is BOTTOM of the column (LROUT), i=8 is TOP (FLT).
        // Labels just outside the LEFT long edge of the board.
        for(i = [0:8])
            translate([-(pcm5102a_short/2 + 0.5), (i - 4) * 2.54, 0])
                text(["LROUT","AGND","ROUT","AGND","A3V3","FMT","XSMT","DEMP","FLT"][i],
                     size=expansion_hdr_text_size, halign="right", valign="center");
        // 6-pin row: i=0 is LEFT (SCK), i=5 is RIGHT (VIN). Labels just outside
        // the BOTTOM short edge, rotated 90° (read bottom-to-top).
        for(i = [0:5])
            translate([(i - 2.5) * 2.54, -(pcm5102a_long/2 + 0.5), 0])
                rotate([0, 0, 90])
                    text(["SCK","BCK","DIN","LCK","GND","VIN"][i],
                         size=expansion_hdr_text_size, halign="right", valign="center");
    }

    // --- GC9A01 TFT (J8) ---
    // Name CENTERED on the TFT viewport circle (hidden by the TFT when seated).
    // Pad labels OUTSIDE the header, rotated 90° extending UP toward the PCB
    // edge. Edge labels (BLK at -X, GND at +X) may slightly clip the PCB outline.
    translate([0, tft_y, 0])
        text("GC9A01", size=expansion_hdr_text_size, halign="center", valign="center");
    for(i = [0:7])
        translate([(i - 3.5) * 2.54, tft_y + 21 + 0.5, 0])
            rotate([0, 0, 90])
                text(["BLK","CS","DC","RES","SDA","SCL","VCC","GND"][i],
                     size=expansion_hdr_text_size, halign="left", valign="center");

    // --- PTT button (SW1) ---
    // Label below the button body so it's visible after the switch is soldered.
    translate([inline_btn_tray_x, inline_btn_tray_y - inline_btn_body/2 - 1.5, 0])
        text("PTT", size=expansion_hdr_text_size, halign="center", valign="top");

    // ============================================================
    // PCB mount bosses
    // ============================================================
    if (include_refs) projection() for(a = lid_angles) rotate([0, 0, a + pcb_boss_rot]) translate([pcb_boss_r, 0, 0])
        difference() {
            cylinder(d=5 + pcb_line_w, h=1, $fn=32);
            cylinder(d=5 - pcb_line_w, h=2, $fn=32);
        }
}

// PCB branding: logo text + URL + version/date near the speaker, top-right,
// oriented along the tangent of the speaker circle (~brand_angle). Stacked
// outward-to-inward: brand_url, brand_name, then "version  date".
module _marking_branding() {
    _lines = [brand_url, brand_name, str(brand_version, "  ", brand_date)];
    translate([spk_x, spk_y, 0])
        rotate([0, 0, brand_angle - 90])         // orient text along the tangent
            translate([0, brand_offset_r, 0])    // push out along the brand_angle radial
                for (i = [0 : len(_lines) - 1])
                    translate([0, ((len(_lines) - 1) / 2 - i) * brand_line_h, 0])
                        text(_lines[i], size=brand_text_size,
                             halign="center", valign="center");
}

module pcb_reference_overlay() {
    if (BUILD_TYPE == "PCB_MARKING") {
        // Base on the actual PCB outline so the speaker cutout (and the boss /
        // ring-notch holes already in render_pcb) are inherited — anything that
        // falls inside the speaker bracket footprint (e.g. the three speaker
        // boss positions) is correctly absent from the marking.
        difference() {
            projection() render_pcb();
            _marking_shapes();
        }
    } else if (BUILD_TYPE == "PCB_SILKSCREEN") {
        // Same as the marking but WITHOUT the 3DPRINT-tray boss reference marks —
        // intended for import onto F.Silkscreen. Branding + labels + outlines stay.
        difference() {
            projection() render_pcb();
            _marking_shapes(include_refs=false);
        }
    } else if (BUILD_TYPE != "PCB_EDGE_CUTS") {
        // OpenSCAD GUI preview only: draw the marking shapes in blue as a
        // visual reference overlaid on the housing render. NEVER rendered for
        // PCB_EDGE_CUTS export — those marks must not bleed into the Edge.Cuts
        // DXF or KiCad will cut along silkscreen text/outlines on fab.
        color("blue") _marking_shapes();
    }
}

/* ==========================================
   7. MODULE: THE LID (THE BACK)
   ========================================== */
module render_circular_lid() {
    difference() {
        cylinder(d=inner_d + 1.3, h=lid_t, $fn=120);
        for(a = lid_angles) rotate([0, 0, a]) translate([lid_screw_r, 0, -1]) cylinder(r=1.7, h=10, $fn=32);
    }
    // PCB-tray: standoffs hang from the lid. Rendered pointing UP in the print
    // orientation; when the lid is installed (flipped) they reach DOWN into the
    // cavity and the PCB hangs from their tips.
    if (TRAY_TYPE == "PCB")
        translate([0, 0, lid_t]) component_pcb_mount(h=pcb_lid_standoff_h);
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