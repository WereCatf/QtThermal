#include "core/CameraTypes.h"

#include <algorithm>
#include <cctype>

namespace qtthermal {

ModelConfig modelConfig(Model model)
{
    ModelConfig config;
    config.model = model;
    if (model == Model::P1) {
        config.pid = 0x45C2;
        config.sensorWidth = 160;
        config.sensorHeight = 120;
    } else {
        config.pid = 0x45A2;
        config.sensorWidth = 256;
        config.sensorHeight = 192;
    }
    return config;
}

bool parseModel(const std::string& name, Model& out)
{
    std::string lowered = name;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (lowered == "p1") {
        out = Model::P1;
        return true;
    }
    if (lowered == "p3") {
        out = Model::P3;
        return true;
    }
    return false;
}

std::string modelName(Model model)
{
    return model == Model::P1 ? "P1" : "P3";
}

} // namespace qtthermal
