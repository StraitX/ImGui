#include "imgui.h"
#include "core/math/vector2.hpp"
#include "core/string_view.hpp"

class String;

namespace ImGui {
	
void DockspaceWindow(const char *name, Vector2s window_size);

bool InputText(const char* label, String& buffer, ImGuiInputTextFlags flags);

void Text(StringView text);

}//namespace ImGui::