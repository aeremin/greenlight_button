// Case for the Radioland nRF52832 coin-cell beacon, mounted on a strip
// (default 50 mm wide, 5 mm thick). No screws or other hardware.
//
// Three parts, each printable without supports:
//   body  - cup holding the board battery-side down, on a flange that rests
//           on the strip. Each long edge of the flange is a 45-degree
//           dovetail tongue.
//   lid   - snaps onto the body; a flexible tongue with a nub presses the
//           board's button
//   clamp - plate under the strip with a rigid rail along each edge of the
//           strip. The rails have dovetail slots for the body's tongues.
//
// The clamp goes under the strip and the body slides along the strip into
// the slots. Nothing flexes to hold the body down: it is held by the
// dovetails in solid rails. Along the strip, a cap on the body stops it at
// one end of the clamp, and two spring prongs cut into the flange edges snap
// out behind the other end. To remove the body, squeeze the prong tips
// together and slide it back out.
//
// The top of the rails is flush with the flange, so apart from the cup and
// lid the top is flat.
//
// Coordinates: board centre at the origin, component side up, lanyard notch
// towards -X. The strip runs along Y; z = 0 is the flange underside and the
// strip's top face. The body slides in from +Y; the prongs latch at -Y.
//
// Render one part at a time, e.g.
//   openscad -D 'part="body"' -o stl/body.stl beacon_case.scad
// Parts: body, lid, clamp (each in print orientation), assembly, section.

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
strip_squeeze   = 0.3;    // how much the strip is squeezed between flange and clamp plate

/* [Body] */
floor_t         = 2.0;    // pocket floor
deck_t          = 3.5;    // flange thickness, flush with the rail tops
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

/* [Mount] */
mount_l         = 30;     // clamp length along the strip
plate_t         = 2.5;
strip_side_clearance = 0.3;
dovetail_h      = 2.0;    // height of the 45-degree tongue; it reaches this far under the rail
rail_w          = 3.0;    // rail material outside the dovetail
slide_clearance = 0.2;    // across the dovetail faces (horizontal)
end_clearance   = 0.1;    // along the strip, at the cap and at the prong hooks
cap_l           = 3;      // body overhang at +Y that stops it against the clamp end
chamfer         = 1.0;
corner_r        = 2;
entry_flare     = 0.8;    // lead-in at the clamp's +Y end
entry_flare_l   = 3;

/* [Prongs] */
prong_len       = 24;     // free length of each spring prong
prong_w         = 1.0;    // full-height part of the prong inside the tongue
prong_slot_w    = 2.0;    // room for the prong to flex inward; limits over-squeezing
hook_d          = 1.5;    // how far each hook sticks out past the slot wall
hook_l          = 4;      // ramp length on the hook's leading side

$fn = 96;
eps = 0.01;

pocket_r     = board_d / 2 + board_clearance;
cup_r        = pocket_r + wall_t;
board_top_z  = floor_t + board_stack;
rim_z        = board_top_z + lid_gap;
lid_top_z    = rim_z + lid_top_t;
skirt_r_in   = cup_r + lid_clearance;
skirt_r_out  = skirt_r_in + skirt_t;
nub_z        = board_top_z + button_h + nub_clearance;
tongue_z     = lid_top_z - tongue_t;
tongue_x0    = button_pos[0] - 3;   // free end, just past the nub

x_ch         = strip_w / 2 + strip_side_clearance;   // rail inner face
x_out        = x_ch + dovetail_h + rail_w;           // outer edge of clamp and cap
x_tongue     = x_ch + dovetail_h - slide_clearance;  // body tongue's outer bottom edge
x_slot_out   = x_tongue - dovetail_h - prong_w;      // outer wall of the prong slot
// The strip pushes the body up until the dovetails meet, which takes up the
// slide clearance; the plate is set so the strip is then squeezed by strip_squeeze.
plate_top_z  = slide_clearance - strip_t + strip_squeeze;
plate_z      = plate_top_z - plate_t;
y_face       = -mount_l / 2 - end_clearance;         // prong hooks' catch face
y_min        = y_face - hook_l;                      // body's -Y end
y_cap0       = mount_l / 2 + end_clearance;
y_cap1       = y_cap0 + cap_l;

