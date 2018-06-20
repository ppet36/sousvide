$fn=30;

module roundedRect(size, radius) {
  x = size[0];
  y = size[1];
  z = size[2];

  translate([0,0,-z/2]) linear_extrude (height=z) hull($fn=50) {
    translate([(-x/2)+(radius/2), (-y/2)+(radius/2), 0])
    circle(r=radius);

    translate([(x/2)-(radius/2), (-y/2)+(radius/2), 0])
    circle(r=radius);

    translate([(-x/2)+(radius/2), (y/2)-(radius/2), 0])
    circle(r=radius);

    translate([(x/2)-(radius/2), (y/2)-(radius/2), 0])
    circle(r=radius);
  }
}

module eheimPump() {
  union() {
    cube ([42, 34, 28], center=true);
    translate([-28,0,-20]) rotate([0,90,0]) cylinder (d=12, h=15, center=true, $fn=50);
    translate([15,9.5,28]) cube ([12, 15, 30], center=true);
    translate([0,0,-25]) cube ([42, 34, 22], center=true);
    translate([28,0,-31]) cube ([15,34,10], center=true);
    translate([-20,0,-33]) rotate([90,0,0]) cylinder (d=50, h=34, center=true, $fn=100);
  }
}

module heater() {
  union() {
   hull() {
     translate([0,0,20]) cube ([10, 20, 1], center=true);
     translate([0,0,-20]) cube ([10, 24.5, 1], center=true);
   }
   translate([0,0,30]) cylinder (d=7, h=20, center=true, $fn=30);
   translate([0,0,-45.5]) cube ([14,29,50], center=true);
  }
}

module toCut() {
  translate([-30,0,0]) {
    translate([-5,0,13]) rotate([0,-15,0]) heater();
    translate([53,0,-5]) eheimPump();
    //translate([55,25,0]) cube([5,2,200], center=true);
    //translate([55,-25,0]) cube([5,2,200], center=true);
    translate([20,12,-25]) ds18b20();
    translate([20,-25,-25]) cylinder (d=3.2, h=200, center=true, $fn=30);
    translate([20,25,-25]) cylinder (d=3.2, h=200, center=true, $fn=30);
  }
}

module holderBox() {
  difference() {
    roundedRect ([90, 60, 60], 10); 
    toCut();
  
    translate([-43,-27,21]) cylinder (d=2, h=20, center=true);
    translate([-43,27,21]) cylinder (d=2, h=20, center=true);
    translate([43,-27,21]) cylinder (d=2, h=20, center=true);
    translate([43,27,21]) cylinder (d=2, h=20, center=true);
  }
}

module ds18b20() {
  union() {
    cylinder (d=6, h=35, center=true, $fn=50);
    translate([0,0,25]) cylinder (d=6.5, h=15, center=true, $fn=50);
    translate([0,0,57]) cylinder (d=6.5, h=50, center=true, $fn=50);
  }
}

module vetrani() {
  hull() {
    cylinder (d=3.2, h=100, center=true);
    translate([0,15,0]) cylinder (d=3.2, h=100, center=true);
  }
}

module okraj() {
  difference() {
    union() {
      translate([0,0,29.25]) roundedRect ([92, 62, 1.5], 10);
      difference() {
        roundedRect([92, 62, 60], 11);
        roundedRect([90.3, 60.3, 60.1], 10);
      }
    }
    
    translate([-45+55.8,-30+5,0]) cylinder (d=6, h=200, center=true);
    translate([-45+66.04,-30+5,0]) cylinder (d=6, h=200, center=true);
    translate([0,0,-20]) rotate([90,0,0]) cylinder (d=3.2, h=200, center=true);
    translate([0,0,-20]) rotate([90,0,90]) cylinder (d=3.2, h=200, center=true);
    translate([50,15,20]) rotate([90,0,90]) cylinder (d=8, h=50, center=true);
    
    translate([-40,-20,0]) vetrani();
    translate([-40+6,-20,0]) vetrani();
    translate([-40+12,-20,0]) vetrani();
    translate([-40+18,-20,0]) vetrani();
    translate([-40+24,-20,0]) vetrani();
  }
}

module vyrez() {
   rotate([0,-90,0]) 
   hull() {
     cube ([30,30,5], center=true);
     translate([20,0,0]) cylinder (d=30, h=5, center=true);
   }  
}

holderBox();
translate([0,0,75]) okraj();

