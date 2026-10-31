#pragma once

#include "input.hpp"

#include <cmath>
#include <set>
#include <string>
#include <vector>

namespace fw {

struct MenuItemLayout {
	std::string id;
	float x = 0.0f;
	float y = 0.0f;
	float width = 1.0f;
	float height = 1.0f;
	bool visible = true;
	bool enabled = true;
};

struct FocusRect {
	float x = 0.0f;
	float y = 0.0f;
	float width = 0.0f;
	float height = 0.0f;
};

enum class MenuAction { None, SelectionChanged, Activate, Back };

struct MenuNavigationResult {
	MenuAction action = MenuAction::None;
	std::string selected_id;
	std::string activated_id;
};

// Spatial focus navigation for the original pointer-only menu screens.
// Feed it Input::menuEvent() and draw focusRect() with the platform renderer.
class MenuFocus {
public:
	bool setItems(const std::vector<MenuItemLayout>& items, std::string* error = nullptr) {
		std::set<std::string> ids;
		for (const MenuItemLayout& item : items) {
			if (item.id.empty() || !std::isfinite(item.x) || !std::isfinite(item.y) ||
				!std::isfinite(item.width) || !std::isfinite(item.height) ||
				item.width <= 0.0f || item.height <= 0.0f) {
				if (error) *error = "menu item has an invalid id or rectangle";
				return false;
			}
			if (!ids.insert(item.id).second) {
				if (error) *error = "menu item IDs must be unique";
				return false;
			}
		}
		std::string previous = selectedId();
		items_ = items;
		if (!isSelectable(previous)) selectFirst();
		if (error) error->clear();
		return true;
	}

	MenuNavigationResult update(MenuEvent event) {
		MenuNavigationResult result;
		const std::string before = selectedId();
		switch (event) {
			case MenuEvent::Up: move(0.0f, -1.0f); break;
			case MenuEvent::Down: move(0.0f, 1.0f); break;
			case MenuEvent::Left: move(-1.0f, 0.0f); break;
			case MenuEvent::Right: move(1.0f, 0.0f); break;
			case MenuEvent::Confirm:
				if (!selected_id_.empty() && isSelectable(selected_id_)) {
					result.action = MenuAction::Activate;
					result.activated_id = selected_id_;
				}
				break;
			case MenuEvent::Back:
				result.action = MenuAction::Back;
				break;
			case MenuEvent::None: break;
		}
		if (result.action == MenuAction::None && selectedId() != before)
			result.action = MenuAction::SelectionChanged;
		result.selected_id = selectedId();
		return result;
	}

	const std::string& selectedId() const { return selected_id_; }
	bool hasSelection() const { return !selected_id_.empty(); }

	bool focusRect(FocusRect& output, float padding = 5.0f) const {
		if (!std::isfinite(padding) || padding < 0.0f) return false;
		for (const MenuItemLayout& item : items_) {
			if (item.id == selected_id_ && item.visible && item.enabled) {
				output = {item.x - padding, item.y - padding,
					item.width + padding * 2.0f, item.height + padding * 2.0f};
				return true;
			}
		}
		return false;
	}

private:
	std::vector<MenuItemLayout> items_;
	std::string selected_id_;

	bool isSelectable(const std::string& id) const {
		if (id.empty()) return false;
		for (const MenuItemLayout& item : items_)
			if (item.id == id) return item.visible && item.enabled;
		return false;
	}

	void selectFirst() {
		selected_id_.clear();
		for (const MenuItemLayout& item : items_) {
			if (item.visible && item.enabled) {
				selected_id_ = item.id;
				return;
			}
		}
	}

	void move(float dx, float dy) {
		const MenuItemLayout* current = nullptr;
		for (const MenuItemLayout& item : items_)
			if (item.id == selected_id_ && item.visible && item.enabled) { current = &item; break; }
		if (!current) { selectFirst(); return; }
		const float cx = current->x + current->width * 0.5f;
		const float cy = current->y + current->height * 0.5f;
		const MenuItemLayout* best = nullptr;
		float best_score = 0.0f;
		for (const MenuItemLayout& item : items_) {
			if (!item.visible || !item.enabled || item.id == selected_id_) continue;
			const float tx = item.x + item.width * 0.5f;
			const float ty = item.y + item.height * 0.5f;
			const float vx = tx - cx, vy = ty - cy;
			const float primary = vx * dx + vy * dy;
			if (primary <= 0.001f) continue;
			const float perpendicular = std::fabs(vx * dy - vy * dx);
			const float score = primary + perpendicular * 2.5f;
			if (!best || score < best_score || (score == best_score && item.id < best->id)) {
				best = &item;
				best_score = score;
			}
		}
		if (best) selected_id_ = best->id;
	}
};

} // namespace fw
