// Case for the Radioland nRF52832 coin-cell beacon, clamped onto a strip
// (default 50 mm wide, 5 mm thick).
//
// Three parts, each printable without supports:
//   body - cup holding the board battery-side down, on a flat flange that
//          rests on the strip
//   lid  - snaps onto the body; a flexible tongue with a nub presses the
//          board's button
//   clip - U-channel that goes under the strip and snaps over the flange
//          edges, clamping the strip between them
//
// Coordinates: board centre at the origin, component side up, lanyard notch
// towards -X. The strip runs along Y; z = 0 is the strip's top face.
//
// Render one part at a time, e.g.
//   openscad -D 'part="body"' -o stl/body.stl beacon_case.scad
// Parts: body, lid, clip (each in print orientation), assembly, section.

part = "assembly";

/* [Board] (same fit as the jig) */
board_d         = 25.2;   // measured 25.0 mm, enlarged after a jig test print
board_clearance = 0.15;   // per side
notch_pos       = [-11.76, 0.21];
notch_r         = 3.15;
board_stack     = 3.6;    // battery holder + PCB under the clamp rib; measured 3.5 squeezed
button_pos      = [-0.8, -0.5];  // ESTIMATE from Radioland's photo; measure on the board
button_h        = 2.0;    // ASSUMED top of the button above the PCB; measure

/* [Strip] */
strip_w         = 50;
strip_t         = 5;
strip_fit       = 0;      // extra room between flange and clip plate; negative clamps harder

/* [Body] */
flange_t        = 2.0;    // also the pocket floor
flange_side_clearance = 0.15;
wall_t          = 1.8;
lid_gap         = 3.0;    // PCB top to lid underside
key_w           = 2.0;    // rib on the body that orients the lid
key_d           = 1.0;

/* [Lid] */
lid_top_t       = 1.6;
lid_clearance   = 0.15;   // per side, skirt over body
skirt_t         = 1.6;
skirt_h         = 4.0;
snap_from_rim   = 2.0;
snap_groove     = 0.4;    // depth of the groove in the body
snap_bead       = 0.3;    // height of the bead in the lid skirt
clamp_rib       = [12.0, 12.55];  // inner and outer radius of the ring pressing the board edge

/* [Button] */
tongue_w        = 7;
tongue_len      = 10;
tongue_t        = 1.0;
slot_w          = 0.8;
nub_d           = 3;
nub_clearance   = 0.3;    // gap between nub and button at rest

/* [Clip] */
clip_l          = 30;
clip_plate_t    = 2.0;
clip_wall_t     = 1.2;
strip_side_clearance = 0.3;
hook_d          = 0.6;
hook_h          = 1.2;
post_d          = 1.0;    // end posts that stop the body sliding along the clip
post_len        = 2.0;

$fn = 96;
eps = 0.01;

pocket_r     = board_d / 2 + board_clearance;
cup_r        = pocket_r + wall_t;
board_top_z  = flange_t + board_stack;
rim_z        = board_top_z + lid_gap;
lid_top_z    = rim_z + lid_top_t;
skirt_r_in   = cup_r + lid_clearance;
skirt_r_out  = skirt_r_in + skirt_t;
channel_w    = strip_w + 2 * strip_side_clearance;
flange_w     = channel_w - 2 * flange_side_clearance;
flange_l     = clip_l;
clip_w       = channel_w + 2 * clip_wall_t;
// In clip coordinates (plate underside at z = 0).
hook_z       = clip_plate_t + strip_t + strip_fit + flange_t;
clip_wall_h  = hook_z + hook_h;
nub_z        = board_top_z + button_h + nub_clearance;
tongue_z     = lid_top_z - tongue_t;
tongue_x0    = button_pos[0] - 3;   // free end, just past the nub

// Board outline: a disc with the lanyard notch cut out.
module board_2d(grow = 0) {
    difference() {
        circle(d = board_d + 2 * grow);
        translate(notch_pos) circle(r = notch_r - grow);
    }
}

module tongue_2d() {
    translate([tongue_x0, button_pos[1] - tongue_w / 2]) square([tongue_len, tongue_w]);
}

// U-shaped slot around the tongue, open at the hinge end (+X).
module tongue_slot_2d() {
    difference() {
        offset(delta = slot_w) tongue_2d();
        tongue_2d();
        translate([tongue_x0 + tongue_len, -50]) square(100);
    }
}

