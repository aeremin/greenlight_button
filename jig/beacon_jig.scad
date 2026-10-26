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
// Coordinates: board centre at the origin, component side up, lanyard notch
// towards -X.
//
// Render one part at a time, e.g.
//   openscad -D 'part="base"' -o stl/base.stl beacon_jig.scad
// Parts: base, lid, gauge, hole_test, assembly (preview only).

part = "assembly";

/* [Board] */
board_d       = 25.4;   // PCB diameter
board_t       = 1.0;    // PCB thickness
board_back_h  = 4.0;    // measured: battery holder (with cell, if fitted) below the PCB
notch_pos     = [-11.76, 0.21];
notch_r       = 3.21;   // radius of the lanyard notch in the board edge

/* [Pogo pins] (defaults suit P75-series pins, 1.02 mm barrel) */
pin_hole_d    = 1.1;    // print hole_test first and adjust for a snug press fit
pin_preload   = 1.2;    // how far each plunger is pushed in when the lid is down
lid_gap       = 3.0;    // clearance between PCB top and lid underside

/* [Jig] */
clearance     = 0.3;    // per side, for the board pocket and the pegs
floor_t       = 2.0;
lid_t         = 6.0;
size          = [46, 34];
corner_r      = 3;
peg_pos       = [[-18, 11], [18, -11]];
peg_d         = [5, 3.5];   // different sizes so the lid only fits one way
finger_w      = 6;
band_w        = 3;      // rubber-band grooves across lid top and base bottom
band_depth    = 1.0;
label_size    = 1.7;
label_depth   = 0.4;

// [x, y, pad number, label offset] for the guide's pad numbering:
// 1 GND, 2 VCC, 3 and 4 NFC, 5 SWCLK, 6 SWDIO.
pads = [
    [-10.05, -4.83, "1", [ 1.9, -0.6]],
    [ -9.79,  5.59, "2", [ 1.9, -0.6]],
    [  4.04,-10.89, "3", [-2.0,  0.9]],
    [  7.33, -8.88, "4", [-1.6,  1.0]],
    [  7.42, -3.94, "5", [-0.5,  1.1]],
    [  9.12, -3.93, "6", [ 1.2,  1.1]],
];

$fn = 64;
eps = 0.01;

pocket_depth = board_back_h + board_t + lid_gap;
base_h       = floor_t + pocket_depth;
pin_protrude = lid_gap + pin_preload;   // below the lid, plunger fully out

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
        translate([sx * 12.5 - band_w / 2, -size[1] / 2 - 1, z])
            cube([band_w, size[1] + 2, h]);
}

module base() {
    difference() {
        rounded_rect(size, corner_r, base_h);
        // board pocket, keyed by the notch
        translate([0, 0, floor_t])
            linear_extrude(pocket_depth + eps) board_2d(clearance);
        // finger slots to lift the board out
        for (sy = [-1, 1])
            translate([-finger_w / 2, sy > 0 ? 0 : -size[1] / 2 - 1, floor_t + board_back_h - 1])
                cube([finger_w, size[1] / 2 + 1, pocket_depth]);
        band_grooves(-eps, band_depth);
    }
    for (i = [0 : 1])
        translate([peg_pos[i][0], peg_pos[i][1], base_h - eps])
            cylinder(d = peg_d[i], h = lid_t);
}

module lid() {
    difference() {
        rounded_rect(size, corner_r, lid_t);
        for (i = [0 : 1])
            translate([peg_pos[i][0], peg_pos[i][1], -eps])
                cylinder(d = peg_d[i] + 2 * clearance, h = lid_t + 2 * eps);
        for (p = pads)
            translate([p[0], p[1], -eps])
                cylinder(d = pin_hole_d, h = lid_t + 2 * eps, $fn = 24);
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

// Spacer for setting pin height: stand the lid on it, push each pin down
// until its tip touches the table, then glue it.
module gauge() {
    difference() {
        rounded_rect(size, corner_r, pin_protrude);
        translate([0, 0, -eps])
            rounded_rect([size[0] - 12, size[1] - 8], corner_r, pin_protrude + 2 * eps);
    }
}

// Coupon for choosing pin_hole_d: holes from 0.9 to 1.3 mm.
module hole_test() {
    ds = [0.9, 1.0, 1.1, 1.2, 1.3];
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
else if (part == "gauge") gauge();
else if (part == "hole_test") hole_test();
else {
    base();
    color("green", 0.8) translate([0, 0, floor_t + board_back_h])
        linear_extrude(board_t) board_2d();
    color("silver") translate([0, 0, floor_t + board_back_h])
        linear_extrude(0.01) board_2d();
    translate([0, 0, base_h + 8]) lid();
    for (p = pads) color("gold")
        translate([p[0], p[1], base_h + 8 - pin_protrude]) cylinder(d = 0.7, h = 16.5, $fn = 12);
}

echo(str("base height ", base_h, " mm, pin protrusion below lid ", pin_protrude, " mm"));
