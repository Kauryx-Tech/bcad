#pragma once

namespace bcad::plugin {
class PluginRegistry;
}

namespace bcad::cadastre {
bool registerParcelSerializer(bcad::plugin::PluginRegistry& registry);
bool registerBoundarySerializer(bcad::plugin::PluginRegistry& registry);
bool registerSurveyMarkSerializer(bcad::plugin::PluginRegistry& registry);
bool registerEasementSerializer(bcad::plugin::PluginRegistry& registry);
}