module body() {
    difference() {
        union() {
            difference() {
                translate([-flange_w / 2, -flange_l / 2, 0]) cube([flange_w, flange_l, flange_t]);
                // corner notches for the clip's end posts
                for (sx = [-1, 1], sy = [-1, 1])
                    translate([sx * flange_w / 2, sy * flange_l / 2, flange_t / 2])
                        cube([2 * (post_d + 0.2), 2 * (post_len + 0.2), flange_t + 1], center = true);
            }
            cylinder(r = cup_r, h = rim_z);
            translate([-cup_r - key_d, -key_w / 2, flange_t]) cube([key_d + 1, key_w, rim_z - flange_t]);
        }
        // board pocket, keyed by the notch
        translate([0, 0, flange_t]) linear_extrude(rim_z) board_2d(board_clearance);
        // snap groove
        translate([0, 0, rim_z - snap_from_rim]) rotate_extrude()
            polygon([[cup_r - snap_groove, 0], [cup_r + key_d + 1, -(snap_groove + key_d + 1)],
                     [cup_r + key_d + 1, snap_groove + key_d + 1]]);
        // finger slot to lift the board out
        translate([pocket_r - 1, -2, board_top_z - 1.5]) cube([wall_t + 2, 4, lid_gap + 2]);
    }
}

module lid_installed() {
    union() {
        difference() {
            union() {
                translate([0, 0, rim_z]) cylinder(r = skirt_r_out, h = lid_top_t);
                translate([0, 0, rim_z - skirt_h]) difference() {
                    cylinder(r = skirt_r_out, h = skirt_h + eps);
                    translate([0, 0, -eps]) cylinder(r = skirt_r_in, h = skirt_h + 3 * eps);
                }
                translate([0, 0, rim_z - snap_from_rim]) rotate_extrude()
                    polygon([[skirt_r_in + eps, -0.5], [skirt_r_in - snap_bead, 0], [skirt_r_in + eps, 0.5]]);
            }
            // slot for the body's key rib
            translate([-skirt_r_out - 1, -(key_w / 2 + 0.3), rim_z - skirt_h - eps])
                cube([skirt_r_out + 1 - skirt_r_in + 0.5, key_w + 0.6, skirt_h + eps]);
            // pry notch for opening
            translate([skirt_r_in - 0.5, -2.5, rim_z - skirt_h - eps]) cube([skirt_t + 1, 5, 1.2]);
            // button tongue: slot through the top, thinned from inside
            translate([0, 0, rim_z - 1]) linear_extrude(lid_top_t + 2) tongue_slot_2d();
            translate([0, 0, rim_z - eps]) linear_extrude(tongue_z - rim_z + eps) tongue_2d();
        }
        // nub under the tongue
        translate([button_pos[0], button_pos[1], nub_z]) cylinder(d = nub_d, h = tongue_z - nub_z + eps);
        // ring pressing the board's outer edge onto the battery holder
        translate([0, 0, board_top_z]) linear_extrude(lid_gap + eps) difference() {
            circle(r = clamp_rib[1]);
            circle(r = clamp_rib[0]);
            translate(notch_pos) circle(r = notch_r + 0.3);
        }
    }
}

module clip() {
    translate([-clip_w / 2, -clip_l / 2, 0]) cube([clip_w, clip_l, clip_plate_t]);
    for (m = [0, 1]) mirror([m, 0, 0]) {
        translate([channel_w / 2, -clip_l / 2, 0]) cube([clip_wall_t, clip_l, clip_wall_h]);
        // hook over the flange edge: flat underside, chamfered top as a lead-in
        translate([0, clip_l / 2, 0]) rotate([90, 0, 0]) linear_extrude(clip_l)
            polygon([[channel_w / 2 + eps, hook_z], [channel_w / 2 - hook_d, hook_z],
                     [channel_w / 2 + eps, hook_z + hook_h]]);
        for (sy = [-1, 1])
            translate([channel_w / 2 - post_d, sy > 0 ? clip_l / 2 - post_len : -clip_l / 2, hook_z - flange_t])
                cube([post_d + eps, post_len, flange_t]);
    }
}

module lid_print() {
    translate([0, 0, lid_top_z]) rotate([180, 0, 0]) lid_installed();
}

module assembly(explode = 0) {
    body();
    color("green", 0.8) translate([0, 0, flange_t]) {
        translate([0, 0, board_stack - 1]) linear_extrude(1) board_2d();
        color("silver") cylinder(d = 20, h = board_stack - 1);
    }
    translate([0, 0, explode]) color("white") lid_installed();
    color("tan", 0.5) translate([-strip_w / 2, -40, -strip_t]) cube([strip_w, 80, strip_t]);
    translate([0, 0, -strip_t - clip_plate_t - strip_fit - explode]) color("orange") clip();
}

if (part == "body") body();
else if (part == "lid") lid_print();
else if (part == "clip") clip();
else if (part == "section") intersection() {
    assembly();
    translate([-40, button_pos[1], -20]) cube([80, 40, 40]);
}
else assembly(8);

echo(str("height above strip ", lid_top_z, " mm, below strip ", clip_plate_t,
         " mm; lid diameter ", 2 * skirt_r_out, " mm; nub length ", tongue_z - nub_z, " mm"));
