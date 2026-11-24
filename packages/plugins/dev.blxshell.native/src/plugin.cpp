// An example blxshell C++ plugin: a bar module drawn with vw, counting clicks
// in the plugin's store. Built against <blxshell/plugin.hpp> from the same
// blxshell, with the same compiler (blxshell checks the ABI tag).
#include <blxshell/plugin.hpp>

#include <format>
#include <string>

namespace {

class Clicks final : public blxshell::Plugin {
 public:
  std::vector<std::string> views() const override { return {"Bar"}; }

  vw::Widget view(vw::Ui& ui, std::string_view, blxshell::Host& host) override {
    const std::string stored = host.get("clicks");
    const int clicks = stored.empty() ? 0 : std::stoi(stored);
    // pill() builds the background first, then the label on it.
    return blxshell::pill(ui, host.colour("surfaceContainer"),
                          {.on_click = [&host, clicks](const vw::ClickEvent&) { host.set("clicks", std::to_string(clicks + 1)); }},
                          [&] {
                            return ui.text({.text = std::format("{} clicks from C++", clicks), .font = host.font("text"),
                                            .size = 13, .weight = vw::weight::medium, .color = host.colour("text")});
                          });
  }
};

}  // namespace

BLXSHELL_PLUGIN(Clicks)