// Extrudes a 2D profile in the XZ plane along Y, from y0 to y1.
module y_extrude(y0, y1) {
    translate([0, y1, 0]) rotate([90, 0, 0]) linear_extrude(y1 - y0) children();
}

module mirror_x() {
    children();
    mirror([1, 0]) children();
}

// Board outline: a disc with the lanyard notch cut out.
module board_2d(grow = 0) {
    difference() {
        circle(d = board_d + 2 * grow);
        translate(notch_pos) circle(r = notch_r - grow);
    }
}

module rounded_rect_2d(x0, x1, y0, y1) {
    translate([x0, y0]) offset(r = corner_r) offset(delta = -corner_r) square([x1 - x0, y1 - y0]);
}

// Body flange cross-section (XZ), with a 45-degree tongue on each edge.
module flange_profile() {
    mirror_x() polygon([[0, 0], [x_tongue, 0], [x_tongue - dovetail_h, dovetail_h],
                        [x_tongue - dovetail_h, deck_t], [0, deck_t]]);
}

// The tongue on the +X edge alone, shifted outward by d.
module tongue_profile(d) {
    translate([d, 0]) polygon([[x_tongue - dovetail_h - 0.5, 0], [x_tongue, 0],
                               [x_tongue - dovetail_h, dovetail_h], [x_tongue - dovetail_h - 0.5, dovetail_h]]);
}

// Full-width block with chamfered top corners, for the cap.
module cap_profile() {
    polygon([[-x_out, 0], [x_out, 0], [x_out, deck_t - chamfer], [x_out - chamfer, deck_t],
             [-(x_out - chamfer), deck_t], [-x_out, deck_t - chamfer]]);
}

// Clamp cross-section (XZ): plate, strip channel, and the rails with their slots.
module clamp_profile() {
    difference() {
        polygon([[-(x_out - chamfer), plate_z], [x_out - chamfer, plate_z], [x_out, plate_z + chamfer],
                 [x_out, deck_t - chamfer], [x_out - chamfer, deck_t],
                 [-(x_out - chamfer), deck_t], [-x_out, deck_t - chamfer], [-x_out, plate_z + chamfer]]);
        mirror_x() polygon([[-eps, plate_top_z], [x_ch, plate_top_z], [x_ch, 0], [x_ch + dovetail_h, 0],
                            [x_ch, dovetail_h], [x_ch, deck_t + 1], [-eps, deck_t + 1]]);
    }
}

// Slot in the clamp rail for one tongue (+X side), grown by d.
module slot_profile(d) {
    offset(delta = d) polygon([[x_ch - 1, 0], [x_ch + dovetail_h, 0], [x_ch, dovetail_h],
                               [x_ch, deck_t + 1], [x_ch - 1, deck_t + 1]]);
}

module tongue_slot_2d() {
    difference() {
        offset(delta = slot_w) button_tongue_2d();
        button_tongue_2d();
        translate([tongue_x0 + tongue_len, -50]) square(100);
    }
}

module button_tongue_2d() {
    translate([tongue_x0, button_pos[1] - tongue_w / 2]) square([tongue_len, tongue_w]);
}

