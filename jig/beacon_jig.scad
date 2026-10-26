// Pogo-pin programming jig for the Radioland nRF52832 coin-cell beacon.
//
// The board lies component side up in the base pocket, keyed by its lanyard
// notch. The lid slides down two pegs of different sizes, so it only fits one
// way round, and presses six pogo pins onto the test pads.
//
// Pad positions come from an-erd's laser-cut jig (Documentation/JIG/
// P1_beacon_jig_MOUNT.svg in github.com/an-erd/ble_beacon) and agree with
// Radioland's module diagram. Pad numbers match the reflashing guide.
//
// Pins are GA20-10025-0818 (see pogo_pin_dimensions.png), inserted tail up.
// Each flange sits in a seat in the lid, which sets the pin height. Pads 5
// and 6 are only 1.69 mm apart, closer than the 1.5 mm barrels allow, so
// those two pins lean apart by pair_tilt degrees to leave a wall between them.
//
// Coordinates: board centre at the origin, component side up, lanyard notch
// towards -X.
//
// Render one part at a time, e.g.
//   openscad -D 'part="base"' -o stl/base.stl beacon_jig.scad
// Parts: base, lid, hole_test, assembly (preview only).

part = "assembly";

/* [Board] */
board_d       = 25.0;   // measured PCB diameter
board_t       = 1.0;    // PCB thickness
board_back_h  = 4.0;    // measured: battery holder (with cell, if fitted) below the PCB
notch_pos     = [-11.76, 0.21];
notch_r       = 3.0;    // radius of the lanyard notch in the board edge (measured ~6 mm across)

/* [Pogo pins] (GA20-10025-0818) */
pin_tip_to_flange = 9.5;  // plunger tip (extended) to the flange's lower face
pin_plunger_len   = 2.5;  // plunger sticking out of the barrel, extended
pin_barrel_d      = 1.5;
pin_flange_d      = 2.0;
pin_flange_t      = 0.5;
pin_tail_d        = 0.8;
pin_tail_len      = 1.8;
pin_hole_d        = 1.6;  // print hole_test first and adjust for a snug press fit
pin_preload       = 0.8;  // how far each plunger is pushed in when the lid is down
pair_tilt         = 5;    // degrees each of pins 5 and 6 leans away from the other
lid_gap           = 3.0;  // clearance between PCB top and lid underside

/* [Jig] */
clearance     = 0.3;    // per side, for the flange seats
board_clearance = 0.15; // per side, for the board pocket and notch key
floor_t       = 2.0;
size          = [46, 34];
corner_r      = 3;
peg_pos       = [[-18, 11], [18, -11]];
peg_hole_d    = [5.6, 4.1]; // holes in the lid; different sizes so the lid only fits one way
peg_clearance = 0.15;   // per side; the pegs are this much smaller than their holes
finger_w      = 6;
band_x        = 13.3;   // rubber-band grooves across lid top and base bottom
band_w        = 3;
band_depth    = 1.0;
label_size    = 1.7;
label_depth   = 0.4;

// [x, y, pad number, label offset, tilt towards +X in degrees] for the guide's
// pad numbering: 1 GND, 2 VCC, 3 and 4 NFC, 5 SWCLK, 6 SWDIO.
pads = [
    [-10.05, -4.83, "1", [ 2.3, -0.3], 0],
    [ -9.79,  5.59, "2", [ 2.3, -0.3], 0],
    [  4.04,-10.89, "3", [-2.4,  0.6], 0],
    [  7.33, -8.88, "4", [-1.9,  1.5], 0],
    [  7.42, -3.94, "5", [-0.9,  2.4], -pair_tilt],
    [  9.12, -3.93, "6", [ 0.9,  2.4],  pair_tilt],
];

$fn = 64;
eps = 0.01;

pocket_depth = board_back_h + board_t + lid_gap;
base_h       = floor_t + pocket_depth;
// The lid top is level with the flange seat of an upright pin, whose
// extended tip reaches pin_preload below the PCB surface.
lid_t        = pin_tip_to_flange - pin_preload - lid_gap;

module rounded_rect(sz, r, h) {
    linear_extrude(h)
        offset(r) offset(-r) square(sz, center = true);
}

// Board outline: a disc with the lanyard notch cut out.
module board_2d(grow = 0) {
    difference() {
        circle(d = board_d + 2 * grow);
        translate(notch_pos) circle(r = notch_r - grow);
    }
}

