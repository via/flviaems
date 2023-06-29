#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Dial.H>

#include "viaems.h"

static void updates(void *_ptr) {
  viaems::Protocol* proto = (viaems::Protocol *)_ptr;
  proto->NewData();
  auto chunk = proto->FeedUpdates();
  std::cerr << "got: " << chunk.points.size() << " points (";
  for (auto key : chunk.keys) {
    std::cerr << key << " ";
  }
  std::cerr << ")" << std::endl;
  Fl::repeat_timeout(0.05, updates, _ptr);
}


int main(int argc, char **argv) {
   viaems::Model model;
   std::shared_ptr<viaems::Protocol> protocol;

  Fl_Window *window = new Fl_Window(340, 180);
  Fl_Dial *dial = new Fl_Dial(0, 0, 100, 100, "RPM");
  window->end();
  window->show(argc, argv);
  std::unique_ptr<viaems::UsbConnection> conn = std::make_unique<viaems::UsbConnection>();
  std::cerr << "connect: " << conn->Connect() << std::endl;
  protocol = std::make_shared<viaems::Protocol>(std::move(conn));
  Fl::add_timeout(0.05, updates, &*protocol);
  return Fl::run();
}