module body() {
    difference() {
        union() {
            y_extrude(y_min, y_cap0 + eps) flange_profile();
            intersection() {
                y_extrude(y_cap0, y_cap1) cap_profile();
                linear_extrude(deck_t) rounded_rect_2d(-x_out, x_out, y_cap0 - corner_r - 1, y_cap1);
            }
            cylinder(r = cup_r, h = rim_z);
            translate([-cup_r - key_d, -key_w / 2, 0]) cube([key_d + 1, key_w, rim_z]);
            // prong hooks: ramp up from the tip to a square catch face
            mirror_x() hull() {
                y_extrude(y_face - eps, y_face) tongue_profile(hook_d);
                y_extrude(y_min, y_min + eps) tongue_profile(0);
            }
        }
        // board pocket, keyed by the notch
        translate([0, 0, floor_t]) linear_extrude(rim_z) board_2d(board_clearance);
        // snap groove
        translate([0, 0, rim_z - snap_from_rim]) rotate_extrude()
            polygon([[cup_r - snap_groove, 0], [cup_r + key_d + 1, -(snap_groove + key_d + 1)],
                     [cup_r + key_d + 1, snap_groove + key_d + 1]]);
        // finger slot to lift the board out
        translate([pocket_r - 1, -2, board_top_z - 1.5]) cube([wall_t + 2, 4, lid_gap + 2]);
        // slots that free the prongs, open at the -Y end
        mirror_x() translate([x_slot_out - prong_slot_w, y_min - 1, -1])
            cube([prong_slot_w, prong_len + 1, deck_t + 2]);
        // lead-in on the leading bottom edge, to ride up onto the squeezed strip
        translate([0, y_min, 0]) rotate([45, 0, 0]) cube([2 * x_out, 2 * chamfer / sqrt(2), 2 * chamfer / sqrt(2)], center = true);
        // chamfers on the top edges at both ends
        for (y = [y_min, y_cap1]) translate([0, y, deck_t]) rotate([45, 0, 0])
            cube([2 * x_out + 2, 2 * chamfer / sqrt(2), 2 * chamfer / sqrt(2)], center = true);
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
            translate([0, 0, rim_z - eps]) linear_extrude(tongue_z - rim_z + eps) button_tongue_2d();
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

// In body coordinates; clamp_print() moves it onto the bed.
module clamp() {
    difference() {
        intersection() {
            y_extrude(-mount_l / 2, mount_l / 2) clamp_profile();
            translate([0, 0, plate_z - 1]) linear_extrude(deck_t - plate_z + 2)
                rounded_rect_2d(-x_out, x_out, -mount_l / 2, mount_l / 2);
        }
        // lead-in where the body's tongues enter
        mirror_x() hull() {
            y_extrude(mount_l / 2 - entry_flare_l, mount_l / 2 - entry_flare_l + eps) slot_profile(0);
            y_extrude(mount_l / 2, mount_l / 2 + 1) slot_profile(entry_flare);
        }
    }
}

module clamp_print() {
    translate([0, 0, -plate_z]) clamp();
}

module lid_print() {
    translate([0, 0, lid_top_z]) rotate([180, 0, 0]) lid_installed();
}

// Mounted: the strip has pushed the body up against the dovetails.
module assembly(explode = 0) {
    translate([0, 0, slide_clearance]) {
        body();
        color("green", 0.8) translate([0, 0, floor_t]) {
            translate([0, 0, board_stack - 1]) linear_extrude(1) board_2d();
            color("silver") cylinder(d = 20, h = board_stack - 1);
        }
        translate([0, 0, explode]) color("white") lid_installed();
    }
    color("tan", 0.5) translate([-strip_w / 2, -45, plate_top_z])
        cube([strip_w, 90, slide_clearance - plate_top_z]);
    translate([0, explode, -explode]) color("orange") clamp();
}

if (part == "body") body();
else if (part == "lid") lid_print();
else if (part == "clamp") clamp_print();
else if (part == "section") intersection() {
    assembly();
    translate([-40, button_pos[1], -20]) cube([80, 40, 40]);
}
else assembly(8);

echo(str("height above strip ", lid_top_z + slide_clearance, " mm, below strip ", plate_t,
         " mm; clamp ", 2 * x_out, " x ", mount_l, " mm; body ", 2 * x_out, " x ", y_cap1 - y_min,
         " mm; lid diameter ", 2 * skirt_r_out, " mm; nub length ", tongue_z - nub_z, " mm"));
