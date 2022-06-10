board_size = [110.5, 33, 2];

board_top_height = 6.1;

board_batt_size = [77, 20.9, 19];
board_batt_pos = [14.5, 6.5];

board_sw_overflow = 2.2;
board_sw_x = 46.8;
board_sw_hole = [10, 3];

board_usb_overflow = 1.6;
board_usb_x = 49;
board_usb_hole = [10, 2];

board_sd_overflow = 9.5;
board_sd_x = 59;
board_sd_size = [11.5, 2.6];

board_holes_x = [2.67, board_size.x - 2.29];
board_holes_y = 2.59;
board_holes_dist = 27.71;
board_holes_dia = 2;

board_antennas_size = [2.8, 9, 1.5];
board_antennas_pos = [board_size.x - board_antennas_size.x, 6.9];

usb_sck_size = [7.3 - 2.4, 10.4, 4.5];
usb_sck_foot_size = [2.4, 2 * 4.8 + usb_sck_size.y, 4.5];

speaker_dia = 40;
speaker_dia2 = 20;
speaker_height = 18.7;

// ---------------------------------------------

board_h = speaker_height + 1.2;

left_inset = 1;

fatness = 1.2;
inset = .2;

DEBUG = true;
//DEBUG = false;

// ---------------------------------------------

inner_box = [134.5, 45.5, 30];

box = [inner_box.x + 2 * fatness, inner_box.y + 2 * fatness, inner_box.z + 2 * fatness];

echo("Box size ", box.x, " x ", box.y, " x ", box.z);
echo("Inner box size ", inner_box.x, " x ", inner_box.y, " x ", inner_box.z);

// ---------------------------------------------

module triangle(o_len, a_len, depth, center = false) {
    centroid = center ? [- a_len / 3, - o_len / 3, - depth / 2] : [0, 0, 0];
    translate(centroid) linear_extrude(height = depth)
        {
            polygon(points = [[0, 0], [a_len, 0], [0, o_len]], paths = [[0, 1, 2]]);
        }
}

module Board(x, y, z) {
    translate([x, y, z]) union() {
        difference() {
            color("black") cube(board_size);
            for (r = [0:1]) {
                translate([board_holes_x[0], board_holes_y + r * board_holes_dist, - .01])
                    cylinder(d = board_holes_dia, h = 10, $fn = 10);
                translate([board_holes_x[1], board_holes_y + r * board_holes_dist, - .01])
                    cylinder(d = board_holes_dia, h = 10, $fn = 10);
            }
        }

        color("gray") {
            translate([board_sw_x, - board_sw_overflow, board_size.z]) cube([board_sw_hole[0], board_sw_overflow, board_sw_hole[1]]);

            translate([board_usb_x, board_size.y, board_size.z]) cube([board_usb_hole[0], board_usb_overflow, board_usb_hole[1]]);
            translate([board_sd_x, board_size.y, board_size.z]) cube([board_sd_size[0], board_sd_overflow, board_sd_size[1]]);
        }

        color("gray") translate([5, 5, board_size.z - .01]) cube([board_size.x - 10, board_size.y - 10, board_top_height]);

        color("gray") translate([board_batt_pos.x, board_batt_pos.y, - board_batt_size.z + .01]) {
            cube(board_batt_size);
        }

        color("orange") translate([board_antennas_pos.x, board_antennas_pos.y, board_size.z]) cube(board_antennas_size);
    }
}

module Speaker(x, y, z) {
    bottom_height = 12;
    translate([x + speaker_dia / 2, y + speaker_dia / 2, z]) color("black") {
        cylinder(h = bottom_height, d = speaker_dia, $fn = 60);
        translate([0, 0, bottom_height]) cylinder(h = speaker_height - bottom_height, d = speaker_dia2, $fn = 60);
    }
}

module BoardCol(x, y, z, h) {
    side = [6, 7];
    hole_dia = 2.5;

    difference() {
        translate([x - (side.x / 2), y - (side.y / 2), z]) {
            color("blue") cube([side.x, side.y, h]);
        }
        translate([x, y, z - .01]) {
            color("green") cylinder(d = hole_dia, h = h + .02, $fn = 15);
        }
    }
}

module UsbSocket(x, y, z) {
    translate([x, y, z]) union() color("black") {
        cube(usb_sck_foot_size);
        translate([usb_sck_foot_size.x, (usb_sck_foot_size.y - usb_sck_size.y) / 2]) cube(usb_sck_size);
    }
}

// ---------------------------------------------