module band_grooves(z, h) {
    for (sx = [-1, 1])
        translate([sx * band_x - band_w / 2, -size[1] / 2 - 1, z])
            cube([band_w, size[1] + 2, h]);
}

// Places children in a pin's frame: origin at the extended tip, +Z up the pin.
// z_tip is the tip height in the caller's coordinates.
module pin_frame(p, z_tip) {
    translate([p[0], p[1], z_tip]) rotate([0, p[4], 0]) children();
}

module pin_cavity() {
    translate([0, 0, -1])
        cylinder(d = pin_hole_d, h = pin_tip_to_flange + 1 + eps, $fn = 24);
    translate([0, 0, pin_tip_to_flange])
        cylinder(d = pin_flange_d + 2 * clearance, h = 10, $fn = 32);
}

module pin_model() {
    cylinder(d = 0.9, h = pin_plunger_len, $fn = 16);
    translate([0, 0, pin_plunger_len])
        cylinder(d = pin_barrel_d, h = pin_tip_to_flange - pin_plunger_len, $fn = 16);
    translate([0, 0, pin_tip_to_flange])
        cylinder(d = pin_flange_d, h = pin_flange_t, $fn = 16);
    translate([0, 0, pin_tip_to_flange + pin_flange_t])
        cylinder(d = pin_tail_d, h = pin_tail_len, $fn = 16);
}

module base() {
    difference() {
        rounded_rect(size, corner_r, base_h);
        // board pocket, keyed by the notch
        translate([0, 0, floor_t])
            linear_extrude(pocket_depth + eps) board_2d(board_clearance);
        // finger slots to lift the board out
        for (sy = [-1, 1])
            translate([-finger_w / 2, sy > 0 ? 0 : -size[1] / 2 - 1, floor_t + board_back_h - 1])
                cube([finger_w, size[1] / 2 + 1, pocket_depth]);
        band_grooves(-eps, band_depth);
    }
    for (i = [0 : 1])
        translate([peg_pos[i][0], peg_pos[i][1], base_h - eps])
            cylinder(d = peg_hole_d[i] - 2 * peg_clearance, h = lid_t);
}

module lid() {
    difference() {
        rounded_rect(size, corner_r, lid_t);
        for (i = [0 : 1])
            translate([peg_pos[i][0], peg_pos[i][1], -eps])
                cylinder(d = peg_hole_d[i], h = lid_t + 2 * eps);
        for (p = pads)
            pin_frame(p, -(lid_gap + pin_preload)) pin_cavity();
        // pad numbers and a notch marker on top
        translate([0, 0, lid_t - label_depth]) linear_extrude(label_depth + eps) {
            for (p = pads)
                translate([p[0] + p[3][0], p[1] + p[3][1]])
                    text(p[2], size = label_size, halign = "center", valign = "center",
                         font = "Liberation Sans:style=Bold");
            translate([-size[0] / 2 + 2.5, 0])
                polygon([[0, 0], [2.5, 1.6], [2.5, -1.6]]);
            difference() {
                circle(d = board_d + 1.2);
                circle(d = board_d + 0.4);
                translate(notch_pos) circle(r = notch_r + 0.4);
            }
        }
        band_grooves(lid_t - band_depth, band_depth + eps);
    }
}

// Coupon for choosing pin_hole_d: holes from 1.4 to 1.8 mm.
module hole_test() {
    ds = [1.4, 1.5, 1.6, 1.7, 1.8];
    difference() {
        translate([-16, -5, 0]) cube([32, 10, lid_t]);
        for (i = [0 : len(ds) - 1]) {
            translate([-12 + i * 6, 1.5, -eps]) cylinder(d = ds[i], h = lid_t + 2 * eps, $fn = 24);
            translate([-12 + i * 6, -2.8, lid_t - label_depth])
                linear_extrude(label_depth + eps)
                    text(str(ds[i] * 10), size = 2.2, halign = "center", valign = "center",
                         font = "Liberation Sans:style=Bold");
        }
    }
}

if (part == "base") base();
else if (part == "lid") lid();
else if (part == "hole_test") hole_test();
else {
    lift = 8;
    base();
    color("green", 0.8) translate([0, 0, floor_t + board_back_h])
        linear_extrude(board_t) board_2d();
    translate([0, 0, base_h + lift]) lid();
    for (p = pads) color("gold")
        pin_frame(p, base_h + lift - lid_gap - pin_preload) pin_model();
}

echo(str("base height ", base_h, " mm, lid thickness ", lid_t, " mm"));
