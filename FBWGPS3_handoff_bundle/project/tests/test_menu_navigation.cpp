#include "menu_navigation.hpp"

#include <iostream>
#include <string>
#include <vector>

int main() {
	fw::MenuFocus focus;
	std::string error;
	const std::vector<fw::MenuItemLayout> items = {
		{"play", 100, 100, 120, 40, true, true},
		{"settings", 250, 100, 120, 40, true, true},
		{"credits", 100, 170, 120, 40, true, true},
		{"disabled", 250, 170, 120, 40, true, false},
		{"hidden", 100, 240, 120, 40, false, true}
	};
	if (!focus.setItems(items, &error) || focus.selectedId() != "play") {
		std::cerr << "initial focus failed: " << error << "\n"; return 1;
	}
	const auto moved_right = focus.update(fw::MenuEvent::Right);
	if (moved_right.selected_id != "settings" || moved_right.action != fw::MenuAction::SelectionChanged) {
		std::cerr << "right navigation failed\n"; return 1;
	}
	const auto moved_down = focus.update(fw::MenuEvent::Down);
	if (moved_down.selected_id != "credits") {
		// From settings, the disabled bottom-right item is skipped; the nearest enabled
		// candidate in the lower half-plane is credits (down-left).
		std::cerr << "disabled/hidden menu items were not skipped\n"; return 1;
	}
	if (focus.selectedId() != "credits") {
		std::cerr << "expected credits after moving down from settings\n"; return 1;
	}
	const auto activate = focus.update(fw::MenuEvent::Confirm);
	if (activate.action != fw::MenuAction::Activate || activate.activated_id != "credits") {
		std::cerr << "confirm did not activate selected item\n"; return 1;
	}
	const auto back = focus.update(fw::MenuEvent::Back);
	if (back.action != fw::MenuAction::Back) {
		std::cerr << "back event failed\n"; return 1;
	}
	fw::FocusRect rect;
	if (!focus.focusRect(rect, 4.0f) || rect.x != 96 || rect.y != 166 || rect.width != 128 || rect.height != 48) {
		std::cerr << "focus rectangle geometry failed\n"; return 1;
	}
	// Refreshing a menu should preserve focus when the same item remains selectable.
	if (!focus.setItems(items, &error) || focus.selectedId() != "credits") {
		std::cerr << "selection preservation failed\n"; return 1;
	}
	if (focus.setItems({{"bad", 0, 0, 0, 10, true, true}}, &error)) {
		std::cerr << "invalid menu rectangle should be rejected\n"; return 1;
	}
	if (!focus.setItems({{"only-disabled", 0, 0, 10, 10, true, false}}) || focus.hasSelection()) {
		std::cerr << "empty selectable menu should have no focus\n"; return 1;
	}
	std::cout << "PASS: initial focus, spatial movement, disabled filtering, confirm/back, focus bounds and refresh preservation\n";
	return 0;
}