// bottom
difference() {
    // main box
    union() {
        difference() {
            cube(box);
            translate([fatness, fatness, fatness]) {
                cube([inner_box.x, inner_box.y, box.z]);
            }

            // switch hole
            translate([fatness + inset + left_inset + board_sw_x, - .01, fatness + board_h + board_size.z + 1]) // +1 = correction
                cube([board_sw_hole[0], 10, board_sw_hole[1]]);

            // USB socket hole
            translate([box.x - usb_sck_size.x - usb_sck_foot_size.x, (box.y - usb_sck_size.y) / 2 - .3, box.z - usb_sck_foot_size.z - .4]) {
                cube([100, 11, 100]);
            }

//            // DEBUG - front wall
//            translate([fatness, - .01, fatness]) cube([inner_box.x, fatness + .02, 100]);
//            // DEBUG - right wall
//            translate([box.x - fatness - .01, fatness, fatness]) cube([100, inner_box.y, 100]);
        }

        // USB socket
        if (DEBUG) {
            UsbSocket(box.x - usb_sck_size.x - usb_sck_foot_size.x, (box.y - usb_sck_foot_size.y) / 2, box.z - usb_sck_foot_size.z - .2);
        }

        // the +1 is a space tolerance for the board
        translate([fatness + left_inset, fatness, fatness]) {
            translate([.2, .2]) {
                if (DEBUG) Board(0, 2, board_h);
                BoardCol(board_holes_x[0], 2 + board_holes_y, 0, board_h);
                BoardCol(board_holes_x[0], 2 + board_holes_y + board_holes_dist, 0, board_h);
            }

            // speaker holder
            translate([board_batt_pos.x + board_batt_size.x + 1, 0]) {
                spk_tolerance = .25;
                h = speaker_height + 1.2;

                if (DEBUG) Speaker(1, spk_tolerance, 0);

                translate([0, - fatness]) difference() {
                    cube([20, box.y, h]);
                    translate([- .01, fatness]) cube([100, box.y - 2 * fatness, speaker_height + .2]);
                }

                translate([0, speaker_dia + (spk_tolerance * 2)]) cube([30, 1, h]);
            }

            // board corner-holders
            translate([board_size.x - 14.5, 0]) {
                height_tolerance = .3;

                translate([0, 0, speaker_height + 1.2 - .01]) difference() {
                    translate([16.5, 0]) rotate([0, 0, 90]) triangle(8, 8, 4);
                    translate([8, 1.8]) cube([7.5, 10, board_size.z + height_tolerance]);
                }
                // this is a bit wild... I hope I'll never have to rework this :-|
                translate([0, 37, speaker_height + 1.2 - .01]) difference() {
                    translate([16.5, 0]) rotate([0, 0, 180]) triangle(8, 8, 4);
                    translate([8, - 11.5]) cube([7.5, 10, board_size.z + height_tolerance]);
                }
            }

            // USB socket holder
            translate([inner_box.x - usb_sck_size.x - usb_sck_foot_size.x - 2 + inset,
                        (inner_box.y - usb_sck_foot_size.y) / 2 - 2,
                            inner_box.z - usb_sck_foot_size.z + fatness - 3]) difference() {
                main_cube = [usb_sck_foot_size.x + usb_sck_size.x + 2, usb_sck_foot_size.y + 4, usb_sck_foot_size.z + 3];

                // main cube
                cube(main_cube);

                // socket "hole"
                tolerance = .2;
                translate([2 - tolerance, 2 - tolerance, 3 - .3]) {
                    cube([usb_sck_foot_size.x + (tolerance*2), usb_sck_foot_size.y + (tolerance*2), 100]);
                    translate([usb_sck_foot_size.x, (usb_sck_foot_size.y - usb_sck_size.y) / 2])
                        cube([100, usb_sck_size.y + (tolerance*2), 100]);
                }

                // cables hole (to inside)
                translate([- .01, (main_cube.y - 8) / 2, 3 - .3]) {
                    cube([10, 8, 10]);
                }
            }

            // weird construction helpers (to support the USB socket holder printing)
            translate([board_batt_pos.x + board_batt_size.x + 20, (inner_box.y - 2) / 2, speaker_height + .2]) color("blue") {
                l = inner_box.x - (board_batt_pos.x + board_batt_size.x + 20);
                cube([l, 2, 2]);
            }
            translate([inner_box.x - 9.1, 0, speaker_height + 2]) color("blue") {
                cube([1.5, inner_box.y, 3 + .01]);
            }
        }

        // cols & wall connections
        translate([fatness - .01, 4, fatness]) cube([2, 4, board_h]);
        translate([fatness - .01, board_size.y - 1.25, fatness]) cube([2, 4, board_h]);
    }
}

//// cover
//translate([0, - 60])
//    //    translate([- fatness - inset / 2, 30 + box.y + fatness + inset / 2, box.z + fatness + inset]) rotate([180])
//    difference() {
//        size_inner = box;
//        size_outter = [box.x + 2 * fatness + inset, box.y + 2 * fatness + inset, box.z + fatness + inset];
//
//        // main box
//        union() {
//            difference() {
//                cube([size_outter.x, size_outter.y, size_outter.z]);
//                translate([fatness + inset / 2, fatness + inset / 2, fatness]) {
//                    cube([size_inner.x, size_inner.y, 100]);
//                }
//            }
//        }
//    }

