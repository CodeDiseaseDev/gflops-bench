#pragma once

#include <functional>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct ParamSpec {
  std::string name, shortened_alias;
  bool takesValue;
};

class ArgParser {
public:
  void register_parameter(const ParamSpec& spec) {
    specs[spec.name] = spec;
  }

  std::optional<ParamSpec> find(const std::string& name) {
    for (const auto& s : specs) {
      if (s.second.name == name ||
          s.second.shortened_alias == name) {
        return s.second;
      }
    }

    return std::nullopt;
  }

  void parse(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
      std::string arg = argv[i];
      std::optional<ParamSpec> found = find(arg);
      if (found) {

        const auto spec = found.value();

        if (spec.takesValue) {
          if (i + 1 < argc) {
            parsed[arg] = argv[++i];
          } else {
            std::cerr << "Missing value for parameter: " << arg << "\n";
          }
        } else {
          parsed[arg] = "true"; // just mark presence
        }
      } else {
        extras.push_back(arg);
      }
    }


  }

  std::optional<std::string> get(const std::string& name) const {
    auto it = parsed.find(name);
    if (it != parsed.end()) return it->second;
    return std::nullopt;
  }

  const std::vector<std::string>& get_extras() const {
    return extras;
  }

private:
  std::unordered_map<std::string, ParamSpec> specs;
  std::unordered_map<std::string, std::string> parsed;
  std::vector<std::string> extras;
};