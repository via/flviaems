#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Dial.H>

#include "viaems.h"

int main(int argc, char **argv) {
   viaems::Model model;
   std::shared_ptr<viaems::Protocol> protocol;

  Fl_Window *window = new Fl_Window(340, 180);
  Fl_Dial *dial = new Fl_Dial(0, 0, 100, 100, "RPM");
  window->end();
  window->show(argc, argv);
  viaems::UsbConnection conn;
  std::cerr << "connect: " << conn.Connect() << std::endl;
  return Fl::run();
}

